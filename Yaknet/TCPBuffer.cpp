/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 


#ifndef _WIN32_
    #include <sys/socket.h> //recv
    #include <errno.h>
#endif

#include "Yaknet/DataStructures/ByteBuffer.h"
#include "Yaknet/DataStructures/TQueue.h"
#include "Yaknet/Utilities/StaticTimer.h"
#include "Yaknet/Utilities/Logger.h"
#include "Connection.h"
#include "WebSockets.h"
#include "TCPStream.h"
#include "NetBuffer.h"

//read into recv buffer, returns total bytes recvd'd
//returns bytes received, or an error number ( anegative errno value)
int TCPConnection::Recv(int socket, int flags)
{
    size_t count = buffer.getSize(); //bytes leftover
    int err = 0;
    //maxPacketLength is not the actual packet size, the idea is that more than one can be read at a time
    //in processing stage we will make sure if maxPacketLength is reached, we have a full packet. or else drop the entire stream(error)
    for (int i = 0; i < MAX_PACKETS_IN_BUFFER; i++) //read the max amount, untill theres nothing left!
    {
        
        int size = recv(socket, recvBuffer.data(), maxPacketLength, 0);

        if (size < 0) //there was an "error"
        {
            size = 0; //do not add -1 to an unsigned type that could be 0!
            err = errno;
            i = MAX_PACKETS_IN_BUFFER; // done recv'ing if we got an error
            //NOTE windows is going to need some WSA junk
            switch(errno)
            {
            
                case EWOULDBLOCK: //nothing to read, done looping
                    err = 0; //this is going to happen often with a nonblocking socket, dont treat as error
                   // LogInfo("got EWOULDBLOCK");
                break;
                case EBADF :
                    LogError("recv() - bad file descriptor");
                break;
                case ECONNRESET :
                    LogError("recv() - Connection reset");
                break;
                case EINTR :
                    LogError("recv() - Signal interrupt");
                break;
                case ENOTCONN :
                    LogError("recv() - Not connected");
                break;
                case ENOTSOCK :
                    LogError("recv() - Not a socket");
                break;
                case EOPNOTSUPP :
                    LogError("recv() - Not supported");
                break;
                case ETIMEDOUT :
                    LogError("recv() - Timed out");
                break;
                default :
                    LogError("recv() - error, other :]");
                break;
            }
            if (err != 0)
                err = -err; //the negative code
        }
        else if (size == 0)
        {
            err = -1;
            i = MAX_PACKETS_IN_BUFFER; //no more looping!
            LogError("recv() - gracefully disconnected todo: handle me!");
        }
       
        //copy 
        if (!buffer.Write(recvBuffer.data(), size))
            LogError("TCPBuffer full, dropping stream data, whole buffer will likely be dropped next. increase bufer size!!");
        else
            count += size;
    }

    //recv'd from the stream, now packetize and populate message queue.
    size_t bytesToProcess = count;
    size_t processed = 0; //avoid confusing maths

    if (count < sizeof(short))//just incase... :[
        return err;

    while (bytesToProcess)
    {
        unsigned short msgSize = *(unsigned short *)(buffer.getDataPtr() + processed);
        //this is very very bad...
        if (msgSize > maxPacketLength)
        {
            buffer.Clear();
            //going to axe this whole buffer now... RUN AWAY
            /*LOGOUTPUT << "msgSize: " << msgSize << "  | maxpacketlength: " << maxPacketLength;
            LogInfo();
            LogError("msgSize exceeded max packet length, dropping buffer, and hoping for the best...");
            //return err; 
            LogError("scratch that, dropping the connection");*/
            events->CallError(this, YAKERR_CORRUPTION);
            return count;
            //return ECONNRESET;
        }

        //dont have the whole packet yet, clear where we're at to reset buffer
        if (bytesToProcess < msgSize)
        {
            //still wondering if this is ever going to happen :)
            LOGOUTPUT << "clearbing before whole packet has been recvd - bytesto: " << 
                bytesToProcess << " | msgSize " << msgSize;
            LogInfo();
            buffer.ClearBefore(processed);
            return count;
        }

        //TODO netbuffer factory to avoid allocations plz??
        //actually that could get pretty complex, memory management + fragmentation and such... hmm,
        //deffly TODO this later....  maybe just force all netbuffers to one size?

        //we made it this far, so we must have at least one full packet
        NetBuffer *nb = new NetBuffer(msgSize);
        nb->FillBuffer(buffer.getDataPtr()+processed, msgSize);
        messageQueue.PushBack(nb);
        processed += msgSize;
        bytesToProcess -=msgSize;

    }

    buffer.ClearBefore(processed);
    return count;
} 

