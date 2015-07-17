/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "WebSockets.h"
#include <ctype.h>
#include <stdio.h>
#include "Yaknet/Utilities/sha1.h"
#include "Yaknet/Utilities/b64_ntop.h"
#include "Yaknet/Utilities/Logger.h"
//TODO  generate keys instead of re-using the same one...
static const char *WSGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
#define HYBI10_ACCEPTHDRLEN 29

#define CLIENT_HANDSHAKE_HYBI "GET /servicename HTTP/1.1\r\n\
       Host: server.example.com\r\n\
       Upgrade: websocket\r\n\
       Connection: Upgrade\r\n\
       Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\
       Origin: http://example.com\r\n\
       \r\n"

#define SERVER_HANDSHAKE_HYBI "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
\r\n"

// will first return any pending connections that completed handshake
// if no pending connections, check if someones trying to connect
//call this every frame or so, TODO or perhaps provide a callback for new connections??
Connection *WebSockConnection::AcceptConnection()
{
    if (!(status & CONN_LISTENING) || status & CONN_CONNECTED )
    {
        LogError("cannot accept new connections on this socket.");
        return 0;
    }

    int handshake;
    for (int i = 0; i < pendingConnections.size(); )
    {
            handshake = pendingConnections[i]->TryHandshake();
            if (handshake == 1)
            {
                Connection *c = pendingConnections[i];
                pendingConnections.erase(pendingConnections.begin()+i);
                LogInfo("handhake success!");
                //connections.push_back(c);
                //connections[c->GetId()] = c;
                AddConnection(c);
                return c;
            }
            else if (handshake < 0)
            {
                #ifdef _WIN32_
                    closesocket(theSocket);
                #else
                    int err = shutdown(pendingConnections[i]->theSocket, SHUT_RDWR);
                #endif
                pendingConnections.erase(pendingConnections.begin()+i);
                LogWarning("socket closed, failed handshake.");
            }

            i++;
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
            LogError("theSocket(): Invalid Socket - ");
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
        WebSockConnection *conn = new WebSockConnection(events, newSocket, newAddr);
        --numReadSets;

        //add the connection to our list
        //connections.push_back(conn);
        pendingConnections.push_back(conn); //make sure we get a handshake!
        conn->status |= CONN_CONNECTED ;

        LogInfo("connection pending");
    }

    //no new connections
    return 0;

}
#define ISspace(x) isspace((int)(x))
//from simple http server,
//TODO this needs to be optimized?
// returns string with \n\0 at the end
int get_line(int sock, char *buf, int size)
{
 int i = 0;
 char c = '\0';
 int n;

 while ((i < size - 1) && (c != '\n'))
 {
  n = recv(sock, &c, 1, 0);
  /* DEBUG printf("%02X\n", c); */
  if (n > 0)
  {
   if (c == '\r')
   {
    n = recv(sock, &c, 1, MSG_PEEK);
    /* DEBUG printf("%02X\n", c); */
    if ((n > 0) && (c == '\n'))
     recv(sock, &c, 1, 0);
    else
     c = '\n';
   }
   buf[i] = c;
   i++;
  }
  else
   c = '\n';
 }
 buf[i] = '\0';
 
 return(i);
}

