/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __STATIC_TIMER_H__
#define __STATIC_TIMER_H__

#include <sys/time.h>
///use this to get a timestamp from when initialized  (good for ~49 days untill wrap around) constrained by time_t size which could be 32bits
//XXX this probably won't work on windows? but neither will any of yaknet currently.
class StaticTimer
{
private:
    StaticTimer(const StaticTimer &){};
    StaticTimer &operator = (const StaticTimer &) { return *this;}

    static  StaticTimer *instance;
    timeval timeStart;
    timeval timeCurrent;

    time_t       elapsedSeconds;
    suseconds_t  elapsedMicroSeconds;

    void Init();
public:

    StaticTimer() {};
    ~StaticTimer() {};
    
    ///returns miliseconds since timer init
    time_t GetTimestamp();
    
    ///initializes the singleton and sets the timer start
    static void InitInstance();
    static void DeleteInstance();
    ///if you crash here, then you forgot to initialize NetEvents
    inline static StaticTimer *GetInstance() { return instance; }
};

#endif
