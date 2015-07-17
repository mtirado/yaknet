/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef _NET_EVENTS_H_
#define _NET_EVENTS_H_

class Connection;

//TODO -- call error for more events (only armored handshake calls errors right now)
#define YAKERR_UNSPECIFIED   0
#define YAKERR_CONNECTION   -1
#define YAKERR_HANDSHAKE    -2
#define YAKERR_MSG_SIZE     -3
#define YAKERR_CORRUPTION   -4 //most likely someone tampering with the first 4 bytes of packet
#define YAKERR_NOTCONNECTED -5

//net event id's  this should be the index into function pointer array
//so 0 --> numEvents
typedef unsigned int NET_EVENT;

///splitting into 2 types of net events, one can be instanced and used with classes, requiring an instance pointer.
///the other uses no instance pointer, i have a feeling the need for event instances will come up eventually.
///the two classes are basically identical, just using a different callback typedef to include the instance.
///the instance is passed implicitly as 'this', as it must derive from NetEventsInstanced.\n\n
///eventIdx should be an index into the function pointer array.\n
///registered eventId's must be within 0 and numEvents.\n
///there are a few special events that have no id: disconnect, connect, and error.\n
///the Connection *callerId represents which connection sent the event\n\n
///function pointers are a difficult crossroads between C / C++, so i implemented 
///an option for both worlds. unfortunately it still requires static callback methods when using NetEventsInstanced
class NetEventBase
{
protected:
    ///size of the function pointer array.
    unsigned int numEvents;
    
public:
    NetEventBase() { numEvents = 0; }
    ///initialize net event table of a given size
    virtual bool Init(unsigned int netEventTableSize) = 0;
    ///callerId represents which connection sent the event, data will usually be a NetBuffer *
    virtual void ProcessEvent(NET_EVENT eventIdx, Connection *callerId, void *data) = 0;
    ///called when connection is disconnected
    virtual void CallDisconnect(Connection * callerId, void *data) = 0;
    ///called when connection is established
    virtual void CallConnect(Connection * callerId, void *data) = 0;
    ///called on error event, data is an integer error code: see YAKERR list above for error codes
    virtual void CallError(Connection * callerId, int yakErr) = 0;
    ///returns event table size
    unsigned int getNumEvents() { return numEvents; }
};

///this version of net events uses a simple, non instanced callback,
///and is suitable for most use cases, use the instanced version if 
///you need multiple NetEvent states using the same static class methods

///NetEvents manages a list of function pointers as application event callbacks and connect/disconnect/error notifications
class NetEvents : public NetEventBase
{
    
typedef void (*NET_EVENT_CALLBACK)(Connection *, void *);
    
private:
    NetEvents(const NetEvents &) {}
    NetEvents &operator = (const NetEvents &);

    NET_EVENT_CALLBACK eventDisconnect;
    NET_EVENT_CALLBACK eventConnect;
    NET_EVENT_CALLBACK eventError;
    ///the array of function pointers, allocated with Init()
    NET_EVENT_CALLBACK *netEventTable;

public:

    ///initialize callback table
    bool Init(unsigned int netEventTableSize);
    ///returns false if eventIdx is greater than numEvents.
    ///inserts func into netEventTable at eventIdx
    bool RegisterNetEvent(NET_EVENT eventIdx, NET_EVENT_CALLBACK func);
    
    ///register a callback that will be executed on disconnect
    void RegisterDisconnectEvent(NET_EVENT_CALLBACK func);
    ///register callback to be executed on connect
    void RegisterConnectEvent(NET_EVENT_CALLBACK func);
    ///register event to be executed on an error
    void RegisterErrorEvent(NET_EVENT_CALLBACK func);
    
    ///callerId represents which connection sent the event, data will usually be a NetBuffer *
    void ProcessEvent(NET_EVENT eventIdx, Connection *callerId, void *data);
    
    void CallDisconnect(Connection * callerId, void *data);
    void CallConnect(Connection * callerId, void *data);
    //data is an integer error code: see YAKERR list above for error codes
    void CallError(Connection * callerId, int yakErr);
    
    NetEvents();
    ~NetEvents();
};


///TODO test this... i havent had to use it yet.
///passes 'this' as the instance, all callbacks should be static methods!!
///this is the more wasteful option as it is passing around the instance pointer on every call

///derive from this class if you need multiple instances with unique data
class NetEventsInstanced : public NetEventBase
{

typedef void (*NET_EVENT_CALLBACK)(NetEventsInstanced *inst, Connection *, void *);
    
private:
    NetEventsInstanced(const NetEvents &) {}
    NetEventsInstanced &operator = (const NetEventsInstanced &);

    NET_EVENT_CALLBACK eventDisconnect;
    NET_EVENT_CALLBACK eventConnect;
    NET_EVENT_CALLBACK eventError;
    ///the array of function pointers, allocated with Init()
    NET_EVENT_CALLBACK *netEventTable;

public:

    ///initialize callback table
    bool Init(unsigned int netEventTableSize);
    ///returns false if eventIdx is greater than numEvents.
    ///inserts func into netEventTable at eventIdx
    bool RegisterNetEvent(NET_EVENT eventIdx, NET_EVENT_CALLBACK func);
    
    ///register a callback that will be executed on disconnect
    void RegisterDisconnectEvent(NET_EVENT_CALLBACK func);
    ///register callback to be executed on connect
    void RegisterConnectEvent(NET_EVENT_CALLBACK func);
    ///register event to be executed on an error
    void RegisterErrorEvent(NET_EVENT_CALLBACK func);
    
    ///callerId represents which connection sent the event, data will usually be a NetBuffer *
    void ProcessEvent(NET_EVENT eventIdx, Connection *callerId, void *data);
    
    void CallDisconnect(Connection * callerId, void *data);
    void CallConnect(Connection * callerId, void *data);
    //data is an integer error code: see YAKERR list above for error codes
    void CallError(Connection * callerId, int yakErr);
    
    NetEventsInstanced();
    ~NetEventsInstanced();
};

///i suggest an enum for your eventId's example: enum {sv_login = 0, sv_logout, sv_num_events};
///class callback methods must be static, or enjoy the compiler error parade.

///a base class for a non-instanced event container, it must initialize events and register them in RegisterNetEvents()
class NetEventContainer : public NetEvents
{  
    //example method, must be static, with instance
    //static void Login(NetEventContainer *instance, Connection *callerId, void *data);
public:
    NetEventContainer(){}
    ///this method MUST initialize events
    virtual void RegisterNetEvents() = 0;
    unsigned int getNumEvents() { return getNumEvents(); }
};


#endif