int WebSockConnection::TryHandshake()
{
    char buff[1024];
    char method[255];
    int i, j;
    int numChars = get_line(theSocket, buff, sizeof(buff));

    if (numChars <= 0)
    {
        if (errno != EWOULDBLOCK)
            return -1;
        else
            return 0;
    }

    i=0;
    j=0;
    while (!ISspace(buff[j]) && (i < sizeof(method) - 1))
    {
        method[i] = buff[j];
        i++; j++;
    }
    method[i] = '\0';

    //received a GET, make sure its a websockets handshake.
    if (strcasecmp(method, "GET") == 0)
    {
        char secKey[32];
        memset(secKey, 0, sizeof(secKey));
        LogInfo("Step 1 accomplished,  we GET http signal!");
        LogInfo("http header:");
        bool gotKey = false;
        while ((numChars > 0) && strcmp("\n", buff))
        {
            numChars = get_line(theSocket, buff, sizeof(buff));
            LOGOUTPUT << buff;
            char *keyPtr = strstr(buff, "Sec-WebSocket-Key:");
            if (keyPtr)
            {
                keyPtr += 19; //go to the start of key
                unsigned int len = strlen(buff);
                char *end = strstr(buff, "\n");
                if (end == 0)
                    LogError("bad http header");
                //damn haxors get off my lawn
                if (end - keyPtr > sizeof(secKey) || len < 20+24)
                {
                    LogError();
                    return -1;
                }
                if ((keyPtr - buff) + (len - 20)  > sizeof(buff))
                {
                    LogError();
                    return -1;
                }
                memcpy(secKey, keyPtr, end-keyPtr);
                gotKey = true;
                break;
            }

        }
        LogInfo();

        if (!gotKey)
            return -1;

        LOGOUTPUT << "secKey: (" << secKey << ")";
        LogInfo();

        //prepare response
        char hashIn[64];
        unsigned char hashOut[21];
        //const char *tempCheck = "dGhlIHNhbXBsZSBub25jZQ==";
        //should respond with: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
        int sizeA = strlen(secKey);

        memcpy(hashIn, secKey, sizeA);
        memcpy(hashIn+sizeA, WSGUID, strlen(WSGUID));
        hashIn[sizeA+strlen(WSGUID)] = 0;
        sha1::calc(hashIn, strlen(hashIn), hashOut);
        char b64Out[HYBI10_ACCEPTHDRLEN];
        b64_ntop(hashOut, SHA_DIGEST_LENGTH, b64Out, HYBI10_ACCEPTHDRLEN);
        LOGOUTPUT << "Hash Out: " << b64Out;
        LogInfo();

        char response[1024];
        sprintf(response, SERVER_HANDSHAKE_HYBI, b64Out);
        LOGOUTPUT << "sending response: \n" << response;
        LogInfo();

        //clear the recv buffer foo
        while (get_line(theSocket, buff, sizeof(buff)));

        send(theSocket, response, strlen(response), 0);//b64 should be null terminated

        return 1;
    }
    else
        return 0;
}


bool WebSockConnection::PrepareWebSocketHeader(char *data)
{
    unsigned short size = (*(unsigned short *)data) - 2;

    headerSize = 0;
    if (size < 126)
        headerSize = 2;
    else if (size >= 126)
        headerSize = 4;
    else if (size > 65535)
    {
        LogInfo("SendData() 64bit payload size is not supported");
        return false;
    }
    bool useMask = !toClient;
    if (useMask) //we are not server
        headerSize+=4; //mask

    if (size + headerSize > MAX_MSG_SIZE)
    {
        LogWarning("SendData() websocket packet will exceede MAX_MSG_SIZE, not sending.");
        return false;
    }

    unsigned short mainHeader = 0;

    WSFLAG_SET_FIN(mainHeader);
    WSFLAG_SET_OPCODE(mainHeader, 2); //2 is data, always use data
    WSFLAG_SET_PAYLOADSIZE(mainHeader, size < 126 ? size : 126);
    //LOGOUTPUT << "PAYLOAD SIZE SHOULD BE : " << size;
    //LogInfo();

    if (useMask)
    {
        //LogInfo("Setting mask");
        WSFLAG_SET_MASK(mainHeader);
        mask = rand(); //yeah, not the most secure i guess? w/e
    }
    else
        mask = 0;

    //endianness kills me
    header[0] = htons(mainHeader);

    if (size >= 126)//use an extra 2 bytes for size?
    {
        header[1] = htons(size); //size goes in right after main header
        memcpy(copyBuff, header, 4);
    }
    else
        memcpy(copyBuff, header, 2); //htons???

    return true;
}

