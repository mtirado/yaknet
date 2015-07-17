/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __TCP_ARMORED_H__
#define __TCP_ARMORED_H__

#include "TCPConnection.h"
#include "DataStructures/TList.h"
#include <gnutls/gnutls.h>
#include <gnutls/openpgp.h>

///this implementation uses GnuTLS with x.509 certificates, using deffie-hellman key exchange
///but it shouldnt be too difficult to change it to OpenSSL, or whatever crypto library you'd prefer.
///it should be noted that even though Connect returns a valid connection, isConnected will not be set untill 
///the handshake has completed, if the handshake fails this connection is deleted (use error callback for this notification)

///an armored tcp connection that uses OpenPGP encryption
class TCPArmored : public TCPConnection
{
friend class Connection; //dont worry about this...  eliminating duplicated code with Connection.cpp
static bool gnutls_initialized;
private:
    ///pending outbound connections to update handshake on
    TList<TCPArmored *>  outgoingConnections;
    ///pending inbound connections, awaiting TLS handshake completion
    std::map<sockaddr_key_t, TCPArmored *> incomingConnections;
    ///checks the status of a handshake with the incoming connections
    void CheckHandshakes();
    
    ///used for construction after sock, and addr have been validated
    TCPArmored(NetEvents *netEvents, SOCKET &sock, sockaddr_in &addr);

    ///send data that has already been prepared.
    ///a slight optimization for SendDataToAllConnections, given a large number of connections
    ///most important for websock connection, which shifts the whole buffer before sending
    bool SendPreparedData(const char *data, unsigned int size);

    ///common constructor init
    virtual void Init(int maxLength);
    
    ///recv from a given socket, and populate message(event) queue
    //virtual int Recv(int socket, int flags);
    
    //key exchange parameters
    gnutls_dh_params_t dh_params;
    //the gnutls session id(?)
    gnutls_session_t session;
    //certificate
    gnutls_certificate_credentials_t credentials;
    

public:
    TCPArmored(NetEvents *netEvents);
    ~TCPArmored();

    ///executes connection drops, calls select and updates read set
    ///listening connections must call this every server tick
    ///NOTE this function is from now on required to be called on the server and client
    ///to process dropped connections and handshake logic, make it so!
    virtual void BeginTick();
    virtual bool Listen(unsigned short port);
    virtual Connection *AcceptConnection();
    virtual Connection *Connect(const char *ipAddr, unsigned short port);
    virtual void Disconnect();
    //sends to only this connection
    virtual bool SendData(char *data, unsigned short eventId);
    //sends to all other connections in connection list
    virtual bool SendDataToAllConnections(char *data, unsigned short eventId);
    //recv from this connection, returns number of bytes read
    //or -1 for error
    virtual int  RecvData();
    
};

#endif
