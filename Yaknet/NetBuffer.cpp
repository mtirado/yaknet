/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

//TODO SERIOUSLY, F'n TEST ALL THESE FUNCTIONS

#include "NetBuffer.h"
#include "Yaknet/Utilities/Logger.h"

NetBuffer::NetBuffer(size_t _capacity, bool _packetized)
{
    packetized = _packetized;
    capacity = _capacity;
    if (packetized) capacity += 4; //always add 4 bytes for size/eventId
    buffer.reserve(capacity);
    memset(buffer.data(), 0, capacity);
    //first 2 bytes is size!! call Prepare() to fill it out before sending!!!
    //next 2 bytes is msg id, send function should fill that out
    //if you need more than 2 bytes, you should probably be using a tcp stream

    //TODO you should get rid of this whole concept

    if (packetized)
        SetPosition(sizeof(short) + sizeof(short));
    else
        SetPosition(0);

}

NetBuffer::~NetBuffer()
{
}

//sets the curPos to the byte passed in  0 - capacity-1
void NetBuffer::SetPosition(unsigned int pos)
{
    if (pos >= capacity)
        curPos = (buffer.data() + capacity-1);  // never go out of bounds...
    else
        curPos = (buffer.data() + pos);

}

char NetBuffer::ReadByte()
{
    if (curPos+sizeof(char) <= GetEnd())
        curPos += sizeof(char);
    else 
        curPos = GetEnd()-sizeof(char);
    return *( (char *)(curPos - sizeof(char)) );
}

short NetBuffer::ReadShort()
{
    if (curPos+sizeof(short) <= GetEnd())
        curPos += sizeof(short);
    else 
        curPos = GetEnd()-sizeof(short);
    return *( (short *)(curPos - sizeof(short)) );
}

int NetBuffer::ReadInt()
{
    if (curPos+sizeof(int) <= GetEnd())
        curPos += sizeof(int);
    else 
        curPos = GetEnd()-sizeof(int);
    return *( (int *)(curPos - sizeof(int)) );
}

float NetBuffer::ReadFloat()
{
    if (curPos+sizeof(float) <= GetEnd())
        curPos += sizeof(float);
    else 
        curPos = GetEnd()-sizeof(float);
    //memcpy(&ret, curPos - sizeof(float), sizeof(float));
    //wtf this doesnt work for floats? WHY!?!?!?!?
    return *( (float *)(curPos - sizeof(float)) );
}

void NetBuffer::WriteByte(const char c)
{
    if (GetWritePosition() + sizeof(char) > capacity)
    {
        LogWarning("NetBuffer::WriteByte() trying to write out of bounds!");
        return;
    }
    memcpy(curPos, &c, sizeof(char));
    curPos += sizeof(char);
}

void NetBuffer::WriteBytes(const char* data, unsigned int size)
{
    if (GetWritePosition() + size > capacity)
    {
        LogWarning("NetBuffer::WriteBytes() trying to write out of bounds!");
        return;
    }

    memcpy(curPos, data, size);
    curPos += size;
}

char* NetBuffer::ReadBytes(unsigned int size)
{
    if (curPos + size > GetEnd())
    {
        LogError("NetBuffer::ReadBytes() trying to read more bytes than it has!, returning null");
        return 0;
    }
     //LOGOUTPUT << "cpos: " << (unsigned int)(curPos - buffer.data()) << "\nsize: " << size;
    
    char *ret = curPos;
    //char *ret = GetBuffer() + 8;//(curPos - GetBuffer());
    //LOGOUTPUT << "\nret: " << (unsigned int)(ret - buffer.data());
    curPos += size;
    //LogInfo();
    return ret;
}

void NetBuffer::WriteShort(const short val)
{
    if (GetWritePosition() + sizeof(short) > capacity)
    {
        LogWarning("NetBuffer::WriteShort() trying to write out of bounds!");
        return;
    }

    memcpy(curPos, &val, sizeof(short));
    curPos += sizeof(short);
}