int WebSockConnection::Recv(int socket, int flags)
{
    size_t count = buffer.getSize(); //account for leftovers
    int err = 0;
    //maxPacketLength is not the actual packet size, so hopefully more than one will be read at a time
    //in processing stage we will make sure if maxPacketLengthis reached, we have a full packet. or else drop the entire stream(error)
    for (int i = 0; i < MAX_PACKETS_IN_BUFFER; i++) //read the max amount, untill theres nothing left!
    {
        int size = recv(socket, recvBuffer.data(), maxPacketLength, 0);
        if (size < 0) //there was an "error"
        {
            size = 0; //do not add -1 to an unsigned type that could be 0!
            err = errno;
            i = MAX_PACKETS_IN_BUFFER; // done recv'ing if we got an error
            //NOTE windows is going to need some WSA junk
            switch(errno)
            {
            
                case EWOULDBLOCK: //nothing to read, done looping
                    err = 0; //this is going to happen often with a nonblocking socket, dont treat as error
                   // LogInfo("got EWOULDBLOCK");
                break;
                case EBADF :
                    LogError("recv() - bad file descriptor");
                break;
                case ECONNRESET :
                    LogError("recv() - Connection reset");
                break;
                case EINTR :
                    LogError("recv() - Signal interrupt");
                break;
                case ENOTCONN :
                    LogError("recv() - Not connected");
                break;
                case ENOTSOCK :
                    LogError("recv() - Not a socket");
                break;
                case EOPNOTSUPP :
                    LogError("recv() - Not supported");
                break;
                case ETIMEDOUT :
                    LogError("recv() - Timed out");
                break;
                default :
                    LogError("recv() - error, other :]");
                break;
            }
            if (err != 0)
                err = -err;
        }
        else if (size == 0)
        {
            err = -1;
            //err = ECONNRESET;  //maybe should be a seperate message, to mark graceful / ugly dc's
            i = MAX_PACKETS_IN_BUFFER; //no more looping!
            LogError("recv() - gracefully disconnected todo: handle me!");
            return err; //there may be packets to process, still... but i'm returning anyway.
        }
       
        //copy 
        if (!buffer.Write(recvBuffer.data(), size))
            LogError("TCPBuffer full, dropping stream data, whole buffer will likely be dropped next. increase bufer size!!");
        else
            count += size;
    }

    size_t bytesToProcess = count;
    size_t processed = 0; //avoid confusing maths

    //websockets header is at LEAST 16bits + 32bit mask
    //if (count < 6)//just incase... :[
    //    return err; //TODO why did i disable this?

    //could potentially go out of bounds due to extra framing data
    while (bytesToProcess >= MIN_MSG_SIZE)
    {
        //LOGOUTPUT << "bytes to process: " << bytesToProcess;
        //LogInfo();

        unsigned short msgSize;
        short headerSize; //if theres extra size data beyond 7 bits
        //also TODO, if final fragment bit isnt set, then theres more packet to be read...
        //need to concatonate them somehow
       
        unsigned short header = ntohs(*(unsigned short *)(buffer.getDataPtr()+processed));

        bool finalFrag = WSFLAG_FIN(header) ? true : false;
        bool maskPresent = WSFLAG_MASKED(header) ? true : false;
        unsigned char length = WSFLAG_PAYLOADSIZE(header);        //126 means next 16 bits are length
        unsigned char opcode = WSFLAG_OPCODE(header);

        LOGOUTPUT <<  "HEADER: " << header << "\nOPCODE: " <<
        (unsigned int) opcode << "\nLENGTH: " << (unsigned int)length;
        LogInfo();

        //text frame, NOT supported.
        if (opcode == 0)
        {
            LogError("Got continuation frame, not supported yet.");
            return count;
            //return ECONNRESET;
        }
        

        if (finalFrag != 1)
        {
            LogWarning("NOT FINAL FRAGMENT!!!  you need to finish implementing this NOWWW");
            //return ECONNRESET;
        }
        if (maskPresent != 1)
        {
            //LogWarning("NO MASK PRESENT");
            //return ECONNRESET; //drop it
        }
        else if (opcode != 2)
        {
            //not a data frame
            LogError("not a binary data frame, dropping connection");
            return -1;
            //return ECONNRESET;
        }
        //parse header and find the actual packet size
        if (length < 126)
        {
            headerSize = 2 + maskPresent * 4; //16 bit + 32bit mask
            msgSize = length;
        }
        else if (length == 126)
        {
            headerSize = 4 + maskPresent * 4; // 16bit + 16bit(len) + 32 bit mask
            msgSize = ntohs(*(unsigned short *)(buffer.getDataPtr() + processed + 2));
        }
        else
        {
            LogError("64bit fragment size??  why!?");
            return -1;
            //return ECONNRESET; //64bit length are wasteful for me right now
        }
            

        //LOGOUTPUT << "msgSize: " << msgSize;
        //LogInfo();

        if (opcode == 0) //continuation frame (not supported yet)
        {
            buffer.Clear(); //get rid of it!
            return count;
        }

        //this is very very bad...
        if (msgSize > maxPacketLength || msgSize == 0)
        {
            buffer.Clear();//super paranoid code would probably drop the whole message queue, maybe memset 0?
            //going to axe this whole buffer now... RUN AWAY
            LOGOUTPUT << "msgSize: " << msgSize << "  | maxpacketlength: " << maxPacketLength;
            LogInfo();
            LogError("bad msgSize, dropping buffer, and hoping for the best...");
            events->CallError(this, YAKERR_CORRUPTION);
            return count;
            LogError("scratch that, dropping the connection");
            //return ECONNRESET;
        }

        //dont have the whole packet yet, clear where we're at to reset buffer
        if (bytesToProcess < msgSize)
        {
            LOGOUTPUT << "clearing before whole packet has been recvd - bytesto: " << bytesToProcess << 
            " | msgSize " << msgSize;
            LogInfo();
            buffer.ClearBefore(processed);
            return count;
        }

        //TODO netbuffer factory to avoid allocations plz??
        //actually that could get pretty complex, memory management + fragmentation and such... hmm,
        //deffly TODO this later....  maybe just force all netbuffers to one size?


        //our packet format wants 2 bytes at start for size
        //and the next 2 bytes for a msg id (contained in payload)
        short payloadSize = msgSize;
        //LOGOUTPUT << "PAYLOAD SIZE????? - " << payloadSize;
        //LogInfo();
        if (payloadSize <= 0)
        {
            buffer.Clear();
            return count; //something screwey happening...
        }
        NetBuffer *nb = new NetBuffer(payloadSize+sizeof(short)); //size in front
        
        //skip header, and start writing at position 2 (after size)
        //unmask if mask exists
        
        char *payload = const_cast<char *>(buffer.getDataPtr()+processed+headerSize);
        if (maskPresent)
        {
            //LogInfo("masking???  never on client end!");
            //you'd think this would be the same endianness as header,  NOPE!
            unsigned int nmask = /*ntohl*/(*((unsigned int *)(payload -4)));
            char *_mask = (char *)&nmask;
            for (int i = 0; i < payloadSize; i++)
                payload[i] ^= _mask[i%4];
        }
        nb->SetPosition(0);
        nb->WriteShort(payloadSize+2);
        nb->FillBuffer(payload, payloadSize, 2);
        messageQueue.PushBack(nb);
        msgSize += headerSize;
        processed += msgSize;
        bytesToProcess -=msgSize;
    }

    buffer.ClearBefore(processed);
    return count;
}

