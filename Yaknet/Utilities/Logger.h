/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef _LOGGER_H_
#define _LOGGER_H_
#include <sstream>
std::stringstream &getOutputBuffer();

#define LOGOUTPUT getOutputBuffer()

/*******************
      Logging
*******************/
//passing a string will print that string, obviously
//passing nothing will print the LOGOUTPUT stringstream
void LogError();
void LogError(std::string msg);
void LogInfo(std::string msg);
void LogInfo();
void LogWarning(std::string msg);
void LogWarning();
void LogVerbose(std::string msg);
void LogVerbose();



#endif
