/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "UDPConnection.h"
#include "NetEvents.h"
#include "Yaknet/Utilities/StaticTimer.h"
#include "Yaknet/Utilities/Logger.h"

UDPConnection::UDPConnection(NetEvents *netEvents) : Connection(netEvents)
{
    Init(MAX_MSG_SIZE);
    numConnAttempts = 0;
    lastActivity = 0;
    handshakeStage = 0;
}

UDPConnection::~UDPConnection(){}
NetBuffer nbRequest(128);
void UDPConnection::SendConnectionRequest(unsigned int sessionKey)
{
    lastActivity = StaticTimer::GetInstance()->GetTimestamp();
    numConnAttempts++;
    
    unsigned short eventId;
    if (handshakeStage == 1)
    {
        LogInfo("sending UDP connection request...");
        eventId = (unsigned short)UDP_CONN_REQUEST;
    }
    else if (handshakeStage == 2) //when connected, this will remain as 2 incase a retranmission is needed
    {
        LogInfo("sending UDP validation request...");
        eventId = (unsigned short)UDP_CONN_VALIDATE;
    }
    else
    {
        LOGOUTPUT << "error: invalid handshake stage?: " << (int)handshakeStage;
        LogError();
        handshakeStage = 0;
        return;
    }
    nbRequest.WriteShort(eventId);
    nbRequest.Prepare();
    SendData(nbRequest.GetBuffer(), eventId);
    
    
}

void UDPConnection::CheckHandshake()
{
    //go through pending outgoing connections and resend requests if needed
    //if this list is too long, this function may be pretty slow (brute force linked list traversal)
    time_t stamp = StaticTimer::GetInstance()->GetTimestamp();
    TList<UDPConnection *>::iterator iter(outgoingConnections);
    while(!iter.past_end())
    {
        UDPConnection *conn = iter.item();
        time_t delta = (stamp - conn->lastActivity);
        if (delta > CONN_REQ_TIMEOUT)
        {
            if (conn->numConnAttempts > CONN_REQ_ATTEMPTS)
            {
                //timed out too many times
                connections.erase(conn->GetId());
                delete conn;
                outgoingConnections.PopAt(iter);
                iter.previous();
            }
            //TODO session key...
            conn->SendConnectionRequest(0);
            
        }
        iter.next();
    }
}

void UDPConnection::BeginTick()
{
    //if not connected check for server connection ack
    if (outgoingConnections.size() > 0)
        CheckHandshake(); //handles retransmission

        
    //check for stale connections, an optimization may be to only check 10 or so connections per tick
    //and continue checking the next 10, next tick, but for now check em all.
    time_t stamp = StaticTimer::GetInstance()->GetTimestamp();
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    while (iter != connections.end())
    {
        if (iter->second->isConnected() && stamp - ((UDPConnection *)iter->second)->lastActivity > CONN_TIMEOUT)
        {
            LogError("connection timed out");
            DropConnection(iter->second);
        }
        iter++;
    }
        
    //process connection drops, deletes
    ProcessDrops();

}

//client is requesting connection
void UDPConnection::HandleConnectionRequest(sockaddr_in sender, char* data, short unsigned int size)
{
    //LogInfo("AND! was a connection request");
    sockaddr_key_t key(sender.sin_addr.s_addr, sender.sin_port);
    UDPConnection *conn;
    std::map<sockaddr_key_t, UDPConnection *>::iterator iter = incomingConnections.find(key);
    if (iter == incomingConnections.end()) //this could be a retransmission so it may already exist in list
    {
        LogInfo("new incoming connection");
        conn = new UDPConnection(events);
        memcpy(&conn->addr, &sender, sizeof(addr));
        //conn->addr = sender;
        conn->id.Set((unsigned int)conn->addr.sin_addr.s_addr, (unsigned int)conn->addr.sin_port); //AddConnection normally does this
        conn->theSocket = theSocket; //use the listening socket to send on
        incomingConnections[key] = conn;
    }
    else
    {
        LogInfo("incoming connection already existed");
        conn = iter->second;
    }
    //send ack
    NetBuffer nb(6);
    nb.WriteShort((short)UDP_CONN_ACK);
    nb.Prepare();
    conn->SendData(nb.GetBuffer(), (short)UDP_CONN_ACK);
}

//final step, client requesting connection confirmation, if the client sends this again
//we must retransmit the response, because the packet may have been dropped
void UDPConnection::HandleValidationRequest(sockaddr_in sender, char* data, short unsigned int size)
{
    LogInfo("Got Validation!!!!");
    //add the validated connection to our list
    sockaddr_key_t key(sender.sin_addr.s_addr, sender.sin_port);
    std::map<sockaddr_key_t, UDPConnection *>::iterator iter = incomingConnections.find(key);
    UDPConnection *conn = 0;
    if (iter != incomingConnections.end())
    {
        conn = iter->second;
        conn->status |= CONN_CONNECTED; //connected, aka validated.
        //connection gets added finally in acceptconnection
    }
    else
    {
        //client retransmitted, our ack must have been dropped, it should be in connections list...
        std::map<sockaddr_key_t, Connection *>::iterator cIter = connections.find(key);
        if (cIter == connections.end())
        {
            LogWarning("UDPConnection::HandleValidationRequest() - retransmission has occured, and connection was not found in connections list!?");
            return;
        }
        conn = (UDPConnection *)cIter->second;
    }
    //send ack
    NetBuffer nb(6);
    nb.WriteShort((short)UDP_CONN_ACK);
    nb.Prepare();
    conn->SendData(nb.GetBuffer(), (short)UDP_CONN_ACK);
}

