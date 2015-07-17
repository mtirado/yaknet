/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef _TQUEUE_H_
#define _TQUEUE_H_
#include <stddef.h>
#include <stdlib.h>
///just a basic queue using linked list.
template<typename Type>
class TQueue
{
private:
    struct Node
    {
        Type item;
        Node *next;
    };

    size_t count;
    Node *front;
    Node *end;

public:
    TQueue() { front = 0; end = 0; count = 0; }
    ~TQueue() { Clear(); }

    inline size_t size() { return count; }
    
    //add new node to end of queue
    void PushBack(Type t)
    {
        if (front != 0)
        {
            //add new node after current end
            end->next = new Node;
            end = end->next;
            end->item = t;
            end->next = 0; //end queue
        }
        else //first push case
        {
            front = new Node;
            front->item = t;
            front->next = 0;
            end = front;
        }
        count++;
    }

    //pop the front of the queue off
    void PopFront()
    {
        if (front != 0)
        {
            Node *tmp = front;
            if (front != end)
            {
                front = front->next;
                count--;
            }
            else //was the only item
            {
                front = 0;
                end = 0;
                count = 0;
            }
            delete tmp;
        }
    }

    //returns the front of the queue
    Type Front()
    {
        if (front != 0)
            return front->item;
        else abort(); //newly instituted crash and burn policy
    }

    //note: this only deletes nodes, not the contained data
    void Clear()
    {
        Node *current = front;
        Node *junk;
        while (current)
        {
            junk = current;
            current = current->next;
            delete junk;
        }
        front = 0;
        end = 0;
        count = 0;
    }
};

#endif
