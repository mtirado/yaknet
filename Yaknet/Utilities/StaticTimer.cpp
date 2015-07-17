/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "StaticTimer.h"

StaticTimer *StaticTimer::instance = 0;

void StaticTimer::InitInstance()
{
    if (!instance)
    {
        instance = new StaticTimer;
        instance->Init();
    }
}

void StaticTimer::DeleteInstance()
{
    if (instance)
    {
        delete instance;
        instance = 0;
    }
}

void StaticTimer::Init()
{
    gettimeofday(&timeStart, 0);
}


time_t StaticTimer::GetTimestamp()
{
    gettimeofday(&timeCurrent, 0);
    elapsedMicroSeconds = timeCurrent.tv_usec - timeStart.tv_usec;
    elapsedSeconds = timeCurrent.tv_sec - timeStart.tv_sec;

    return (elapsedSeconds * 1000) + (elapsedMicroSeconds / 1000);
}
