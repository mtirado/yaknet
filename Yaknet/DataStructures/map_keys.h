/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __MAP_KEYS_H__
#define __MAP_KEYS_H__

//64 bit type for maps (long long SHOULD always be 64bit since C99?)
//TODO ipv6? doh..
class sockaddr_key_t
{
private:
    unsigned long long data;

public:
    sockaddr_key_t() { data = 0; }
    
    sockaddr_key_t(unsigned int ipv4, unsigned int port) { Set(ipv4, port); }
    
    inline unsigned long long GetData() const { return data; }

    bool operator <  (const sockaddr_key_t &other) const { return data <  other.data; }
    bool operator == (const sockaddr_key_t &other) const { return data == other.data; }

    void Set(unsigned int ipv4, unsigned int port)
    {
        data = 0;
        ((unsigned int *)&data)[0] = ipv4;
        ((unsigned int *)&data)[1] = port;
    }
    
    unsigned long long getData() { return data; }
};

#endif