//needs to append websocket protocol data
bool WebSockConnection::SendData(char *data, unsigned short eventId)
{
    if (isDropping())
        return false;

    //fill out header
    if (!PrepareWebSocketHeader(data))
        return false;

    unsigned short payloadSize = *(unsigned short *)data;
    //we wrote header, and validated sizes, all good to memcpy
    payloadSize -= 2; //stripping size from beginning (its in websock header now)
    memcpy(copyBuff+headerSize, data+2, payloadSize); //+2 skip over old size

    //LOGOUTPUT << "PAYLOAD SIZE: " << payloadSize << "\nHEADER SIZE: " << headerSize;
    //LogInfo();
    *(unsigned short *)(copyBuff + headerSize) = eventId;
    int size = headerSize+payloadSize;
    if (size > MAX_MSG_SIZE)
        size = MAX_MSG_SIZE;

    char *payload = copyBuff+headerSize;
    if (mask)
    {
        //LogInfo("send is masking..");
        char *_mask = (char *)&mask;
        
        for (int i = 0; i < payloadSize; i++)
            payload[i] ^= _mask[i%4];
        //not same endiannes as header??
        //NOTE size may be different when you get to shorts...
        mask = /*htonl*/(mask);
        memcpy(copyBuff+(headerSize-4), &mask, 4);
    }

    //LOGOUTPUT << "header getting sent: " << *(unsigned short*)copyBuff
    //<< "\nsize: " << size;
    //LogInfo();
    //send data to a specific client
    int bytesSent = send(theSocket, copyBuff, size, 0);
    int sent;
    if (bytesSent == SOCKET_ERROR)
    {
        LogError("send(): Socket Error - ");
        DropConnection(this);
        return false;
    }

    while (bytesSent < size)
    {
        sent = send(theSocket, copyBuff, size, 0);
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


bool WebSockConnection::SendDataToAllConnections(char *data, unsigned short eventId)
{
    //fill out header
    if (!PrepareWebSocketHeader(data))
      return false;

      unsigned short payloadSize = *(unsigned short *)data;
    //we wrote header, and validated sizes, all good to memcpy
    payloadSize -= 2; //stripping size from beginning (its in websock header now)
    *(unsigned short *)(data + 2) = eventId;
    memcpy(copyBuff+headerSize, data+2, payloadSize); //+2 skip over old size

    //LOGOUTPUT << "PAYLOAD SIZE: " << payloadSize << "\nHEADER SIZE: " << headerSize;
    //LogInfo();

    
    int size = headerSize+payloadSize;

    std::map<sockaddr_key_t, Connection *>::iterator iter = connections.begin();
    for ( ; iter != connections.end(); iter++)
    {
        WebSockConnection *conn = (WebSockConnection *)(iter->second);
        conn->SendPreparedData(copyBuff, size);
    }
    return true;
}


Connection *WebSockConnection::Connect(const char *ipAddr, unsigned short port)
{
    if (status & CONN_LISTENING)
    {
        LogError("cannot connect(), this connection is listening.");
        return 0;
    }

    //connections sockets are for listening, unless created by connecting like so
    WebSockConnection *newConn = new WebSockConnection(events);
    memset(&newConn->addr, 0, sizeof(newConn->addr));
    newConn->addr.sin_family = AF_INET;
    newConn->addr.sin_port = htons(port);
    //create the theSocket
#ifdef _WIN32_
    newConn->addr.sin_addr.s_addr = inet_addr(ipAddr);
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
        //MessageBox(0, "cannot connect to server", "network error! ", MB_OK);
        char errString[128] = "Connect(): Socket Error - ";
        //strcat(errString + strlen(errString), NET_GetErrorString());
        LOGOUTPUT << "Connect() :  socket errno(" << errno << ")";
        LogError();
        delete newConn;
        return 0;
    }

    //set to NON blocking after we connect, otherwise we get EINPROGRESS
    if (!SetSocketBlockingEnabled(newConn->theSocket, false))
        LogError("COULD NOT SET NON-BLOCKING SOCKET");

    

    //----  extra web socket handshake -----

    //connected, send upgrade request, and wait for response
    //TODO add a timeout so it doesnt hang

    char upgrade[MAX_MSG_SIZE];
    strcpy(upgrade, CLIENT_HANDSHAKE_HYBI);
    LOGOUTPUT << "sending upgrade request: \n" << upgrade;
    LogInfo();

    if (!send(newConn->theSocket, upgrade, strlen(upgrade)+1, 0)) //add null!
    {
        delete newConn;
        return 0;
    }

    //now wait untill handshake confirmation
    //TODO this isa very simple recieve and may fail under certain circumstances
    bool shaking = true;
    int size = 0;
    while(1) //TODO add something to break this loop
    {
        size += recv(newConn->theSocket, upgrade, MAX_MSG_SIZE, 0);
        if (size < 0) //there was an "error"
        {
            size = 0; //do not add -1 to an unsigned type that could be 0!
            if (errno != EWOULDBLOCK)
            {
                LogError("Connection() socket error after sending handshake");
                delete newConn;
                return 0;
            }
            continue;
        }
        //we got something. TODO handle partial recv ? :(
        if (size > 0)
            break;
    }

    //verify handhskae, blablabla
    //no verification is good enough for this hacky test
    
    //connections.push_back(newConn);
    //connections[newConn->GetId()] = newConn;
    AddConnection(newConn);
    newConn->status |= CONN_CONNECTED;
    newConn->toClient = false; //to server! (will be masked)
    LogInfo("websocket connected...");
    return newConn;
}
