/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Connection.h"
#include "TCPConnection.h"
#include "NetEvents.h"
#include "Yaknet/Utilities/Logger.h"
//some of the standard TCP / UDP code is very similar so i'm implementing them here
//to avoid the code bloat
std::vector<char> recvBuffer; //remove this, and use recvBuffer for each connection instance for thread safe-ness

Connection::Connection(NetEvents* netEvents)
{
    events = netEvents;
    status = 0; 
    theSocket = INVALID_SOCKET;
}

void Connection::Init(int maxLength)
{
    SetId(0,0);
    status = 0;
    maxPacketLength = maxLength;
    recvBuffer.reserve(maxLength); //TODO a resize function for different size buffers? otherwise, might as well just use MAX_MSG_SIZE
}

void Connection::AddConnection(Connection* conn)
{
    //sets the connections ID, using its address
    conn->SetId((unsigned int)conn->addr.sin_addr.s_addr, (unsigned int)conn->addr.sin_port);
    connections[conn->id] = conn;
}

void Connection::SetId(unsigned int ipv4, unsigned int port)
{
    id.Set(ipv4, port);
}


void Connection::ProcessDrops()
{
    size_t size = dropList.size();
    while(size > 0)
    {
        connections.erase(dropList.Front()->GetId());
        delete dropList.Front();
        dropList.PopFront();
        LogInfo("------- connection deleted -------");
        LOGOUTPUT << "connection size: " << connections.size();
        LogInfo();
        size--;
    }
    
}


void Connection::ProcessEvents()
{
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    //for (int i = 0; i < connections.size(); i++)
    for ( ; iter != connections.end(); iter++)
    {
        //TCPConnection *conn = (TCPConnection *)connections[i];
        TCPConnection *conn = (TCPConnection *)(iter->second);
        //process all network events!
        size_t num = conn->getNumMessages();
        while (num)
        {
            NetBuffer *msg = conn->getMessage();
           
            //msg->PrintBytes();
            msg->SetPosition(0); //get at the header info
            unsigned short size = msg->ReadShort();
            unsigned short mId  = msg->ReadShort();
           // LOGOUTPUT << "SIZE: " << size << "\nEVENT ID: " << mId;
            //LogInfo();

            events->ProcessEvent(mId, conn, msg);

            conn->popMessage();
            delete msg; //to the trashcan with ye
            num--;
        }
    }
}
        


bool Connection::Listen(unsigned short port)
{
    if (status & CONN_LISTENING || status & CONN_CONNECTED)
    {
        LogError("Listen() - Socket is already listening, or connected to something");
        return false;
    }

    //create the theSocket
    if (status & CONN_TCP)
    {
#ifdef _WIN32_
    theSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    theSocket = socket(AF_INET, SOCK_STREAM, 0);
#endif
    }
    else
    {
#ifdef _WIN32_
    theSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    theSocket = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    }


    if (theSocket == INVALID_SOCKET)
    {
        LogError("Socket(): Invalid Socket - ");
        return false;
    }

     /* So that we can re-bind to it without TIME_WAIT problems */
    int reuse_addr = 1;
    setsockopt(theSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
        sizeof(reuse_addr));

    //SET TO NON-BLOCKING!!
    if (!SetSocketBlockingEnabled(theSocket, false))
        LogError("COULD NOT SET NON-BLOCKING SOCKET");
    else LogInfo("Socket set as non blocking");


    //setup our address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    LogInfo("------- addr initialized -------");
    //bind this theSocket to our address
    int ret = bind(theSocket, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        LogError("bind(): Socket Error - ");
        return false;
    }

    if (status & CONN_TCP)
    {
        //disable nagles algorithm (send packets immediately do not buffer)
        int nagle = 1;
        int result = setsockopt(theSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &nagle, sizeof(int));
        if (result < 0)
            LogWarning("Connect() - Could not set TCP_NODELAY");
    
        //theSocket has been binded, so listen for connections
        ret = listen(theSocket, SOMAXCONN);
        if (ret == SOCKET_ERROR)
        {
            LogError("listen() - Socket Error - ");
            return false;
        }

        FD_SET(theSocket, & ((TCPConnection *)this)->masterSet);
    }
    
    status |= CONN_LISTENING;
    LogInfo("Socket Listening...");
    return true;

}



/** Returns true on success, or false if there was an error */
bool Connection::SetSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;

#ifdef WIN32
   unsigned long mode = blocking ? 0 : 1;
   return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return false;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
}
