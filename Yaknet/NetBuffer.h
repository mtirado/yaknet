/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef _NET_BUFFER_H_
#define _NET_BUFFER_H_

#include <memory.h>
#include <vector>
//#include "Logger.h"


/**
 * by default this buffer reserves the first 4 bytes for packet size, and eventID, so do not write over these.
 * i recommend you write to it in a linear fashion (without calling setposition) so you can call Prepare() to
 * fill out the size before sending. alternatively you can manually set the first 2 bytes before sending.
 * the connections send method should fill out eventID for you.\n\n
 * if using a connection type that does not require header data (TCPStream) you must pass a false
 * value to the constructor's _packetized parameter, or else it will still reserve the 4 bytes for header data.\n\n
 * calling any of the read or write functions will advance the current position pointer to the next element.
 * for reading / writing bytes there is no size written, it is up to you to handle that.
 */

///A byte buffer with functions to read and write specific types to be sent over the network.
class NetBuffer
{
private:
    ///reserve space for size and eventID ?
    bool packetized;
    ///maximum size.
    size_t capacity;
    std::vector<char> buffer;
    ///current read/write address.
    char *curPos;

    //can not copy this.
    NetBuffer(const NetBuffer &) {}
    NetBuffer & operator = (const NetBuffer &){}
    NetBuffer(){} //no default constructor!  use initializer list
public:
    ///if packetized, 4 bytes for header data will be allocated and reserved at the beginning.
    ///the position will start writing at index 3.
    NetBuffer(size_t _capacity, bool _packetized = true);
    ~NetBuffer();

    ///returns a copy of the string read (MAKE SURE YOU DELETE IT!!!!)
    char *ReadString();

    ///sets the curPos to the byte index passed in  0 - capacity-1
    ///NOTE size if based off of current position, be careful when using this
    void SetPosition(unsigned int pos);

    char  ReadByte();
    short ReadShort();
    int   ReadInt();
    float ReadFloat();
    void  WriteByte(const char c);
    void  WriteShort(const short val);
    void  WriteInt(const int val);
    void  WriteFloat(const float val);
    ///honestly you shouldnt use this function, use a length variable before string and writebytes,
    ///relying on null terminators will lead to exploits.
    ///TODO rip these string functions out.
    void  WriteString(const char  *str);
    ///you must write the size before this if you need to know it on receiving end.
    ///this assumes you know exactly how many bytes are in the packet
    void  WriteBytes(const char *data, unsigned int size);
    char *ReadBytes(unsigned int size);
    ///sets the first 2 bytes to the assumed size, curPos - buffer.data().
    void Prepare();

    ///returns pointer to the beginning of internal buffer.
    char *GetBuffer();

    ///address directly after the buffer in memory (buffer.data() + capacity).
    char *GetEnd();

    int GetSize();


    ///fills the buffer starting at startPos, bytes is number of bytes and
    ///resets position to 0.
    void FillBuffer(const char *data, size_t bytes, size_t startPos = 0);

    ///clear the buffer, and reset position by default
    void ClearBuffer(bool resetPosition = true);

    ///debug print
    void PrintBytes();

    ///returns the amount of data in the buffer.\n
    ///NOTE: if curPos is not on after the last written element, size will be incorrect.
    ///to avoid this, do not use setposition.
    ///returns curPos - buffer.data()
    size_t GetWritePosition();
    
    ///returns the amount of bytes left in the buffer.
    size_t GetBytesLeft();
};



#endif
