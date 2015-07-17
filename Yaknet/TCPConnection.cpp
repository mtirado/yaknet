/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "TCPConnection.h"
#include "NetEvents.h"
#include "Yaknet/Utilities/Logger.h"

using std::vector;
void TCPConnection::Init(int maxLength)
{
    
    Connection::Init(maxLength);
    status  = 0;
    status |= CONN_TCP;
    numReadSets = 0;
    //setup read set and master set
    FD_ZERO(&masterSet);
    tvSelect.tv_sec = 0;
    tvSelect.tv_usec = 0;
}
TCPConnection::TCPConnection(NetEvents *netEvents, SOCKET &sock, sockaddr_in &_addr) : Connection(netEvents), buffer(MAX_MSG_SIZE * (MAX_PACKETS_IN_BUFFER + 1))
{
    Init(MAX_MSG_SIZE);
    memcpy(&addr, &_addr, sizeof(addr));
    SetId((unsigned int)addr.sin_addr.s_addr, (unsigned int)addr.sin_port);
    //set the map key
    //addr = _addr;
    theSocket = sock;
}

TCPConnection::TCPConnection(NetEvents *netEvents) : Connection(netEvents), buffer(MAX_MSG_SIZE * (MAX_PACKETS_IN_BUFFER + 1))
{
    Init(MAX_MSG_SIZE);
    memset(&addr, 0, sizeof(sockaddr_in));
    theSocket = SOCKET_ERROR;
}

//only call this if it is OK to have buffer and recv buffer of equal size
//dont forget to setup socket and addr!!
TCPConnection::TCPConnection(NetEvents *netEvents, unsigned int bufferSize) : Connection(netEvents), buffer(bufferSize)
{
    Init(bufferSize);
}

TCPConnection::~TCPConnection(){}

void TCPConnection::BeginTick()
{
    if ((status & CONN_LISTENING) )
    {    
        //for debug, should always be zero here
        if (numReadSets > 0)
        {
            LOGOUTPUT << "readset: " << numReadSets;
            LogInfo();
        }

        //dont let the master set get altered
        readSet = masterSet;
        //use select to read if anyone is trying to connect to theSocket
        //NOTE first arg was 0 on windos implementation?
        numReadSets = select(FD_SETSIZE, &readSet, 0, 0, &tvSelect);
    }
    //process connection drops, at the same time
    ProcessDrops();
}

Connection *TCPConnection::AcceptConnection()
{
    if (!(status & CONN_LISTENING) || status & CONN_CONNECTED )
    {
        LogError("cannot accept new connections on this socket.");
        return 0;
    }

    //ready for reading
    if (FD_ISSET(theSocket, &readSet) != 0)
    {
        LogInfo("AcceptConnection() FD_ISSET");
        //check the incomming connection
        SOCKET      newSocket;
        sockaddr_in newAddr;
        socklen_t nAddrLen = sizeof(sockaddr_in);

        newSocket = accept(theSocket, (sockaddr *)&newAddr, &nAddrLen);

        if (newSocket == INVALID_SOCKET)
        {
            LogError("ERROR: Invalid Socket - ");
            return 0;
        }

         //SET TO NON-BLOCKING!!
        if (!SetSocketBlockingEnabled(newSocket, false))
            LogError("COULD NOT SET NON-BLOCKING SOCKET");
        
        //disable nagles algorithm (send packets immediately do not buffer)
        int nagle = 1;
        int result = setsockopt(newSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &nagle, sizeof(int));
        if (result < 0)
            LogWarning("Connect() - Could not set TCP_NODELAY");

        //add the theSocket to the connection list
        TCPConnection *conn = new TCPConnection(events, newSocket, newAddr);
        --numReadSets;

        //FD_SET(((TCPConnection *)pConn)->theSocket, &masterSet);
        //add the connection to our list
        //connections[conn->GetId()] = conn;
        //connections.push_back(conn);
        AddConnection(conn);
        conn->status |= CONN_CONNECTED ; 

        LogInfo("conn accepted!");
        //return the new connection
        return conn;
    }

    //no new connections
    return 0;

}

void TCPConnection::DropConnection(Connection *conn)
{

    if (((TCPConnection *)conn)->theSocket == theSocket)
    {
        //LogInfo("Dropping connection");
        Disconnect(); // that was this connection that was dropped
    }
    else
    {
        //simply close the theSocket and remove it from the connection list
        std::map<sockaddr_key_t, Connection *>::iterator iter = connections.find(conn->GetId());
        if (iter != connections.end())
        {
            FD_CLR(((TCPConnection *)conn)->theSocket, &masterSet);
            LOGOUTPUT << "Dropping connection id = " << conn->GetId().GetData();
            LogInfo();
            iter->second->Disconnect(); //this flags for disconnect?
            dropList.PushBack(iter->second);
        }
    }
}

