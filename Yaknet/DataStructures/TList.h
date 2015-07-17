/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __TLIST_H__
#define __TLIST_H__
#include <stddef.h>
#include <stdlib.h>


///a basic doubly linked list. implements slow (brute force) access for GetAt and PopAt
template<typename Type>
class TList
{
public:
    class iterator;
    
    struct Node
    {
        Type item;
        Node *next;
        Node *prev;
    };

    size_t count;
    Node *front;
    Node *end;

    TList() { front = 0; end = 0; count = 0; }
    ~TList() { Clear(); }

    inline size_t size() { return count; }

    //add new node to end of list
    void PushBack(Type t)
    {
        if (front != 0)
        {
            //add new node after current end
            Node *p = end;
            end->next = new Node;
            end = end->next;
            end->item = t;
            end->next = 0; //end of list
            end->prev = p;
        }
        else //first push case
        {
            front = new Node;
            front->item = t;
            front->next = 0;
            front->prev = 0;
            end = front;
        }
        count++;
    }

    //add new node to front of list
    void PushFront(Type t)
    {
        if (front != 0)
        {
            Node *n = new Node;
            n->next = front;
            n->prev = 0;
            front->prev = n;
            n->item = t;
            front = n;
        }
        else //first push case
        {
            front = new Node;
            front->item = t;
            front->next = 0;
            front->prev = 0;
            end = front;
        }
        count++;
    }

    //pop the front of the list off
    void PopFront()
    {
        if (front != 0)
        {
            Node *tmp = front;
            if (front != end)
            {
                front = front->next;
                front->prev = 0; //front of list, no previous
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

    //pop the back of the list off
    void PopBack()
    {
        if (end != 0)
        {
            Node *tmp = end;
            if (front != end)
            {
                end = end->prev;
                end->next = 0; //new end, clear its next
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

    //returns the front of the list
    Type Front()
    {
        if (front != 0)
            return front->item;
        else abort(); //i decided this should just crash and burn if you try to access empty list
    }

    //returns the back of the list
    Type Back()
    {
        if (end != 0)
            return end->item;
        else abort();
    }

    //seeks to index n and returns item
    Type GetAt(size_t n)
    {
        if (n+1 > count)
            abort();// don't try to access out of bounds elements ;)
        Node *node = front;
        if (n == 0)
            return node->item;
        for (size_t i = 0; i < n; i++)
            node = node->next; //seek next by n-1 nodes

        return node->item;
    }

    //removes at the iterators location and advances to the next node
    //for example if it was at front it would stay at front, but at end it would be invalid after pop
    //call previous to go back one, for normal for loop type behavior
    void PopAt(TList<Type>::iterator &iter)
    {
        if (iter.past_end())
            return;

        if (iter.location == front)
        {
            PopFront();
            iter.location = front;
        }
        else if (iter.location == end)
        {
            PopBack();
            iter.location = 0; //invalid, after end
        }
        else //not front, or back  next/prev should be valid
        {
            Node *node = iter.location;
            node->prev->next = node->next;
            node->next->prev = node->prev;
            iter.location    = node->next;
            delete node;
            count--;
        }
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
        count = 0;
        end = 0;
    }


    
    ///////////////////////////
    //     iterator
    ///////////////////////////
    class iterator
    {
        friend class TList;
    private:
        typename TList<Type>::Node *location;
        TList<Type> *list;
        iterator();

    public:
        iterator(TList<Type> &_list)
        {
            list = &_list;
            location = list->front;
        }

        //sets the iterator back to the front
        inline void rewind()
        {
            location = list->front;
        }

        //returns true if the location is past the last node
        inline bool past_end()
        {
            return (location == 0) ? true : false;
        }
        
        inline bool invalid()
        {
            return (location == 0) ? true : false; 
        }

        //advances iterator to next (stops after end)
        void next()
        {
            if (location)
                location = location->next;
        }

        //reverses iterator to last node (invalid if you go before front)
        void previous()
        {
            if (location == 0)
            {
                location = list->end;
                return;
            }
            if (location != list->front)
                location = location->prev;
            else 
                location = 0;
        }

        //returns the current nodes item
        inline Type &item()
        {
            if (location)
                return location->item;
            else
                abort();//this is going to crash, but you shouldnt be going out of bounds...
        }
    };

    
};

#endif
