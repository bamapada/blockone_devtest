#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <bitset>
#include <cstdio>
#include <limits>
#include <vector>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
using namespace std;

struct account_balance {
   int    account_id; ///< the name of the account
   int    balance; ///< the balance of the account
};

struct transfer {
   int from;    ///< the account to transfer from
   int to;      ///< the account to transfer to
   int amount;  ///< the amount to transfer
};

using transaction = vector<transfer>;

// Helper object for sorting (ascending) and find function
struct compare_accid
{ 
    bool operator()(const account_balance& value, const account_balance& key) 
    { 
        return (key.account_id > value.account_id); 
    }
    
   friend bool operator== (const account_balance& lhs, const account_balance& rhs);
};

bool operator== (const account_balance& lhs, const account_balance& rhs) 
{ 
    return (lhs.account_id == rhs.account_id)? true : false;
}

struct IDisposible
{
    // Helper for self cleaning when object is no longer required
    virtual void Dispose() = 0;
};

struct ITransactions : IDisposible
{
    // push a transaction to the database
    virtual void push_transaction(const transaction& t) = 0;

    // settle the database such that the invariant is maintained 
    // and the best state of the database is present
    virtual void settle() = 0;

    // returns a list of all the balances in any order
    virtual vector<account_balance> get_balances() const = 0;

    // returns the 0-based indices of the surviving transactions in that sequence which, when 
    // applied to the initial balances produce a state where the invariant is maintained
    virtual vector<size_t> get_applied_transactions() const = 0;
};

class MemDatabase;
class TrxInfo : private IDisposible
{
    private:
        TrxInfo(const transaction& t)
        : TrxId(0), IsPositive(true), trx(t)
        {}

        // Dispose when no longer requred
        void Dispose() { delete this; }

    private:
        size_t TrxId;
        bool IsPositive;
        const transaction& trx;
        vector<account_balance> post_trx_acc_state;
    friend class MemDatabase;
};

class MemDatabase : public ITransactions
{
    public:
    explicit MemDatabase(const vector<account_balance>& initial_balances)
    {
        // Verify initial data for the following unhappy conditions
        // 1. Receiving data may contain duplicated account id
        // 2. Initial account balance may be negative
        // Take the help of ordered set to identify duplicate account id
        set<int> acc_ids;
        for(auto bal : initial_balances)
        {
            if((bal.balance >= 0) && (acc_ids.find(bal.account_id) == acc_ids.end()))
            {
                this->init_acc_bal.push_back(bal);
                acc_ids.insert(bal.account_id);
            }
        }

        // Sort the vector, so that we can apply binary search to identify on account details
        if(!this->init_acc_bal.empty())
            std::sort(this->init_acc_bal.begin(), this->init_acc_bal.end(), compare_accid());
    }

    void push_transaction(const transaction& t)
    {
        // Validate details with this transfer before we pushing it to the transaction queue 
        if(this->VerifyTransferDetails(t))
        {
            TrxInfo* lpTrx = new TrxInfo(t);
            // If there exist any previous transfer
            // 1. Assign next trasaction id
            // 2. Copy the last intermediate state                
            if(!this->trx_queue.empty())
            {
                TrxInfo*& lpLastTrx = *(this->trx_queue.rbegin());                
                lpTrx->TrxId = 1 + lpLastTrx->TrxId;
                lpTrx->post_trx_acc_state = lpLastTrx->post_trx_acc_state;
            }
            // Now execute this transaction
            // Iterate all transfer object attached to this transaction and
            // apply them to create an intermediate state register, adjust from and to account.
            for(auto& xfer : lpTrx->trx)
            {                
                xfer_balance(xfer.to, xfer.amount, lpTrx);
                xfer_balance(xfer.from, xfer.amount, lpTrx, false);                
            }

            // Have we been attain any negative balance?
            for(auto& lpAcc : lpTrx->post_trx_acc_state)
                if(lpAcc.balance < 0)
                {
                    lpTrx->IsPositive = false;
                    break;
                }
            // Push this transfer in the queue
            this->trx_queue.push_back(lpTrx); 
        }
    }

