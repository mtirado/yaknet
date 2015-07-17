/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "TCPArmored.h"
#include "Utilities/Logger.h"
#include "NetEvents.h"

//use a faster more insecure handshake (for development runs)
#define FAST_ECDHE

#define KEYFILE  "x509-server-key.pem"
#define CERTFILE "x509-server.pem"
#define CAFILE   "x509-ca.pem"
//TODO: x509 CRL --revocation list-- ??

//NOTE i left in all the anonymous authentication function calls as comments
//incase that type of behavior is desired.
//TEMPORARY -- if using anonymous auth put these in the connection class
//gnutls_anon_client_credentials_t anoncred;
//gnutls_anon_server_credentials_t s_anoncred;

bool TCPArmored::gnutls_initialized = false;
void TCPArmored::Init(int maxLength)
{
    credentials = 0;
    session = 0;
    //initiailze gnutls if we havent already done so.
    if (!gnutls_initialized)
    {
        LOGOUTPUT << "initializing gnutls version: " << gnutls_check_version(0);
        LogInfo();
        //honestly it should be 3.3 and greater, new vulns found and all...
        if (gnutls_check_version("3.1.5") == NULL) 
        {
            LOGOUTPUT << "if your version of GnuTLS is lower than 3.1.5, please update!\n";
            LogWarning();
            //exit(1);
        }
        int ret = gnutls_global_init();
        if (ret != 0)
        {
            LOGOUTPUT << "Could not initialize GnuTLS -- error: " << ret;
            LogError();
            return;//return before connection is initted, will cause a crash.
        }
        gnutls_initialized = true;
    }
    //initialize TCP layer
    TCPConnection::Init(maxLength);
}

TCPArmored::TCPArmored(NetEvents *netEvents, int& sock, sockaddr_in& addr): TCPConnection(netEvents, sock, addr)
{
    Init(MAX_MSG_SIZE);
}

TCPArmored::TCPArmored(NetEvents *netEvents) : TCPConnection(netEvents)
{
    Init(MAX_MSG_SIZE);
}

TCPArmored::~TCPArmored(){}


bool TCPArmored::Listen(unsigned short port)
{
    if(TCPConnection::Listen(port))
    {
        LogInfo("initializing credentials");
        gnutls_certificate_allocate_credentials(&credentials);
        int ret;
        ret = gnutls_certificate_set_x509_trust_file(credentials, CAFILE, GNUTLS_X509_FMT_PEM);
        if (ret < 0)
        {
            LOGOUTPUT <<"gnutls_certificate_set_x509_trust_file error: " << ret;
            LogError();
            return false;
        }
        //set crl file?
        ret = gnutls_certificate_set_x509_key_file(credentials, CERTFILE, KEYFILE, GNUTLS_X509_FMT_PEM);
        if (ret < 0)
        {
            LOGOUTPUT <<"gnutls_certificate_set_x509_key_file error: " << ret;
            LogError();
            return false;
        }
        
        //gnutls_anon_allocate_server_credentials(&s_anoncred);
        //server needs to generate key exchange parameters
#if defined(FAST_ECDHE)
        unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_LOW);
        LogWarning("using faster, less secure handshake parameters.");
#else
        unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_HIGH);
#endif
        LOGOUTPUT << "dh bits: " << bits; LogInfo();
        //TODO the documentation tells us we should discard and regenerate these once a day/week/month -- somethin
        gnutls_dh_params_init(&dh_params);
        ret = gnutls_dh_params_generate2(dh_params, bits);
        //set the cert, and setup 'server' session info
        gnutls_certificate_set_dh_params(credentials, dh_params);
        //gnutls_anon_set_server_dh_params(s_anoncred, dh_params);
        
        if (ret < 0)
        {
            LOGOUTPUT <<"gnutls_certificate_set_dh_params error: " << ret;
            LogError();
            return false;
        }
        //should now be ready to accept connections and handshake
        LogInfo("handshake ready.");
        
    
        return true;
    }
    return false;
}