//if you do not immediately process the buffer, it will be overwritten on the next recv
int TCPStream::Recv(int socket, int flags)
{
    //normally there is something to say when the message is over,  \r\n??
    //but i guess i'll let the user handle that and just push to a buffer
    int err = 0;
    int size = recv(socket, recvBuffer.data(), maxPacketLength, 0);

    if (size < 0) //there was an "error"
    {
        size = 0; //do not add -1 to an unsigned type that could be 0!
        err = errno;
        //NOTE windows is going to need some WSA junk
        switch(errno)
        {

            case EWOULDBLOCK: //nothing to read, done looping
                err = 0; //this is going to happen often with a nonblocking socket, dont treat as error
                return err;
                // LogInfo("got EWOULDBLOCK");
            break;
            case EBADF :
                LogError("recv() - bad file descriptor");
            break;
            case ECONNRESET :
                LogError("recv() - Connection reset");
            break;
            case EINTR :
                LogError("recv() - Signal interrupt");
            break;
            case ENOTCONN :
                LogError("recv() - Not connected");
            break;
            case ENOTSOCK :
                LogError("recv() - Not a socket");
            break;
            case EOPNOTSUPP :
                LogError("recv() - Not supported");
            break;
            case ETIMEDOUT :
                LogError("recv() - Timed out");
            break;
            default :
                LogError("recv() - error, other :]");
            break;
        }
        return -1;//nothing to process...
    }
    else if (size == 0)
    {
        err = ECONNRESET;  //maybe should be a seperate message, to mark graceful / ugly dc's
        LogError("recv() - gracefully disconnected todo: handle me!");
        return -1;
    }

    //just so no extra garbage gets left over.
    buffer.Clear();
    
    //fill the buffer
    if (buffer.Write(recvBuffer.data(), size))
        return size; //return bytes recv'd

    LogError("TCPBuffer full, dropping stream data, whole buffer will likely be dropped next. increase bufer size!!");
    return -1;
}
