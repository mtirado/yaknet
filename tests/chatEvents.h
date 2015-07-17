/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __CHAT_EVENTS_H__
#define __CHAT_EVENTS_H__

#include "Yaknet/Connection.h"
#include "Yaknet/NetEvents.h"
#include <map>
#include <string>

//usernames, uses connection pointer as an ID
extern std::map<Connection *, std::string> G_Users;

enum SV_EVENTS { SV_LOGIN = 0, SV_LOGOUT, SV_CHAT_MSG, NUM_SV_CALLBACKS };
enum CL_EVENTS { CL_LOGIN = 0, CL_CHAT_MSG, NUM_CL_CALLBACKS };

void RegisterServerCallbacks(NetEvents *ne, Connection *serverConn);
Connection *GetServerConnection();
void RegisterClientCallbacks(NetEvents *ne);

//Server Callbacks
void svLogin(Connection *callerId, void *data);
void svLogout(Connection *callerId, void *data);
void svChatMsg(Connection *callerId, void *data);

//Client Callbacks

//login success/failure
void clLogin(Connection *callerId, void *data);
//void clLogout(unsigned int netId, void *data);

//text from the chat
void clChatMsg(Connection *callerId, void *data);


#endif
