/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "TCPStream.h"
#include "Yaknet/Utilities/Logger.h"
TCPStream::TCPStream(NetEvents *netEvents, SOCKET &sock, sockaddr_in &_addr) : TCPConnection(netEvents, MAX_STREAM_RECV)
{
    Init(MAX_MSG_SIZE);
    memcpy(&addr, &_addr, sizeof(addr));
    //addr = _addr;
    theSocket = sock;
}

TCPStream::TCPStream(NetEvents *netEvents) : TCPConnection(netEvents, MAX_STREAM_RECV)
{
    Init(MAX_MSG_SIZE);
    memset(&addr, 0, sizeof(sockaddr_in));
    theSocket = SOCKET_ERROR;
}

TCPStream::~TCPStream()
{
}

int TCPStream::RecvData()
{
    return Recv(theSocket, 0);
}


//ignore event ID's
bool TCPStream::SendDataToAllConnections(char *data, unsigned short size)
{
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    for ( ; iter != connections.end(); iter++)
    {
        iter->second->SendData(data, size);
    }
    return true;
}

//doesnt assigns msg id (internal use for sendtoallconnections)
bool TCPStream::SendData(char *data, unsigned short size)
{
    if (isDropping())
        return false;

    
    if (size > MAX_STREAM_RECV)
    {
        LogError("TCPStream::SendData truncated message, MAX_STREAM_RECV");
        size = MAX_STREAM_RECV;
    }

    //send data to a specific client
    int bytesSent = send(theSocket, data, size, 0);
    int sent;
    if (bytesSent == SOCKET_ERROR)
    {
        LogError("send(): Socket Error - ");
        DropConnection(this);
        return false;
    }

    //TODO maybe iterate a few times and break, instead of infinite loop??
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

//this is exactly the same as TCPConnection::connect just with a TCPStream allocation...
//theres probably a better way TODO this
//returns the connection we just made
Connection *TCPStream::Connect(const char *ipAddress, unsigned short port)
{
    if (status & CONN_LISTENING)
    {
        LogError("cannot connect(), this connection is listening.");
        return 0;
    }

    //connections sockets are for listening, unless created by connecting like so
    TCPStream *newConn = new TCPStream(events);
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

    //connection established, add to the map
    //connections[newConn->GetId()] = newConn;
    AddConnection(newConn);

    newConn->status |= CONN_CONNECTED;
    LogInfo("socket connected...");

    return newConn;
}