    void settle()
    {
        // If there exist no transaction, then no settlement is required
        if(this->trx_queue.empty())
            return;
        
        // Iterate through all the transaction we made so far.
        // 1. Find the last good transfer that created all account positive result
        // 2. Capture the indices of all of them and store it for future use
        bool bCommit = false;
        stack<size_t> applied_trx_stack;
        for(auto it = this->trx_queue.rbegin(); it != this->trx_queue.rend(); ++it)
        {
            if(!bCommit && (*it)->IsPositive)
            {
                // Commit all the intermediate state account to initial account details
                for(auto imAccInfo : (*it)->post_trx_acc_state)
                {
                    account_balance key = {imAccInfo.account_id, 0};
                    auto orgAccInfo = find(this->init_acc_bal.begin(), this->init_acc_bal.end(), key);
                    orgAccInfo->balance = imAccInfo.balance;
                }
                bCommit = true;
                this->applied_indices.clear();
            }

            // Are we able to commit the intermediate transaction successfully,
            // then erase all previous successful indices.
            if(bCommit)
                applied_trx_stack.push((*it)->TrxId);
        }

        // Store the indices of applied transfer in the final container 
        while(!applied_trx_stack.empty())
        {
            this->applied_indices.push_back(applied_trx_stack.top());
            applied_trx_stack.pop();
        }

        for(auto lpTrx : this->trx_queue)
            lpTrx->Dispose();
        this->trx_queue.clear();
    }

    // Returns the current balances of the registered accounts.
    vector<account_balance> get_balances() const
    {
        return this->init_acc_bal;
    }

    // Returns indices (zero based) of the applied transfer
    vector<size_t> get_applied_transactions() const
    {
        return this->applied_indices;
    }

    // Clean the heap when we no longer required
    void Dispose() { delete this; }

    private:
        // Validate each transfer details prior to compute the actuals
        // 1. Origin and destination account should have different name and no self reference
        // 2. Amount should be posted as positive only integer
        // 3. Both, Origin and destination account should be originally exist in the initial account register
        // 4. If above all the point satisfied, then will push to the transaction to create intermediate state
        bool VerifyTransferDetails(const transaction& trx) const
        {
            bool bValid = false;

            if(!trx.empty())
            {
                bValid = true;
                for(auto& each_tr : trx)
                {
                    if((bValid = (each_tr.from != each_tr.to && each_tr.amount > 0)))
                    {
                        account_balance key_from{each_tr.from, 0};
                        account_balance key_to{each_tr.to, 0};
                        auto from = binary_search(
                            this->init_acc_bal.begin(),
                            this->init_acc_bal.end(), 
                            key_from, 
                            compare_accid());
                        auto to = binary_search(
                            this->init_acc_bal.begin(),
                            this->init_acc_bal.end(),
                            key_to,
                            compare_accid());
                        bValid = (from && to);
                    }
                }
            }

            return bValid;
        }

        // Transfer balance from origin to destination account
        void xfer_balance(int id, int amount, TrxInfo*& lpTrx, bool bDeposite = true)
        {
            // In order to post the transaction
            // 1. First lookup the current state register, in case previous transfer already took place
            // 2. Otherwise search in the initial register  copy
            account_balance key = {id, 0};
            auto account = find(lpTrx->post_trx_acc_state.begin(), lpTrx->post_trx_acc_state.end(), key);
            if(account == lpTrx->post_trx_acc_state.end())
            {
                account = find(this->init_acc_bal.begin(), this->init_acc_bal.end(), key);
                auto final_bal = bDeposite? (account->balance + amount) : (account->balance - amount);
                lpTrx->post_trx_acc_state.push_back({account->account_id, final_bal});
            }
            else if(bDeposite)
                account->balance += amount;
            else
                account->balance -= amount;
        }
        
    private:
        vector<account_balance> init_acc_bal;
        vector<TrxInfo*> trx_queue;
        vector<size_t> applied_indices;
};

auto create_database(const vector<account_balance>& initial_balances ) 
{
   return static_cast<ITransactions*>(new MemDatabase(initial_balances));
}

///////////////////////////////////////////////
void TestCase_1()
{
    vector<account_balance> acc
    {
        {1, 5}, 
        {3, 15}, 
        {2, 10},
        {5, 10},
        {4, -10},
        {7, 10},
        {7, 40},
        {6, 20},
        {6, 10}
    };

    vector<transfer> xfer_1
    {
        {1, 2, 3},
        {3, 1, 2}
    };

    vector<transfer> xfer_2
    {
        {1, 7, 3},
        {3, 1, 2}
    };

    vector<transfer> xfer_3
    {
        {1, 2, 3},
        {3, 1, -2}
    };

    vector<transfer> xfer_4
    {
        {1, 2, 3},
        {20, 1, -2}
    };

    ITransactions* lpTrx = create_database(acc);
    lpTrx->push_transaction(xfer_1);
    lpTrx->push_transaction(xfer_2);
    lpTrx->push_transaction(xfer_3);
    lpTrx->push_transaction(xfer_4);
    lpTrx->Dispose();
    /*db.push_transaction(xfer);
    vector<account_balance> x{db.get_balances()};
    for(auto a : x)
        cout << a.account_id << "/" << a.balance << endl;
    */
}