//an ack sent from a host connection
void UDPConnection::HandleConnectionAck(sockaddr_in sender, char* data, short unsigned int size)
{
    //use a blacklist if you're worried about someone sending fake handshake responses?
    sockaddr_key_t key(sender.sin_addr.s_addr, sender.sin_port);

    //find  the connection in liste
    TList<UDPConnection *>::iterator iter(outgoingConnections);
    UDPConnection *conn = 0;
    while (!iter.past_end())
    {
        if (iter.item()->GetId() == key)
        {
            conn = iter.item();
            break;
        }
        iter.next();
    }
    if (conn == 0)
    {
        LogWarning("someone sent an ack, but is not in pending connections list.");
        return;
    }
    //check handshake state and respond accordingly...
    if (conn->handshakeStage == 1)
    {
        //LogInfo("-->handshake stage 1");
        conn->handshakeStage = 2;
        numConnAttempts = 0;
        conn->SendConnectionRequest(0);
    }
    else if (conn->handshakeStage == 2)
    {
        
        //verify it doesnt already exist...  even though it should never happen, take check out later?
        if (connections.find(key) != connections.end())
        {
            LogWarning("connection already exists, wtf happened....?");
            return;
        }
        //LogInfo("-->handshake stage 2");
        conn->status |= CONN_CONNECTED;
        AddConnection(conn);
        outgoingConnections.PopAt(iter);
        conn->lastActivity = StaticTimer::GetInstance()->GetTimestamp(); //from this point out, clock only updates on recv's
        //we're validated now!
        LogInfo("connection validated");
    }
    else
    {
        //LOGOUTPUT << "-->handshake stage: " << (int)conn->handshakeStage;
        //LogWarning();
        LogInfo("received ack, but we're already connected??");
    }
}


Connection *UDPConnection::Connect(const char *ipAddress, unsigned short port)
{
    //udp connections must be listening so we can receive responses
    if (!(status & CONN_LISTENING))
    {
        LogError("UDPConnection::Connect must be listening before it can connect.");
        return 0;
    }

    
    sockaddr_in new_addr;
    memset(&new_addr, 0, sizeof(new_addr));
#ifdef _WIN32_
    new_addr.sin_addr.s_addr = inet_addr(ipAddress);
#else
    inet_pton(AF_INET, ipAddress, &new_addr.sin_addr);
#endif
    new_addr.sin_port = htons(port);

    
    //check list to make sure its not already a pending connection
    sockaddr_key_t key(new_addr.sin_addr.s_addr, new_addr.sin_port);
    TList<UDPConnection *>::iterator iter(outgoingConnections);
    while(!iter.past_end())
    {
        if (iter.item()->GetId() == key)
        {
            LogWarning("UDPConnection::Connect() - connection is already pending, or established");
            return iter.item();
        }
        iter.next();
    }
    
    //connections sockets(theSocket) are for listening, unless created by connecting like so
    UDPConnection *newConn   = new UDPConnection(events);
    memcpy(&newConn->addr, &new_addr, sizeof(sockaddr_in));
    //newConn->addr.sin_addr   = new_addr.sin_addr;
    //newConn->addr.sin_port   = new_addr.sin_port;
    //newConn->addr.sin_family = AF_INET;
    
    //create the theSocket
    newConn->theSocket = theSocket; //use the listening socket to send on

    //set address
    //memcpy(&newConn->addr, &new_addr, sizeof(addr));
    //newConn->addr = new_addr;

    
    // TODO  connection logic, send connection messages, and retransmit if no response, fail after x amount
    newConn->id.Set(new_addr.sin_addr.s_addr, new_addr.sin_port);
    outgoingConnections.PushBack(newConn); //will remain in the queue untill confirmed
    //could technically just start communicating, but i would like to authenticate connections
    //BeginTick() will wait for connection ack, if it fails it will remove the connection
    newConn->handshakeStage = 1;
    newConn->SendConnectionRequest(0);
    //if you need faster connection processing, use a list? it should time out in a few seconds though
    //i dunno how
    
    //we must add this connection to the list so process events knows about it!
    
    //newConn->status |= CONN_CONNECTED; its not connected untill server confirms
    LogInfo("UDP \'connection\' request sent");

    return newConn;
}

//searches incoming list for validated(connected) status, transfers into connection list, and returns connection
Connection* UDPConnection::AcceptConnection()
{
    std::map<sockaddr_key_t, UDPConnection *>::iterator iter = incomingConnections.begin();
    for ( ; iter != incomingConnections.end(); iter++)
    {
        //check status
        if (iter->second->status & CONN_CONNECTED)
        {
            UDPConnection *conn = iter->second;
            AddConnection(conn);
            incomingConnections.erase(conn->GetId());
            conn->lastActivity = StaticTimer::GetInstance()->GetTimestamp(); //update time stamp
            return conn;
        }
    }
    return 0;
}

