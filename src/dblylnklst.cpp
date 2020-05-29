#include <iostream>
using namespace std;

class DblyLnkLst;

class Node
{
    explicit Node(int val) 
    : _data(val), next(nullptr), prev(nullptr)
    {}

    int _data;
    Node *next;
    Node *prev;
    friend class DblyLnkLst;
};

class DblyLnkLst
{
    public:
    DblyLnkLst()
    : lpHead(nullptr), lpTail(nullptr)
    {}

    ~DblyLnkLst()
    {
        if(nullptr != lpHead)
        {
            for(Node *lpPrev = nullptr, *lpNext = lpHead;
            nullptr != lpNext;){
                lpPrev = lpNext;
                lpNext = lpNext->next;
                delete lpPrev;
            }
        }
    }

    void Insert(int data, bool bAtBack = true)
    {
        Node *lpNode = nullptr;
        
        if(nullptr != (lpNode = new Node(data)))
        {
            if(nullptr == this->lpHead && nullptr == this->lpTail)
                this->lpHead = this->lpTail = lpNode;
            else if(bAtBack)
            {
                lpNode->prev = lpTail;
                lpTail->next = lpNode;
                lpTail = lpNode;
            }
            else
            {
                lpNode->next = lpHead;
                lpHead->prev = lpNode;
                lpHead = lpNode;
            }
            
        }
    }

    void Display(bool bForward = true)
    {
        if(bForward)
        {
            cout << "Forward iteration result .." << endl;
            for(Node *lpNext = lpHead; nullptr != lpNext; lpNext = lpNext->next)
                cout << lpNext->_data << " <=> ";
            cout << endl;
        }
        else
        {
            cout << "Reverse iteration result .." << endl;
            for(Node *lpPrev = lpTail; nullptr != lpPrev; lpPrev = lpPrev->prev)
                cout << lpPrev->_data << " <=> ";
            cout << endl;
        }        
    }

    private:
    Node *lpHead;
    Node *lpTail;
};

int main()
{
    DblyLnkLst dll;
    dll.Insert(10, false);
    dll.Insert(1445, false);
    dll.Insert(11112);
    dll.Insert(1345);
    dll.Insert(10098);
    dll.Insert(1367);
    dll.Insert(126);
    dll.Insert(198);
    dll.Insert(156);
    dll.Insert(124, false);
    dll.Insert(1001);
    dll.Insert(15);
    dll.Insert(12);
    dll.Insert(102);
    dll.Display();
    dll.Display(false);

    return 0;
}