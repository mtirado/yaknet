/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __TCPCONNECTION_H__
#define __TCPCONNECTION_H__

#include "Connection.h"


///each TCPConnection has a socket for listening(theSocket), and adding connections as they connect.
///if you want to connect to others, a new connection will be allocated and added to current connections list
///this hopefully will allow for easy(?) peer to peer communication and be worth the added confusion

///Packetized TCP connection
class TCPConnection : public Connection
{
friend class Connection; //dont worry about this...  eliminating duplicated code with Connection.cpp
protected:
    int             numReadSets;  //how many read sets select returned
    timeval         tvSelect;     //select timeout value
    fd_set          masterSet;    //master set for selecting
    fd_set          readSet;      //"temp" read set to pass to select
    
    ///used for construction after sock, and addr have been validated
    TCPConnection(NetEvents *netEvents, SOCKET &sock, sockaddr_in &addr);
    ///mild hackery used to initialize buffer in TCPStream subclass
    TCPConnection(NetEvents *netEvents, unsigned int bufferSize);
    
    ///send data that has already been prepared.
    ///a slight optimization for SendDataToAllConnections, given a large number of connections
    ///most important for websock connection, which shifts the whole buffer before sending
    bool SendPreparedData(const char *data, unsigned int size);

    ///holds partial packet data between recv's
    ByteBuffer buffer;
    

    ///common constructor init
    virtual void Init(int maxLength);
    
    ///returns bytes received, or an error number ( anegative errno value)
    virtual int Recv(int socket, int flags);

public:
    TCPConnection(NetEvents *netEvents);
    ~TCPConnection();

    ///executes connection drops, calls select and updates read set
    ///listening connections must call this every server tick
    ///TODO this function is from now on required to be called on the server and client
    ///for processing dropped connections, make it so!
    virtual void BeginTick();
    //virtual bool Listen(short port);
    virtual Connection *AcceptConnection();
    virtual void DropConnection(Connection *conn);
    virtual Connection *Connect(const char *ipAddr, unsigned short port);
    virtual void Disconnect();
    //sends to only this connection
    virtual bool SendData(char *data, unsigned short eventId);
    //sends to all other connections in connection list
    virtual bool SendDataToAllConnections(char *data, unsigned short eventId);
    //recv from this connection, returns number of bytes read
    //or -1 for error
    virtual int  RecvData();
    //recv from all connections
    virtual int RecvFromAllConnections();

    //internal byte buffer data(filled by recv)
    inline const char *getBufferData() const { return buffer.getDataPtr(); }

};

#endif
