/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef _BYTE_BUFFER_H
#define _BYTE_BUFFER_H
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <vector>
//#include "Logger.h"

//not thread safe like this...
//NOTE TODO for thread safety, put this inside the bytebuffer class
//if you do that, you can just use a scratch array of size[capacity]
static std::vector<char> copyVector; //basically just using vector resize/reserve/capacity functionality
///this is not optimized for very large arrays, the clear before function is using a
///static copy vector that will resize as it needs,
///so a large array can potentially resize the static copy vector to the same size
///only the copy buffer is resized, the ByteBuffer's array is only allocated once in constructor.

///a stream of bytes that are written incrementally, and cleared before or after a given index.
class ByteBuffer
{
protected:
    char *data;
    size_t capacity;  //total size of data[]
    //size_t size;      //size of written data NOTE this was just for readability?
    size_t position;  //current write position
    
private:
    //disabled...
    ByteBuffer(const ByteBuffer &) {}
    ByteBuffer & operator = (const ByteBuffer &){}
    ByteBuffer(){} //no default constructor!  use initializer list
public:

    ///returns current size
    inline size_t getSize() const { return position; }
    ///returns maximum size
    inline size_t getCapacity() const { return capacity; }
    ///access bytes!
    inline const char *getDataPtr() const { return data; }
    ///allocation size of the array
    ByteBuffer(size_t _capacity)
    {
        capacity = _capacity;
        //size = 0;
        position = 0;
        data = new char[capacity];
        if (copyVector.size() == 0) //hasnt even been used, set the reserve
            copyVector.reserve(8192); //8kb reserved for static copy buffer (adjust for usage needs?)
    }
    ~ByteBuffer() { delete[] data; }

    ///write data to the buffer, and advance the position.
    bool Write(char *_data, size_t count)
    {
        if (count == 0) return true;
        if (position + count >= capacity)
            return false;

        memcpy(data + position, _data, count);
        position += count;
        //size += count;
        return true;
    }

    ///clears the whole buffer.
    void Clear()
    {
        memset(data, 0, capacity);
        //size = 0;
        position = 0;
    }
    //size = 9
    //pos = 8
    //                         P  S
    //[0][1][2][3][4][5][6][7][8]   ignore this
    ///clears buffer after position
    void ClearAfter(size_t pos)
    {

        pos += 1; //after pos
        if (pos >= position)
            return;

        int cleared = (position) - pos; //transform size to a 0 based index
        //if (cleared < 0) return; //this should never happen, but just in case
        memset(&data[pos], 0, cleared);
        //size -= cleared;
        position -= cleared;
        
    }

    //size = 9
    //pos = 8
    //                         P  S
    //[0][1][2][3][4][5][6][7][8]   ignore this
    ///clear buffer before position
    void ClearBefore(size_t pos)
    {
        if (pos == 0)
            return;

        if (pos >= position)
        {
            Clear();
            return;
        }
        if (pos >= copyVector.capacity())
            copyVector.resize(pos + 1024); // add a little extra
            
        int preserved = position - pos;

        //LOGOUTPUT << "ClearBefore(): pos = " << pos << "   size = " << position;
        //LogInfo();
        //moves everything at pos to a static copy buffer, then to data[0]
        
        //copy into copyvector
        memcpy(&copyVector[0], &data[pos], preserved);
        //memset(data, 0, size);
        memcpy(data, &copyVector[0], preserved);
        //size = preserved;
        position = preserved;
        //note, copy vector has temporary data all over it
    }

    ///debug print
//     void PrintBytes()
//     {
//         LOGOUTPUT << "buffer( " << position << ") [";
//         for (int i = 0; i < position; i++)
//         {
//             LOGOUTPUT << (int)*(data+i) << "|";
//         }
//         LOGOUTPUT << "]";
//         LogInfo();
//     }
  
};

#endif
