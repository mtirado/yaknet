/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Yaknet/Utilities/console_stuff.h"
#include "Yaknet/NetEvents.h"
#include "Yaknet/Utilities/Logger.h"
#include "Yaknet/TCPConnection.h"
#include <iostream>
#include "chatEvents.h" //the protocol
NetEvents ne;
NetEvents *netEvents = 0;
void Server(short port);
void Client(const char *ip, short port);

int main()
{
    LogInfo("yak chat!");

    //init happens later when we know if its server or client
    netEvents = &ne;
    
    LogInfo("run (s)erver or (c)lient ?");
    char c = getch();

    if (c == 'c')
        Client("0.0.0.0", 1337);
    else
        Server(1337);

    return 0;
}

void sv_Disconnect(Connection *callerId, void *data)
{
    std::string lazyBuff;
    lazyBuff = G_Users[callerId] + " - has retreated";

    NetBuffer response(MAX_MSG_SIZE);

    response.WriteString(lazyBuff.c_str()); //success
    response.Prepare();
    LOGOUTPUT << lazyBuff.c_str();
    LogInfo();
    G_Users.erase(callerId);
    //NOTE, probably going to try to send to just the dropped connection.
    GetServerConnection()->SendDataToAllConnections(response.GetBuffer(), CL_CHAT_MSG);

}

void Server(short port)
{
    LogInfo("starting yakchat server...");
    TCPConnection *conn = new TCPConnection(netEvents);
    TCPConnection *client = 0;
    netEvents->RegisterDisconnectEvent(sv_Disconnect);
    //register all callbacks!
    RegisterServerCallbacks(netEvents, conn);

    //tell the connection to start listening
    conn->Listen(port);

    Connection *accepted;
    while(1)
    {
        conn->BeginTick();
        accepted = conn->AcceptConnection();
        //if (accepted)
        //      newConn = accepted;
        conn->RecvFromAllConnections();
        conn->ProcessEvents();
          
        usleep(6000); //dont annihilate the cpu plz

    }
}

bool client_disconnected = false;
void cl_Disconnect(Connection *callerId, void *data)
{
    client_disconnected = true;
}

void Client(const char *ip, short port)
{
    
    LogInfo("starting yakchat client");
    TCPConnection *conn = new TCPConnection(netEvents);

    Connection *toServer=0;
    //register callbacks
    RegisterClientCallbacks(netEvents);
    netEvents->RegisterDisconnectEvent(cl_Disconnect);

    //connect to the server
    toServer = conn->Connect(ip, port);
        if (!toServer) return;

    std::string uname;
    LogInfo("enter username: ");
    getline(std::cin, uname);

    NetBuffer nb(MAX_MSG_SIZE);
    nb.WriteString(uname.c_str());
    nb.Prepare();
    toServer->SendData(nb.GetBuffer(), SV_LOGIN);//login message
    nb.ClearBuffer();

    std::string tmp;
    while(!client_disconnected) //if using recvFromAll - needs disconnect event callback
    {
        if (kbhit())
        {
            char data = getch();
            std::cout << ">" << data;

            getline(std::cin, tmp);
            tmp = data + tmp;
            //cout << "sending server char(" << data << ")\n";

            //b.WriteShort(tmp.size() + 5);
            //nb.WriteShort(SV_CHAT_MSG);
            nb.WriteString(tmp.c_str());
            nb.Prepare();
           
           if ( !toServer->SendData(nb.GetBuffer(), SV_CHAT_MSG))
                LogInfo("send failed");

            nb.ClearBuffer();
               
        }
        

        //error occured if false
        //conn->RecvFromAllConnections();
        if (toServer->RecvData() < 0)
            return;

        //events go to containing connection
        conn->ProcessEvents();
         
        usleep(4000);
    }
}
