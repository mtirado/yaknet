/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "TCPConnection.h"

///a plain ole TCP stream, no packetized header data or message seperation
class TCPStream : public TCPConnection
{
protected:

    ///Recv will clear the ByteBuffer, so process the data recv'd before you call Recv again.
    virtual int Recv(int socket, int flags);
    TCPStream(NetEvents *netEvents, SOCKET &sock, sockaddr_in &addr);

public:
    TCPStream(NetEvents *netEvents);
    ~TCPStream();

    //TODO write a server test for TCPStream (only tested client on irc)
    virtual int RecvData();
    virtual Connection *Connect(const char *ipAddress, unsigned short port);

    ///instead of an eventId the second parameter is the size of the data
    virtual bool SendData(char *data, unsigned short size);
    ///there is no event id, second parameter is size of data.
    virtual bool SendDataToAllConnections(char *data, unsigned short size);
    ///yaknet events do not exist within a TCPStream
    virtual void ProcessEvents(){}
};
