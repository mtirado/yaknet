/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Yaknet/Connection.h"
#include "Yaknet/Utilities/Logger.h"
#include "Yaknet/NetEvents.h"
#include "Yaknet/Connection.h"
#include "Yaknet/Utilities/StaticTimer.h"

NetEvents::NetEvents()
{
    netEventTable = 0;
    eventDisconnect = 0;
    eventConnect = 0;
    eventError = 0;
}

NetEvents::~NetEvents()
{}

bool NetEvents::Init(unsigned int netEventTableSize)
{
    //this will only init THE static timer ONCE and is needed for the udp connection timer
    StaticTimer::InitInstance();
    
    if (netEventTableSize == 0) return false;
    if (numEvents > 0 )
    {
        LogError("double initialization of NetEvents just happened. and ignored.");
        return false;
    }
    
    numEvents = netEventTableSize;
    netEventTable = new NET_EVENT_CALLBACK[numEvents];
    memset(netEventTable, 0, sizeof(void *) * numEvents);
    LOGOUTPUT << "NetEvents System Initialized, callback table size(" << numEvents << ")";
    LogInfo();
    return true;
}

bool NetEvents::RegisterNetEvent(NET_EVENT eventIdx, NET_EVENT_CALLBACK func)
{
    if (eventIdx > numEvents -1)
    {
        LogError("eventIdx out of bounds");
        return false;
    }
    if (netEventTable[eventIdx] != 0)
    {
        LogError("event already registered, ignoring request.");
	return false;
    }
    netEventTable[eventIdx] = func;
    return true;
}

void NetEvents::ProcessEvent(NET_EVENT eventIdx, Connection *callerId, void* data)
{
    if (eventIdx > numEvents-1)
    {
        LOGOUTPUT << "ProcessEvent() - invalid eventIdx: " << eventIdx;
        LogInfo();
        return;
    }
    if (netEventTable[eventIdx] == 0)
    {
        LogError("ProcessEvent() - Network Event not registered!");
        return;
    }

    //LOGOUTPUT << "eventidx: " << eventIdx << " | callerId : " << callerId << " | data: " << ((char *)data)[4];
    //LogInfo();
    //LogInfo("callback - go!");
    (netEventTable[eventIdx])(callerId, data);
}


void NetEvents::RegisterConnectEvent(NET_EVENT_CALLBACK func)
{
    eventConnect = func;
}

void NetEvents::RegisterDisconnectEvent(NET_EVENT_CALLBACK func)
{
    eventDisconnect = func;
}

void NetEvents::RegisterErrorEvent(NET_EVENT_CALLBACK func)
{
    eventError = func;
}


void NetEvents::CallConnect(Connection* callerId, void* data)
{
    if (eventConnect) eventConnect(callerId, data);
    //else LogError("Connect Event not registered!");
}

void NetEvents::CallDisconnect(Connection* callerId, void* data)
{
    if (eventDisconnect) eventDisconnect(callerId, data); 
    //else LogError("Disconnect Event not registered!");
}

void NetEvents::CallError(Connection* callerId, int yakErr)
{
    if (eventError) eventError(callerId, (void *)yakErr);
    //else LogError("Error Event not registered");
}



/////////   instanced net events   //////////
//lots of duplicate code because the callback adds an instance parameter :(
NetEventsInstanced::NetEventsInstanced()
{
    netEventTable = 0;
    eventDisconnect = 0;
    eventConnect = 0;
    eventError = 0;
}

NetEventsInstanced::~NetEventsInstanced()
{}

bool NetEventsInstanced::Init(unsigned int netEventTableSize)
{
    //this will only init THE static timer ONCE and is needed for the udp connection timer
    StaticTimer::InitInstance();
    
    if (netEventTableSize == 0) return false;
    if (numEvents > 0 )
    {
        LogError("double initialization of NetEventsInstanced just happened. and ignored.");
        return false;
    }
    
    numEvents = netEventTableSize;
    netEventTable = new NET_EVENT_CALLBACK[numEvents];
    memset(netEventTable, 0, sizeof(void *) * numEvents);
    LOGOUTPUT << "NetEventsInstanced System Initialized, callback table size(" << numEvents << ")";
    LogInfo();
    return true;
}

bool NetEventsInstanced::RegisterNetEvent(NET_EVENT eventIdx, NET_EVENT_CALLBACK func)
{
    if (eventIdx > numEvents -1)
    {
        LogError("eventIdx out of bounds");
        return false;
    }
    if (netEventTable[eventIdx] != 0)
    {
        LogError("event already registered, ignoring request.");
        return false;
    }
    netEventTable[eventIdx] = func;
    return true;
}

void NetEventsInstanced::ProcessEvent(NET_EVENT eventIdx, Connection *callerId, void* data)
{
    if (eventIdx > numEvents-1)
    {
        LOGOUTPUT << "ProcessEvent() - invalid eventIdx: " << eventIdx;
        LogInfo();
        return;
    }
    if (netEventTable[eventIdx] == 0)
    {
        LogError("ProcessEvent() - Network Event not registered!");
        return;
    }

    //LOGOUTPUT << "eventidx: " << eventIdx << " | callerId : " << callerId << " | data: " << ((char *)data)[4];
    //LogInfo();
    //LogInfo("callback - go!");
    (netEventTable[eventIdx])(this, callerId, data);
}


void NetEventsInstanced::RegisterConnectEvent(NET_EVENT_CALLBACK func)
{
    eventConnect = func;
}

void NetEventsInstanced::RegisterDisconnectEvent(NET_EVENT_CALLBACK func)
{
    eventDisconnect = func;
}

void NetEventsInstanced::RegisterErrorEvent(NET_EVENT_CALLBACK func)
{
    eventError = func;
}


void NetEventsInstanced::CallConnect(Connection* callerId, void* data)
{
    if (eventConnect) eventConnect(this, callerId, data);
    //else LogError("Connect Event not registered!");
}

void NetEventsInstanced::CallDisconnect(Connection* callerId, void* data)
{
    if (eventDisconnect) eventDisconnect(this, callerId, data); 
    //else LogError("Disconnect Event not registered!");
}

void NetEventsInstanced::CallError(Connection* callerId, int yakErr)
{
    if (eventError) eventError(this, callerId, (void *)yakErr);
    //else LogError("Error Event not registered");
}