//returns the connection we just made
Connection *TCPConnection::Connect(const char *ipAddress, unsigned short port)
{
    //TODO test!
    //this behavior should be fine, the master connection's socket should be able to listen and make new connections
//     if (status & CONN_LISTENING)
//     {
//         LogError("cannot connect(), this connection is listening.");
//         return 0;
//     }

    //connections sockets(theSocket) are for listening, unless created by connecting like so
    TCPConnection *newConn = new TCPConnection(events);

    memset(&newConn->addr, 0, sizeof(newConn->addr));
    newConn->addr.sin_family = AF_INET;
    newConn->addr.sin_port = htons(port);
    //create the theSocket
#ifdef _WIN32_
    newConn->addr.sin_addr.s_addr = inet_addr(ipAddress);
    newConn->theSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    inet_pton(AF_INET, ipAddress, &newConn->addr.sin_addr);
    newConn->theSocket = socket(AF_INET, SOCK_STREAM, 0);
#endif

    //disable nagles algorithm (send packets immediately do not buffer)
    int nagle = 1;
    int result = setsockopt(newConn->theSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &nagle, sizeof(int));
    if (result < 0)
        LogWarning("Connect() - Could not set TCP_NODELAY");


    if (newConn->theSocket == INVALID_SOCKET)
    {
        LogError("theSocket(): Invalid Socket - ");
        delete newConn;
        return 0;
    }

    //connect to the server
    if (connect(newConn->theSocket, (sockaddr *)&newConn->addr, sizeof(sockaddr)) == SOCKET_ERROR)
    {
        LOGOUTPUT << "Connect() :  socket errno(" << errno << ")";
        LogError();
        delete newConn;
        return 0;
    }

    //set to NON blocking after we connect, otherwise we get EINPROGRESS
    if (!SetSocketBlockingEnabled(newConn->theSocket, false))
        LogError("COULD NOT SET NON-BLOCKING SOCKET");

    //we must add this connection to the list so process events knows about it!
    //connections.push_back(newConn);
    //connections[newConn->GetId()] = newConn;
    AddConnection(newConn);
    newConn->status |= CONN_CONNECTED;
    LogInfo("socket connected...");

    return newConn;
}

bool TCPConnection::SendPreparedData(const char *data, unsigned int size)
{
    if (isDropping())
        return false;

    //int size = *(unsigned short *)data;
    if (size > MAX_MSG_SIZE)
        size = MAX_MSG_SIZE;
    
    //send data to a specific client
    int bytesSent = send(theSocket, data, size, 0);
    int sent;
    if (bytesSent == SOCKET_ERROR)
    {
        LogError("send(): Socket Error - ");
        DropConnection(this);
        return false;
    }

    while (bytesSent < size)
    {
        sent = send(theSocket, data, size, 0);
        if (sent == SOCKET_ERROR)
        { 
            LogError("send(): Socket Error - ");
            DropConnection(this);
            return false;
        }

        bytesSent += sent;
    }
    return true;
}


bool TCPConnection::SendDataToAllConnections(char *data, unsigned short eventId)
{
    unsigned short size = *(unsigned short *)data;
    *(unsigned short *)(data + sizeof(unsigned short)) = eventId;
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    for ( ; iter != connections.end(); iter++)
    {
        ((TCPConnection *)iter->second)->SendPreparedData(data, size);
    }
    return true;
}

//doesnt assigns msg id (internal use for sendtoallconnections)
bool TCPConnection::SendData(char *data, unsigned short eventId)
{
    if (isDropping())
        return false;

    *(unsigned short *)(data + sizeof(unsigned short)) = eventId;
    int size = *(unsigned short *)data;
    if (size > MAX_MSG_SIZE)
        size = MAX_MSG_SIZE;

    //send data to a specific client
    int bytesSent = send(theSocket, data, size, 0);
    int sent;
    if (bytesSent == SOCKET_ERROR)
    {
        LogError("send(): Socket Error - ");
        DropConnection(this);
        return false;
    }

    while (bytesSent < size)
    {
        sent = send(theSocket, data, size, 0);
        if (sent == SOCKET_ERROR)
        { 
            LogError("send(): Socket Error - ");
            DropConnection(this);
            return false;
        }

        bytesSent += sent;
    }
    return true;
}


int TCPConnection::RecvData()
{   
    return Recv(theSocket, 0);
}


int TCPConnection::RecvFromAllConnections( )
{
    int recvRead = 0;
    unsigned int numBytes = 0;
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    for ( ; iter != connections.end(); iter++)
    {
        recvRead = iter->second->RecvData();
        if (recvRead >= 0)
            numBytes += recvRead;
        else
            DropConnection(iter->second);
    }
    return numBytes;
}

void TCPConnection::Disconnect()
{
#ifdef _WIN32_
    closesocket(theSocket);
#else
    int err = shutdown(theSocket, SHUT_RDWR); 
    if (err == -1) // errno   use stderror to get exact reason (may not be thread safe?)
    {
        LOGOUTPUT << "errno - " << errno << "\n";
        LogError();
        LogError("shutdown(socket) error,  possible causes :\nis not a valid descriptor.\nThe specified socket is not connected.\nis a file, not a socket.");
    }
#endif

    //remove from connections next tick
    status |= CONN_DROPPING;

    //the disconnect callback
    events->CallDisconnect(this, 0);

    //TODO should probably notify all connection of a graceful disconnect?
    //clear connection list
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    while (iter != connections.end())
    {
        delete iter->second;
        iter++;
    }
    connections.clear();

    //turn off bits
    status = status & ~CONN_CONNECTED;
    status = status & ~CONN_LISTENING; 
    
}