void NetBuffer::WriteInt(const int val)
{
    if (GetWritePosition() + sizeof(int) > capacity)
    {
        LogWarning("NetBuffer::WriteInt() trying to write out of bounds!");
        return;
    }

    memcpy(curPos, &val, sizeof(int));
    curPos += sizeof(int);
}

void NetBuffer::WriteFloat(const float val)
{
    if (GetWritePosition() + sizeof(float) > capacity)
    {
        LogWarning("NetBuffer::WriteFloat() trying to write out of bounds!");
        return;
    }

    memcpy(curPos, &val, sizeof(float));
    curPos += sizeof(float);
}

//honestly this is probably really insecure, use write bytes with strlen before the data, no null terminator
void NetBuffer::WriteString(const char  *str)
{
    unsigned int len = (unsigned int)strlen(str) + 1;

    if (GetWritePosition() + len > capacity)
    {
        LogWarning("NetBuffer::WriteString() trying to write out of bounds!");
        len = capacity - GetWritePosition();
        //memcpy(curPos, str, (capacity - GetWritePosition()) );
        //curPos += (capacity - GetWritePosition());
        //return;
    }

    memcpy(curPos, str, len);
    curPos += len;
}

//read a string from the buffer, updates internal position
char *NetBuffer::ReadString()
{
    unsigned int bytesLeft = capacity - GetWritePosition();
    //length of string
    unsigned int len = 0;

    //check for terminator first
    char c = curPos[len];

    //get length by looping untill we hit the null terminator
    while (c)
    {
        c = curPos[len];
        len++;
        if (len == bytesLeft)
            break;
    }
    if (len == 0)
    {
        char *newStr = new char[5];
        newStr[0] = 'n';newStr[1] = 'u';newStr[2] = 'l';newStr[3] = 'l';
        newStr[4] = 0;
        return newStr;
    }
    if (len > bytesLeft)
        len = bytesLeft;

    char *newStr = new char[len + 1];
    memcpy(newStr, curPos, len);
    newStr[len] = 0; //added terminator
    curPos += len + 1; //move curpos to end of string plus the terminator

    return newStr;
}


void NetBuffer::Prepare() 
{
    *((unsigned short *)buffer.data()) = (unsigned short)GetWritePosition();
}

///returns pointer to the beginning of internal buffer.
char *NetBuffer::GetBuffer()
{
    return buffer.data();
}

///address directly after the buffer in memory (buffer.data() + capacity).
char *NetBuffer::GetEnd()
{
    return (char *)(buffer.data() + capacity);
}

int NetBuffer::GetSize() { return capacity; }


///fills the buffer starting at startPos, bytes is number of bytes and
///resets position to 0.
void NetBuffer::FillBuffer(const char *data, size_t bytes, size_t startPos)
{
    if (bytes+startPos > capacity)
    {
        LogWarning("NetBuffer::FillBuffer() - truncated, size exceeeded maximum defined size");
        bytes = capacity-startPos;
    }
    memcpy(buffer.data() + startPos, data, bytes);
    curPos = buffer.data() + startPos + bytes;
}

///clear the buffer, and reset position by default
void NetBuffer::ClearBuffer(bool resetPosition)
{
    memset(buffer.data(), 0, buffer.capacity());
    if (resetPosition)
        SetPosition( packetized ? sizeof(short)+sizeof(short) : 0 );
}

///debug print
void NetBuffer::PrintBytes()
{
    LOGOUTPUT << "buffer(" << GetSize() << ") [";
    for (int i = 0; i < GetSize(); i++)
    {
        LOGOUTPUT << (int)*((char *)buffer.data()+i) << " | ";
        //LOGOUTPUT << *((char *)buffer.data()+i);
    }
    LOGOUTPUT << "]";
    LogInfo();
}

///returns the amount of data in the buffer.\n
///NOTE: if curPos is not on after the last written element, size will be incorrect.
///to avoid this, do not use setposition.
///returns curPos - buffer.data()
size_t NetBuffer::GetWritePosition()
{
    return (size_t)(curPos - buffer.data());
}

size_t NetBuffer::GetBytesLeft()
{
    return (size_t)(GetEnd() - curPos);
}
