/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __UNIQUEID_H__
#define __UNIQUEID_H__
#include <vector>

///as long as all the id's are returned properly this is a nice fast ID generator
///no verification is made inside ReturnID, if you provide it a random ID then the
///next id you get will be that random one off the stack, 
//TODO -- i suppose we should probably be checking if id is in use before returning.

///unique ID factory, must return ID's when they are no longer needed
class UniqueID
{
private:
    unsigned int maximum; //the highest possible ID
    std::vector<unsigned int> available;
public:

    UniqueID() { maximum = 1; }
    ///returns the first ID on the stack, or returns the next possible id (maximum)
    unsigned int GetID()
    {
        //if (maximum == 0) { you wrapped and now the whole thing is screwed. }
        if (available.size())
        {
            unsigned int r = available.back();
            available.pop_back();
            return r;
        }
        else
            return maximum++; //if this wraps around, the whole damn thing will break. use a 64bit type instead...
    }

    ///return an id, no verification is made, it assumes you arent just throwing random numbers at it.
    ///so only return id's you received from GetID, or enjoy some undefined behavior.
    void ReturnID(unsigned int id)
    {
        //if you want to verify, i reckon we aught to use a binary tree or somethin for that before we push_back.
        available.push_back(id);
    }
};

#endif
