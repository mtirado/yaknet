/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Yaknet/NetEvents.h"
#include "Yaknet/Utilities/Logger.h"
#include <iostream>
#include <unistd.h>
#include "Yaknet/UDPConnection.h"
#include <stdlib.h>
#include "Yaknet/Utilities/console_stuff.h"
#include "Yaknet/Utilities/StaticTimer.h"

using namespace std;
NetEvents *netEvents = 0;
UDPConnection *masterConn;

void Server(short port);
void Client(const char *ip, short port);

void cl_echo(Connection *netId, void *data)
{
    //client echo event,  print data to screen
    NetBuffer *nb = (NetBuffer *)data;
    nb->SetPosition(4);
    LOGOUTPUT << "echo:    " << nb->ReadByte();
    LogInfo();
}

void sv_echo(Connection * netId, void *data)
{
    //server echo event, relay data back to client
    NetBuffer *nb = (NetBuffer *)data;
    //nb->PrintBytes();
    LOGOUTPUT << "hollaback: sv_echo(" << netId << ") - " << nb->ReadByte();
    LogInfo();
    
    netId->SendData(nb->GetBuffer(), 0);
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
        Client("127.0.0.1", 1337);
    else
        Server(1337);

    return 0;
}



void Server(short port)
{
    LogInfo("starting echo server...");
    UDPConnection *conn = new UDPConnection(netEvents);
    masterConn = conn;
    UDPConnection *client = 0;
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
            cout << "\n\n*new client accepted*\n\n";
        if (conn->RecvFromAllConnections() < 0)
            break;
        conn->ProcessEvents();

        usleep(4000);

    }
}

void Client(const char *ip, short port)
{
    netEvents->RegisterDisconnectEvent(cl_Disconnect);
    UDPConnection *conn = new UDPConnection(netEvents);
    unsigned int seed = StaticTimer::GetInstance()->GetTimestamp();
    srand(seed);
    unsigned short lPort = 10000 + rand()%40000;
    LOGOUTPUT << "starting client on port:" << lPort;
    LogInfo();
    if (!conn->Listen(lPort)) //all master connections need to listen, this creates the socket
        return;
    Connection *toServer = conn->Connect(ip, port);
    if (!toServer)
    {
        LogError("unable to connect");
        return;
    }

    cout << endl << endl;

    //register echo callback
    netEvents->RegisterNetEvent(0, cl_echo); //only 1 event, id: 0

    NetBuffer nb(16);

    //pick a random port
    

    while(1) //if using recvFromAllConnections, will need disconnect event callback
    {
        conn->BeginTick(); //runs handshake logic
        if (kbhit())
        {
            char data = getch();
            cout << "sending server char(" << data << ")\n";

            nb.ClearBuffer();
            nb.WriteByte(data);
            nb.Prepare();
           if ( !toServer->SendData(nb.GetBuffer(), 0))
                LogInfo("send failed");

            nb.ClearBuffer();

        }

        if(conn->RecvFromAllConnections() < 0)
            return;
        
        //events go to containing connections
        conn->ProcessEvents();

        usleep(4000);
    }
}
