/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Yaknet/NetEvents.h"
#include "Yaknet/WebSockets.h"
#include "Yaknet/Utilities/Logger.h"
#include <iostream>
#include <unistd.h>
#include "Yaknet/Utilities/console_stuff.h"

using namespace std;
NetEvents *netEvents = 0;
WebSockConnection *masterConn;
Connection *newConn;

void Server(short port);
void Client(const char *ip, short port);

void cl_echo(Connection *netId, void *data)
{
    //client echo event,  print data to screen
    NetBuffer *nb = (NetBuffer *)data;
    //nb->SetPosition(4);
    LOGOUTPUT << "echo:    " << nb->ReadByte();
    LogInfo();    
}

void sv_echo(Connection * netId, void *data)
{
    //server echo event, relay data back to client
    NetBuffer *nb = (NetBuffer *)data;
    //nb->SetPosition(4);
    LOGOUTPUT << "hollaback: sv_echo(" << netId << ") - " << nb->ReadByte();
    LogInfo();
    
    newConn->SendData(nb->GetBuffer(), 0);
}

static bool client_disconnected = false;

void cl_Disconnect(Connection *netId, void *data)
{
   client_disconnected = true;
}
int main()
{
    
    LogInfo("Launching Echo Test");
    NetEvents ne;
    netEvents = &ne;

    if (!netEvents->Init(1))
    {
        LogError("could not initialize networking system.");
        return -1;
    }
    
    LogInfo("run (s)erver or (c)lient ?");
    char c = getch();

    if (c == 'c')
        Client("0.0.0.0", 1337);
    else
        Server(1337);

    return 0;
}



void Server(short port)
{
    LogInfo("starting echo server...");
    WebSockConnection *conn = new WebSockConnection(netEvents);
    masterConn = conn;
    WebSockConnection *client = 0;
    conn->Listen(port);

    char packet[MAX_MSG_SIZE] = {0};
    char *pData = packet;
    int nSize;

    //register echo callback
    netEvents->RegisterNetEvent(0, sv_echo);

    Connection *accepted;
    while(1)
    {
        conn->BeginTick();
        accepted = conn->AcceptConnection();
        if (accepted)
            newConn = accepted;
        conn->RecvFromAllConnections();
        conn->ProcessEvents();
            
        usleep(4000);

    }
}

void Client(const char *ip, short port)
{
    netEvents->RegisterDisconnectEvent(cl_Disconnect);
    LogInfo("starting client");
    WebSockConnection *conn = new WebSockConnection(netEvents);
    Connection *toServer = conn->Connect(ip, port);

    if (!toServer)
        LogInfo("Could not connect to websocket server");
    else
        LogInfo("Websocket connection established, upgrade negotiated");

    int nBytes = 0;
    char packet[MAX_MSG_SIZE] = {0};
    //char *pData = packet;
    char buff[4];
    cout << endl << endl;

    //register echo callback
    netEvents->RegisterNetEvent(0, cl_echo); //only 1 event, id: 0

    NetBuffer nb(MAX_MSG_SIZE);
    
    while(!client_disconnected) //if using recvFromAllConnections, will need disconnect event callback
    {
        if (kbhit())
        {
            char data = getch();
            cout << "sending server char(" << data << ")\n";

            nb.SetPosition(0);
            nb.WriteShort(5);//TODO can remove this and let the connection manage it
            nb.WriteShort(0);
            nb.WriteByte(data);
           
           if ( !toServer->SendData(nb.GetBuffer(), 0))
                LogInfo("send failed");

            nb.ClearBuffer();
               
        }

        //conn->RecvFromAllConnections(); //this works too
        if (!toServer->RecvData())
            return;

        //events go to containing connections
        conn->ProcessEvents();
         
        usleep(4000);
    }
}
