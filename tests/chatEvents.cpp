/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "chatEvents.h"
#include "Yaknet/Utilities/Logger.h"
//this is the only thing i'm not liking thus far, needing a global for server to send
//to all connections...  must ponder some more?
std::map<Connection *, std::string> G_Users;
static Connection *G_ServerConnection = 0;

Connection *GetServerConnection() { return G_ServerConnection; }

void RegisterServerCallbacks(NetEvents *ne, Connection *serverConn)
{
    G_ServerConnection = serverConn;
    ne->Init(NUM_SV_CALLBACKS);
    ne->RegisterNetEvent(SV_LOGIN, svLogin);
    ne->RegisterNetEvent(SV_LOGOUT, svLogout);
    ne->RegisterNetEvent(SV_CHAT_MSG, svChatMsg);
}

void RegisterClientCallbacks(NetEvents *ne)
{
    ne->Init(NUM_CL_CALLBACKS);
    ne->RegisterNetEvent(CL_LOGIN, clLogin);
    //ne->RegisterNetEvent(1, clLogout);
    ne->RegisterNetEvent(CL_CHAT_MSG, clChatMsg);
}

//Server Callbacks

//client requesting login
void svLogin(Connection *callerId, void *data)
{
        NetBuffer *nb = (NetBuffer *)data;
        char *tmp = nb->ReadString();//username
        
        //TODO check if username exists already or something, and respond 0 for failure
        G_Users[callerId] = tmp; //copied into string
        LOGOUTPUT << "User Logged in : " << tmp;
        LogInfo();
        delete[] tmp;

        //send response
        NetBuffer response(MAX_MSG_SIZE);
        response.WriteShort(1); //success
        response.Prepare();
        callerId->SendData(response.GetBuffer(), CL_LOGIN);

        //send login notification
        std::string lazyBuff;
        lazyBuff = G_Users[callerId] + " - has logged in";

        response.ClearBuffer();	
        response.WriteString(lazyBuff.c_str());
        response.Prepare();
        
        G_ServerConnection->SendDataToAllConnections(response.GetBuffer(), CL_CHAT_MSG);

}

//client is logging out
void svLogout(Connection *callerId, void *data)
{

}

//data is a string containing the message, do not assume null termination.
void svChatMsg(Connection *callerId, void *data)
{
        NetBuffer *nb = (NetBuffer *)data;

        char *tmp = nb->ReadString();//msg
        //TODO check if username exists already or something, and respond 0 for failure
        std::string lazyBuff;
        lazyBuff = G_Users[callerId] + ": ";
        lazyBuff += tmp;

        LOGOUTPUT << lazyBuff;
        LogInfo();

        //send response
        NetBuffer response(MAX_MSG_SIZE);	
        response.WriteString(lazyBuff.c_str()); //success
        response.Prepare();
        
        if (G_ServerConnection)
                G_ServerConnection->SendDataToAllConnections(response.GetBuffer(), CL_CHAT_MSG);
        else
                LogError("Null Server Connection?");

        delete[] tmp;
}

//Client Callbacks

//data is a char containing 0 or non zero
//0 being a failure,
void clLogin(Connection *callerId, void *data)
{
        LogInfo("cl_Login()");
}

void clLogout(Connection *callerId, void *data){}

//data is a string containing the message
void clChatMsg(Connection *callerId, void *data)
{
        NetBuffer *nb = (NetBuffer *)data;

        char *tmp = nb->ReadString();//msg
        LOGOUTPUT << tmp;
        LogInfo();
        delete[] tmp;
}
