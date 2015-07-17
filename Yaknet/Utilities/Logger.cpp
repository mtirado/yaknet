/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Logger.h"
#include <iostream>
using std::cout;
using std::endl;
using std::stringstream;
using std::string;

static std::stringstream G_OutputBuffer(std::stringstream::out);

stringstream &getOutputBuffer() { return G_OutputBuffer; }

//#define VERBOSE_LOGGING

#if defined(__ANDROID__)
#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LGO_ERROR, "native-activity",__VA_ARGS__))
void LogError(string msg)
{
    LOGE(msg);
}
void LogError(){ LOGE(LOGOUTPUT.str()); LOGOUTPUT.str(""); }


#else

void LogError(string msg){cout << "ERROR: " << msg << endl;}
void LogError(){ cout << "ERROR: " << LOGOUTPUT.str() << endl; LOGOUTPUT.str(""); }
void LogInfo(string msg){cout << msg << endl;}
void LogInfo(){ cout << LOGOUTPUT.str() << endl; LOGOUTPUT.str(""); }
void LogWarning(string msg){cout << "WARNING: " << msg << endl;}
void LogWarning(){ cout << "WARNING: " << LOGOUTPUT.str() << endl; LOGOUTPUT.str(""); }

void LogVerbose(string msg)
{
#ifdef VERBOSE_LOGGING
    cout << "*** " << msg << endl;
#endif
}
void LogVerbose()
{
#ifdef VERBOSE_LOGGING
    cout << "*** " << LOGOUTPUT.str() << endl;
#endif
    LOGOUTPUT.str("");

}


#endif