//NOTE udp connections are sharing sockets, can't just close all willy nilly-like
void UDPConnection::Disconnect()
{

    //remove from dropList next tick
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

void UDPConnection::DropConnection(Connection* conn)
{
    if (((UDPConnection *)conn)->GetId() == GetId())
    {
        LogInfo("Dropping (master) connection");

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

        Disconnect(); // this connection was dropped
    }
    else
    {
        //simply close the theSocket and remove it from the connection list
        std::map<sockaddr_key_t, Connection *>::iterator iter = connections.find(conn->GetId());
        if (iter != connections.end())
        {
            LOGOUTPUT << "Dropping connection id = " << conn->GetId().GetData();
            LogInfo();
            iter->second->Disconnect(); //tomahawk it
            dropList.PushBack(iter->second); //queue for deletion
        }
    }
}

//TODO error checking
bool UDPConnection::SendData(char* data, short unsigned int eventId)
{
    //yeah i know that sending the size again is redundant...
    unsigned short size = *(unsigned short *)data;
    *(unsigned short *)(data + sizeof(unsigned short)) = eventId;
    int r = sendto(theSocket, data, size, 0, (struct sockaddr *)&addr, sizeof(addr));
//     LOGOUTPUT << "SendData() - bytes sent: " << r;
//     LogInfo();
    
    if (r < 0)
        LogError("UDPConnection::SendData - TODO: error checking, something weird happened");
    
    return (r < 0) ? false : true;
}

bool UDPConnection::SendDataToAllConnections(char* data, short unsigned int eventId)
{
    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    for ( ; iter != connections.end(); iter++)
    {
        ((UDPConnection *)iter->second)->SendData(data, eventId);
    }
    return true; //TODO error checking......  on other connection types too ................... !
}

int UDPConnection::RecvFromAllConnections()
{
    if (!(status & CONN_LISTENING))
        return -1;
    int err, size;
    int recvBytes = 0;
    sockaddr_in sender;
    unsigned int addrlen = sizeof(sender);
    bool receive = true;
    while(receive)
    {
        memset(&sender, 0, sizeof(sender)); //am i paranoid for this?
        size = recvfrom(theSocket, recvBuffer.data(), maxPacketLength, 0, (sockaddr *)&sender, &addrlen);
        if (size < 0) //there was an "error"
        {
            size = 0;
            err = errno;
            //windows is going to need some WSA junk  eg: if (WSAGetLastError() == WSAEWOULDBLOCK)
            switch(errno)
            {
                case EWOULDBLOCK:   //nothing to read,
                    receive = false;//break out of the loop.
                    continue;
                break;
                case EBADF :
                    LogError("recv() - bad file descriptor");
                break;
                default :
                    LogError("recv() - error");
                break;
            }
            Disconnect();
            return -1;
        }
        else //packet received, process it.
        {
            recvBytes += size;
            unsigned short msgSize = *(unsigned short *)recvBuffer.data();
//          LOGOUTPUT << "msgSize: " << msgSize << "\nsize: " << size;
//          LogInfo();
            //verify message size for paranoia sake
            if (size != msgSize || size < 4)
            {
                
                /*LogError("UDPConnection::Recv - msgSize error, dropping packet");
                LOGOUTPUT << "msgSize: " << msgSize << "\nsize: " << size;
                LogInfo();*/
                events->CallError(this, YAKERR_CORRUPTION);
                continue;
            }
            sockaddr_key_t key(sender.sin_addr.s_addr, sender.sin_port);
            //check event to see if its a handshake packet
            unsigned short eventId = *((unsigned short *)&(recvBuffer.data()[2]));
            if (eventId == UDP_CONN_REQUEST) //sending connection request
                HandleConnectionRequest(sender, recvBuffer.data(), msgSize);
            else if (eventId == UDP_CONN_VALIDATE) //sending validation
                HandleValidationRequest(sender, recvBuffer.data(), msgSize);
            else if (eventId == UDP_CONN_ACK)  //ack from host
                HandleConnectionAck(sender, recvBuffer.data(), msgSize);
            else //not a handshake packet, package the event for processing
            {
                //get senders connection if it exists
                std::map<sockaddr_key_t, Connection *>::iterator iter = connections.find(key);
                if (iter != connections.end())
                {
                    //this sender is an active connection,
                    NetBuffer *nb = new NetBuffer(msgSize);
                    nb->FillBuffer(recvBuffer.data(), msgSize);
                    //let processmessages worry about verifying the message
                    ((UDPConnection *)iter->second)->lastActivity = StaticTimer::GetInstance()->GetTimestamp();
                    iter->second->messageQueue.PushBack(nb);
                }
                else
                {
                    //its not connected, we could log, or do some other stuff here
                    events->CallError(this, YAKERR_NOTCONNECTED);
                }
            }
        }
    }
    
    return recvBytes;

}

