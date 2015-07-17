/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Yaknet/NetEvents.h"
#include "Yaknet/Utilities/Logger.h"
#include <iostream>
#include <unistd.h>
#include "Yaknet/TCPConnection.h"
#include "Yaknet/Utilities/console_stuff.h"

using namespace std;
TCPConnection *masterConn;

void Server(short port);
void Client(const char *ip, short port);

static bool client_disconnected = false;

void cl_echo(Connection *netId, void *data)
{
    //client echo event,  print data to screen
    NetBuffer *nb = (NetBuffer *)data;
    nb->SetPosition(4);
    LOGOUTPUT << "echo:    " << nb->ReadByte();
    LogInfo();    
}

void cl_Disconnect(Connection *netId, void *data)
{
    client_disconnected = true;
}


void sv_echo(Connection * netId, void *data)
{
    //server echo event, relay data back to client
    NetBuffer *nb = (NetBuffer *)data;
    //nb->SetPosition(2);
    LOGOUTPUT << "hollaback: sv_echo(" << netId << ") - " << nb->ReadByte();
    LogInfo();
    netId->SendData(nb->GetBuffer(), 0);
}


int main()
{
    
    LogInfo("Launching Echo Test");
        
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
    NetEvents events;
    events.Init(1);
    LogInfo("starting echo server...");
    TCPConnection *conn = new TCPConnection(&events);
    masterConn = conn;
    TCPConnection *client = 0;
    conn->Listen(port);

    char packet[MAX_MSG_SIZE] = {0};
    char *pData = packet;
    int nSize;

    //register echo callback
    events.RegisterNetEvent(0, sv_echo);

    Connection *accepted;
    while(1)
    {
        conn->BeginTick();
        accepted = conn->AcceptConnection();
        if (accepted)
            cout << "\n\n*new client accepted*\n\n";
        conn->RecvFromAllConnections();
        conn->ProcessEvents();
            
        usleep(4000);

    }
}

void Client(const char *ip, short port)
{
    //netEvents->RegisterDisconnectEvent(cl_Disconnect);
    LogInfo("starting client");
    NetEvents events;
    events.Init(2);
    TCPConnection *conn = new TCPConnection(&events);
    Connection *toServer = conn->Connect(ip, port);

    cout << endl << endl;

    //register echo callback
    events.RegisterNetEvent(0, cl_echo); //only 1 event, id: 0

    NetBuffer nb(MAX_MSG_SIZE);
    
    while(1) //if using recvFromAllConnections, will need disconnect event callback
    {
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

        //conn->RecvFromAllConnections(); //this works too
        if (toServer->RecvData() < 0)
            return;

        //events go to containing connections
        conn->ProcessEvents();
         
        usleep(4000);
    }
}
