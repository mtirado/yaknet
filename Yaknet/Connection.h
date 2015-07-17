/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

//TODO test large file transfers...
//first 2 bytes of the packet is the size.  meaning no packets larger than MAX_SHORT plz.
//for large files it would probably be a good idea to use TCPStream

#define MAX_MSG_SIZE 512 // maximum length of a packet
#define MAX_STREAM_RECV 1024 //maximum read on TCPStream::Recv
 //may want to tune buffer sizes a bit, i just set this up real quick
#define MAX_PACKETS_IN_BUFFER 10 //maximum theoretically full packets allowed in the recvbuffer


#include <map>
#include "Yaknet/DataStructures/ByteBuffer.h"
#include "Yaknet/DataStructures/TQueue.h"
#include "Yaknet/DataStructures/map_keys.h"
#include "Yaknet/NetEvents.h"
#include "NetBuffer.h"
#include <vector>
#include <map>

//TODO find someone with windows to try yaknet....

//normal unix socket stuffs, 
#ifndef _WIN32_
    #include <errno.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <sys/fcntl.h>
    #include <arpa/inet.h>
    #include <netinet/tcp.h>
    #include <netinet/in.h>
    #define SOCKET int
    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
#endif


/*   status flags
lotsa bits
32  31  30  29   .....  4   3   2   1

bit 1   =   connected
bit 2   =   listening
bit 3   =   on - TCP  off - UDP
bit 4   =   on - packetized TCP  off - TCP stream
bit 5   =   has disconnected and will be removed next tick
*/

//TODO should probably improve state management soon.
#define CONN_CONNECTED      0x1   //connected!
#define CONN_LISTENING      0x2   //this connection is listening for connections.
#define CONN_TCP            0x4   //UDP not implemented yet.
#define CONN_STREAMING      0x8   //not using this (no stream implemented yet).
#define CONN_DROPPING       0x10  //the connection is about to be dropped next tick.

///the temporary recv buffer, probably not thread safe, if you need thread safety use the commented out recvBuffer
extern std::vector<char> recvBuffer; //just using vector for resize/reserve/capacity functionality

///note that some subclasses may completely disable methods.
///for example, TCPStream disables ProcessEvents

///abstract base class for all connections.
class Connection
{
    friend class UDPConnection; //it needs access to the message queue
private:
    ///the connection key in parents connection map, 0 would be invalid
    sockaddr_key_t id;
    //disabled, constructor must pass events pointer
    Connection(){}
protected:
    ///a top level connection's socket is used to listening on, and to make new connections
    SOCKET          theSocket;
    sockaddr_in     addr;

    ///this is where all events are called, application defined.
    NetEvents *events;
    
    ///describes the state of this connection.
    unsigned int status;

    ///list of connections made through either AcceptConnection, or Connect.
    ///the key is the address and port represented as 64 bits. an unordered map is probably faster..
    std::map<sockaddr_key_t, Connection *> connections;

    //temporary data, recv reads into this vector.
    //NOTE: if you're doing multithreaded connection recv'ing, you should uncomment this
    //so that each instance has their own recv buffer, static buffer is fine for sequential recvs
    //std::vector<char> recvBuffer; //just using vector for resize/reserve/capacity functionality

    ///a list of connections that are about to be dropped next tick
    TQueue<Connection *> dropList;
    
    ///any packets larger than this will be truncated
    unsigned short maxPacketLength;
    
    ///common constructor init
    virtual void Init(int maxLength);

    ///adds a connection to the map and sets its id (map key)
    void AddConnection(Connection * conn);

    ///event queue, populated during recv(). be sure to pop and delete after a message is processed!
    TQueue<NetBuffer *> messageQueue;

    ///returns message in front of queue
    inline NetBuffer *getMessage() { return messageQueue.Front(); }
    ///pops message off front of queue
    inline void popMessage() { messageQueue.PopFront(); }
    inline size_t getNumMessages() { return messageQueue.size(); }
    
public:

     Connection(NetEvents *netEvents);
    ~Connection() { }

    ///deletes connections that are flagged as being dropped
    virtual void ProcessDrops();
    ///tell connection to listen on a port. only top level(master) connections should be listening
    virtual bool Listen(unsigned short port);
    ///pretty much an update, call this at top of update loop (server and client)
    virtual void BeginTick() = 0;
    ///returns first new incoming connection and automatically adds to internal connection list.
    virtual Connection *AcceptConnection() = 0;
    ///drops the connection passed in from internal connection list.
    virtual void DropConnection(Connection *conn) = 0;
    ///attempt to make a connection, returns 0 on failure.
    ///connection is automatically added to internal connection list.
    virtual Connection *Connect(const char *ipAddr, unsigned short port) = 0;
    ///closes this connection.
    virtual void Disconnect() = 0;
    ///send data over this connection.
    virtual bool SendData(char *data, unsigned short eventId) = 0;
    ///send data to everyone in connection list.
    virtual bool SendDataToAllConnections(char *data, unsigned short eventId) = 0;
    ///recv data from this connection, returns number of bytes recv'd.
    virtual int RecvData() {} // UDP has no individial recv
    ///recv data from everyone in connection list, returns number of bytes recv'd.
    virtual int RecvFromAllConnections() = 0;
    ///reads every message in the queue and executes the callback assigned to it
    ///uses the NeEvents singleton to lookup registered callbacks for the event
    ///currently only the master connection can process events (conn that calls Connect / Accept)
    virtual void ProcessEvents();
    ///every connection by default is non-blocking, if you want a connection to block pass true to this
    bool SetSocketBlockingEnabled(int socket, bool blocking);

    inline sockaddr_key_t GetId() { return id; }
    void SetId(unsigned int ipv4, unsigned int port);
    inline bool isConnected()     { return status & CONN_CONNECTED; }
    ///this connection will be dropped next tick.
    inline bool isDropping() { return status & CONN_DROPPING; }
};


#endif