void TestCase_Single_Successful()
{
    vector<account_balance> acc
    {
        {1, 5},        
        {2, 10},
        {3, 15}
    };

    vector<transfer> xfer
    {
        {1, 2, 3},
        {3, 1, 2}
    };

    ITransactions* lpTrx = create_database(acc);
    lpTrx->push_transaction(xfer);
    lpTrx->settle();
    // List all the balances
    cout << "Single Successful Transaction" << endl;
    cout << "List of all the balances ..." << endl;
    for(auto accInfo : lpTrx->get_balances())
        cout << accInfo.account_id << "  " << accInfo.balance << endl;
    cout << "Applied transfer order ..." << endl << "| ";
    for(auto indx : lpTrx->get_applied_transactions())
        cout << indx << " | ";
    cout << endl;
    lpTrx->Dispose();
}

void TestCase_Multiple_Successful()
{
    vector<account_balance> acc
    {
        {1, 5},        
        {2, 10},
        {3, 15}
    };

    vector<transfer> xfer_1
    {
        {1, 2, 3},
        {3, 1, 2}
    };
    vector<transfer> xfer_2
    {
        {2, 1, 11}
    };

    ITransactions* lpTrx = create_database(acc);
    lpTrx->push_transaction(xfer_1);
    lpTrx->push_transaction(xfer_2);
    lpTrx->settle();
    // List all the balances
    cout << "Multiple Successful Transaction" << endl;
    cout << "List of all the balances ..." << endl;
    for(auto accInfo : lpTrx->get_balances())
        cout << accInfo.account_id << "  " << accInfo.balance << endl;
    cout << "Applied transfer order ..." << endl << "| ";
    for(auto indx : lpTrx->get_applied_transactions())
        cout << indx << " | ";
    cout << endl;
    lpTrx->Dispose();
}

void TestCase_Single_Failing()
{
    vector<account_balance> acc
    {
        {1, 5},        
        {2, 10},
        {3, 15}
    };

    vector<transfer> xfer
    {
        {2, 1, 11}
    };

    ITransactions* lpTrx = create_database(acc);
    lpTrx->push_transaction(xfer);
    lpTrx->settle();

    cout << "Single Failing Transaction" << endl;
    // List all the balances
    cout << "List of all the balances ..." << endl;
    for(auto accInfo : lpTrx->get_balances())
        cout << accInfo.account_id << "  " << accInfo.balance << endl;
    cout << "Applied transfer order ..." << endl << "| ";
    for(auto indx : lpTrx->get_applied_transactions())
        cout << indx << " | ";
    cout << endl;
    lpTrx->Dispose();
}

void TestCase_Multiple_Restore_Consistency()
{
    vector<account_balance> acc
    {
        {1, 5},        
        {2, 10},
        {3, 15}
    };

    vector<transfer> xfer_1
    {
        {2, 1, 11}
    };

    vector<transfer> xfer_2
    {
        {1, 2, 3},
        {3, 1, 2}
    };

    ITransactions* lpTrx = create_database(acc);
    lpTrx->push_transaction(xfer_1);
    lpTrx->push_transaction(xfer_2);
    lpTrx->settle();
    // List all the balances
    cout << "Multiple restore consistency" << endl;
    cout << "List of all the balances ..." << endl;
    for(auto accInfo : lpTrx->get_balances())
        cout << accInfo.account_id << "  " << accInfo.balance << endl;
    cout << "Applied transfer order ..." << endl << "| ";
    for(auto indx : lpTrx->get_applied_transactions())
        cout << indx << " | ";
    cout << endl;
    lpTrx->Dispose();
}

int main()
{
    TestCase_Single_Successful();
    TestCase_Multiple_Successful();
    TestCase_Single_Failing();
    TestCase_Multiple_Restore_Consistency();
    //TestCase_1();
    return 0;
}