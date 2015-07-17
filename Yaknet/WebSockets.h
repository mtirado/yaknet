/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef _WEB_SOCKETS_H_
#define _WEB_SOCKETS_H_

#include "TCPConnection.h"

///this class is not fully implemented yet (but it does work from web browser)
///maintains the websockiets protocol ontop of our normal TCPConnection
///usage is exactly the same as TCPConnection

///implements TCPConnection over websockets
class WebSockConnection : public TCPConnection
{
private:
    bool toClient;
protected:
    std::vector <WebSockConnection *> pendingConnections; //waiting for handshake


    ///at this time a copy buffer is needed to shift data before sending
    ///this is to allow for easy implementation of identical TCPConnection usage
    ///but really its a silly thing to be doing :[  fixit?
    char copyBuff[MAX_MSG_SIZE]; //to append header to regular tcp data layer
    //header construction variables for sending
    int headerSize;
    unsigned short header[2];
    unsigned int mask;

    //will recv, checking for handshake -1 error, 0 nope, 1 - handshake!
    int TryHandshake();

    WebSockConnection(NetEvents *netEvents, SOCKET &sock, sockaddr_in &addr) : TCPConnection(netEvents, sock, addr)
                                                        { toClient = true; }
    //handle the extra protocol stuff here
    virtual int Recv(int socket, int flags);
    bool PrepareWebSocketHeader(char *data);
public:
    WebSockConnection(NetEvents *netEvents) : TCPConnection(netEvents) { toClient = true; }
    virtual Connection *AcceptConnection();
    //virtual void DropConnection(Connection *conn);
    virtual Connection *Connect(const char *ipAddr, unsigned short port);
    //virtual void Disconnect();
    virtual bool SendData(char *data, unsigned short eventId);
    virtual bool SendDataToAllConnections(char *data, unsigned short eventId);
    //virtual bool RecvData();

};
//helpers for constructing and reading websocket header flags
#define MIN_MSG_SIZE 2

#define WSFLAG_FIN(x)               (x & 32768)
#define WSFLAG_MASKED(x)            (x & 128)
#define WSFLAG_PAYLOADSIZE(x)       (x & 127)
#define WSFLAG_OPCODE(x)            ((x >> 8) & 15)

#define WSFLAG_SET_FIN(x)           (x |= 32768)
#define WSFLAG_SET_MASK(x)          (x |= 128)
#define WSFLAG_SET_PAYLOADSIZE(x,y) (x |= (y & 127))
#define WSFLAG_SET_OPCODE(x,y)      (x |= ((y & 15) << 8))

#endif