Connection* TCPArmored::AcceptConnection()
{
    if (!(status & CONN_LISTENING) || status & CONN_CONNECTED )
    {
        LogError("cannot accept new connections on this socket.");
        return 0;
    }

    //ready for reading
    if (FD_ISSET(theSocket, &readSet) != 0)
    {
        LogInfo("TCPArmored::AcceptConnection() FD_ISSET");
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

        //set the id, and add to incoming connections
        TCPArmored *conn = new TCPArmored(events, newSocket, newAddr);
        --numReadSets;
       
        //add to pending connection list (if it does not already exist
        sockaddr_key_t key = conn->GetId();
        std::map<sockaddr_key_t, TCPArmored *>::iterator iter = incomingConnections.find(key);
        if (iter == incomingConnections.end()) //this could be a retransmission so it may already exist in list
        {
            //add to incoming list so we can run the handshake on it in BeginTick
            LogInfo("TCPArmored -- accepted, awaiting handshake.");
            incomingConnections[key] = conn;
            //init session as server
            gnutls_init(&conn->session, GNUTLS_NONBLOCK/* | not sure if i need*/ | GNUTLS_SERVER);
            gnutls_certificate_server_set_request(conn->session, GNUTLS_CERT_IGNORE);
            //uses 'this' connections credentials
            gnutls_credentials_set(conn->session, GNUTLS_CRD_CERTIFICATE, credentials);
//             gnutls_priority_set_direct(conn->session,
//                                            "NORMAL:+ANON-ECDH:+ANON-DH",
//                                            NULL);
//             gnutls_credentials_set(conn->session, GNUTLS_CRD_ANON, s_anoncred);
            
            //uses RSA authentication + 256bit AES
#if defined(FAST_ECDHE)
            gnutls_priority_set_direct(conn->session, "SECURE128", NULL);
#else
            gnutls_priority_set_direct(conn->session, "SECURE256:-ECDHE-RSA:-RSA:+DHE-RSA", NULL);
#endif
            // request client certificate if any.
            gnutls_handshake_set_timeout(conn->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
            gnutls_transport_set_int(conn->session, conn->theSocket); //think this ties session to socket
        }
        else
            LogWarning("TCPArmored -- weird: pending incoming connection was already hanging out in the incoming list...");

    }
    
    //check for connected status, return first one found
    std::map<sockaddr_key_t, TCPArmored *>::iterator iter = incomingConnections.begin();
    TCPArmored *conn;
    for( ; iter != incomingConnections.end(); ++iter)
    {
        conn = iter->second;
        if (conn->status & CONN_CONNECTED)
        {
            incomingConnections.erase(conn->GetId());
            AddConnection(conn);
            //call the main connect callback
            events->CallConnect(conn, 0);
            return conn;
        }
    }

    //no new connections
    return 0;
}

void TCPArmored::BeginTick()
{
    //processes drops, and updates read set
    TCPConnection::BeginTick();
    
    //run handshake on pending connections, add them if successful
    CheckHandshakes();
}

//runs through the incoming connections map and checks handshake status, sets connected flag if success
//accept connection will check for connected flag to give us the new verified armored connection
void TCPArmored::CheckHandshakes()
{
    int ret;
    TCPArmored *conn;
    
    //outgoing connections
    TList<TCPArmored *>::iterator c_iter(outgoingConnections);
    for( ; !c_iter.past_end(); c_iter.next())
    {
        conn = c_iter.item();
        LOGOUTPUT << "connection socket: " << conn->theSocket << "\nsession: " << conn->session;
        LogInfo();
        ret = gnutls_handshake(conn->session);
        if (ret < 0)
        {
            //non blocking sockets will return these errors untill success
            if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
                continue;
            //NOTE on a rehandshake the following errors may be returned: GNUTLS_E_GOT_APPLICATION_DATA || GNUTLS_E_WARNING_ALERT_RECEIVED
            if (ret == GNUTLS_E_GOT_APPLICATION_DATA)
            {
                LogWarning("Rehandshake occurred, data is pending?");
                continue;
            }
            if (ret == GNUTLS_E_WARNING_ALERT_RECEIVED)
            {
                LogWarning("error: GNUTLS_E_WARNING_ALERT_RECEIVED -- not exactly sure what this means, treating as fatal error");
            }
            //some fatal error happened :(
            LOGOUTPUT << "outgoing Handshake error(" << ret << ") : " << gnutls_strerror(ret);
            LogError();
            
            conn->Disconnect();
            outgoingConnections.PopAt(c_iter);
            c_iter.previous();
            events->CallError(conn, YAKERR_HANDSHAKE);
            delete conn;
        }
        else if (ret == GNUTLS_E_SUCCESS)
        {
            //print some info
            char *desc;
            desc = gnutls_session_get_desc(conn->session);
            LOGOUTPUT << "client handshake accepted: " << desc;
            LogInfo();
            gnutls_free(desc);
            
            //transfer lists
            outgoingConnections.PopAt(c_iter);
            c_iter.previous();
            AddConnection(conn);
            conn->status |= CONN_CONNECTED;
            //call the connected callback
            events->CallConnect(conn, 0);
        }
    }

    
    //incoming connections
    std::map<sockaddr_key_t, TCPArmored *>::iterator iter = incomingConnections.begin();
    for( ; iter != incomingConnections.end(); ++iter)
    {
        conn = iter->second;
        if (conn->status & CONN_CONNECTED)
            continue; //it has already been validated, but not accepted by program yet.
            
        ret = gnutls_handshake(conn->session);
        if (ret < 0) 
        {
            //non blocking sockets will return these errors untill success
            if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
                continue;
            //NOTE on a rehandshake the following errors may be returned: GNUTLS_E_GOT_APPLICATION_DATA || GNUTLS_E_WARNING_ALERT_RECEIVED
            if (ret == GNUTLS_E_GOT_APPLICATION_DATA)
            {
                LogWarning("Rehandshake occurred, data is pending?");
                continue;
            }
            if (ret == GNUTLS_E_WARNING_ALERT_RECEIVED)
            {
                LogWarning("error: GNUTLS_E_WARNING_ALERT_RECEIVED -- not exactly sure what this means, treating as fatal error");
            }
            //some fatal error happened :(
            LOGOUTPUT << "incoming Handshake error(" << ret << ") : " << gnutls_strerror(ret);
            LogError();
            
            //TODO standardize some error codes
            conn->Disconnect();
            incomingConnections.erase(conn->GetId());
            events->CallError(conn, YAKERR_HANDSHAKE);
            delete conn;
            continue;
        }
        else if (ret == GNUTLS_E_SUCCESS)
        {
            //yayyy TODO on connection callback
            LogInfo("Handshake Successful!!!!");
            conn->status |= CONN_CONNECTED;
        }
    }
}

//pretty close to the UDP connect function
Connection* TCPArmored::Connect(const char* ipAddr, unsigned short port)
{    
    
    //check list to make sure its not already a pending connection
    //i don't care if this one is slower than the incoming map
    sockaddr_key_t key(inet_addr(ipAddr), port);
    TList<TCPArmored *>::iterator iter(outgoingConnections);
    while(!iter.past_end())
    {
        if (iter.item()->GetId() == key)
        {
            LogWarning("TCPArmored::Connect() - connection is already pending, or established");
            return iter.item();
        }
        iter.next();
    }
    
    //run normal tcp connect
    TCPArmored *newConn   = new TCPArmored(events);
    
    memset(&newConn->addr, 0, sizeof(newConn->addr));
    newConn->addr.sin_family = AF_INET;
    newConn->addr.sin_port = htons(port);
    //create the theSocket
#ifdef _WIN32_
    newConn->addr.sin_addr.s_addr = inet_addr(ipAddress);
    newConn->theSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    inet_pton(AF_INET, ipAddr, &newConn->addr.sin_addr);
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
    
    //create the theSocket
    newConn->SetId(newConn->addr.sin_addr.s_addr, newConn->addr.sin_port);
    

    int ret;
    //init gnutls session
    gnutls_init(&newConn->session, GNUTLS_NONBLOCK | GNUTLS_CLIENT);
    //gnutls_anon_allocate_client_credentials(&anoncred);
    //gnutls_priority_set_direct(newConn->session, "PERFORMANCE:+ANON-ECDH:+ANON-DH", NULL);
    //gnutls_credentials_set(newConn->session, GNUTLS_CRD_ANON, anoncred);
    gnutls_certificate_allocate_credentials(&newConn->credentials);
    
    //default priorities
#if defined(FAST_ECDHE)
            ret = gnutls_priority_set_direct(newConn->session, "SECURE128", NULL);
#else
            ret = gnutls_priority_set_direct(newConn->session, "SECURE256:-ECDHE-RSA:-RSA:+DHE-RSA", NULL);
#endif
    if (ret < 0) 
    {
        LOGOUTPUT << "gnutls_priority_set_direct -- error code: " << ret; 
        LogError();
        delete newConn;
        return 0;
    }
    ret = gnutls_credentials_set(newConn->session, GNUTLS_CRD_CERTIFICATE, newConn->credentials);
    if (ret < 0)
    {
        LOGOUTPUT << "gnutls_credentials_set -- error code: " << ret; 
        LogError();
        delete newConn;
        return 0;
    }
    //set sessions socket
    gnutls_transport_set_int(newConn->session, newConn->theSocket);
    gnutls_handshake_set_timeout(newConn->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
    outgoingConnections.PushBack(newConn); //will remain in the queue untill confirmed
    //LogInfo("TCPArmored connection request sent");
    return newConn;
}

void TCPArmored::Disconnect()
{
    if (credentials)
        gnutls_certificate_free_credentials(credentials);
    if (session)
        gnutls_deinit(session);
    TCPConnection::Disconnect();
}

bool TCPArmored::SendDataToAllConnections(char *data, unsigned short eventId)
{
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    for ( ; iter != connections.end(); iter++)
    {
        ((TCPArmored *)iter->second)->SendData(data, eventId);
    }
    return true;
}

//doesnt assigns msg id (internal use for sendtoallconnections)
bool TCPArmored::SendData(char *data, unsigned short eventId)
{
    if (isDropping())
        return false;

    *(unsigned short *)(data + sizeof(unsigned short)) = eventId;
    int size = *(unsigned short *)data;
    if (size > MAX_MSG_SIZE)
        size = MAX_MSG_SIZE;

    //send data to a specific client
    int bytesSent;
    int debug = 0;
    do
    {
        if (debug > 0) //remove this once its determined to not be an issue.
            LogError("GnuTLS E_AGAIN / E_INTERRUPTED");
        bytesSent = gnutls_record_send(session, data, size);
        debug++;
    } //really don't understand why but the docs tell me to do this?
    while (bytesSent == GNUTLS_E_AGAIN || bytesSent == GNUTLS_E_INTERRUPTED);
    //if it blocks a bunch, i'm going to need to buffer sends or multithread???
    
    int sent;
    if (bytesSent < 0)
    {
        LOGOUTPUT << "GnuTLS Send Error code: " << bytesSent;
        LogError();
        DropConnection(this);
        return false;
    }
    

    while (bytesSent < size)
    {
        debug = 0;
        do
        {
            if (debug > 0) //remove this once its determined to not be an issue.
                LogError("GnuTLS E_AGAIN / E_INTERRUPTED");
            sent = gnutls_record_send(session, data, size);
            debug++;
        } //really don't understand why but the docs tell me to do this, so this i shall do!?
        while (sent == GNUTLS_E_AGAIN || sent == GNUTLS_E_INTERRUPTED);
        if (sent < 0)
        { 
            LOGOUTPUT << "GnuTLS Send Error code: " << bytesSent;
            LogError();
            DropConnection(this);
            return false;
        }

        bytesSent += sent;
    }
    return true;
}


/////// RECV FUNCTION /////////
//read into recv buffer, returns total bytes recvd'd
//returns error,  otherwise errno
int TCPArmored::RecvData()
{
    size_t count = buffer.getSize(); //bytes leftover
    int err = 0;
    //maxPacketLength is not the actual packet size, so hopefully more than one will be read at a time
    //in processing stage we will make sure if maxPacketLengthis reached, we have a full packet. or else drop the entire stream(error)
    for (int i = 0; i < MAX_PACKETS_IN_BUFFER; i++) //read the max amount, untill theres nothing left!
    {
        //int size = recv(socket, recvBuffer.data(), maxPacketLength, 0);
        int size = gnutls_record_recv(session, recvBuffer.data(), maxPacketLength);
        if (size < 0) //there was an "error"
        {
            //size = 0; //do not add -1 to an unsigned type that could be 0!
            err = errno;
            i = MAX_PACKETS_IN_BUFFER; // done recv'ing if we got an error
            
            //GnuTLS errors?
            switch (size)
            {
                //think these first 3 equate to "wouldblock"
//                 case 0:
//                     LogError("GnuTLS error 0 = stream EOF???");
//                 break;
                case GNUTLS_E_INTERRUPTED:
                    LogError("E_Interrupted");
                break;
                
                case GNUTLS_E_AGAIN:
                    //LogError("E_AGAIN");
                break;
                
                case GNUTLS_E_REHANDSHAKE:
                    LogError("GNUTLS_E_REHANDSHAKE received");
                    //rehandshake info: http://gnutls.org/manual/html_node/Data-transfer-and-termination.html
                    /*the client may receive an error code of GNUTLS_E_REHANDSHAKE
                     *This message may be simply ignored, replied with an alert GNUTLS_A_NO_RENEGOTIATION , 
                     * or replied with a new handshake, depending on the clients will.*/
                break;
                
                //i don't think errno is getting set correctly through gnutls....
                case GNUTLS_E_INVALID_SESSION:
                case GNUTLS_E_PREMATURE_TERMINATION:
                    LogError("session dropped");
                    err = -1;
                break;
                
                default:
                    LOGOUTPUT << "GnuTLS recv error code: " << size;
                    LogError();
                break;
            }
            
            size = 0; //dont copy anything!
            
            //NOTE windows is going to need some WSA junk
            switch(err)
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
                    LOGOUTPUT << "recv() - error code: " << err;
                    LogError();
                break;
            }
            if (err != 0)
                err = -err;
        }  
        else if (size == 0)
        {
            i = MAX_PACKETS_IN_BUFFER; //no more looping!
        }
        if (err < 0)
            LogError("recv_record() - gracefully disconnected todo: handle me!");
       
        //copy 
        if (!buffer.Write(recvBuffer.data(), size))
            LogError("TCPBuffer full, dropping stream data, whole buffer will likely be dropped next. increase bufer size!!");
        else
            count += size;
    }

    size_t bytesToProcess = count;
    size_t processed = 0; //avoid confusing maths

    //normal TCP, no extra websocket header to process

    //recv'd from the stream, now packetize and populate message queue, reset buffer when done!
    if (count < sizeof(short))//just incase... :[
        return err;

    while (bytesToProcess)
    {
        
        unsigned short msgSize = *(unsigned short *)(buffer.getDataPtr() + processed);
        
        //this is very very bad...
        if (msgSize > maxPacketLength)
        {
            buffer.Clear();//super paranoid code would probably drop the whole message queue, maybe memset 0?
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
            LOGOUTPUT << "clearbing before whole packet has been recvd - bytesto: " << bytesToProcess << 
            " | msgSize " << msgSize;
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
