project(yaknet)
cmake_minimum_required ( VERSION 2.8 )

message(STATUS "setting include dirs")
include_directories( ${CMAKE_SOURCE_DIR} )
#include_directories( ${CMAKE_SOURCE_DIR}/DataStructures )
include_directories( ${CMAKE_SOURCE_DIR} )
#include_directories( ${CMAKE_SOURCE_DIR}/utilities )
include_directories( ${CMAKE_SOURCE_DIR}/tests )


set(yaknet_SRCS
                Yaknet/NetBuffer.cpp
                Yaknet/NetEvents.cpp
                Yaknet/Connection.cpp
                Yaknet/TCPConnection.cpp
                Yaknet/WebSockets.cpp
                Yaknet/TCPStream.cpp
                Yaknet/UDPConnection.cpp
                Yaknet/TCPArmored.cpp
                Yaknet/TCPBuffer.cpp)

set (utilities_SRCS
                Yaknet/Utilities/StaticTimer.cpp
                Yaknet/Utilities/Logger.cpp
                Yaknet/Utilities/sha1.cpp
                Yaknet/Utilities/b64_ntop.cpp)

#set the bin and lib foldes
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(LINK_DIRECTORIES ${PROJECT_SOURCE_DIR}/lib)

#build the static lib
add_library(yaknet STATIC ${yaknet_SRCS} ${utilities_SRCS})




###### build examples #######

                
message(STATUS "echo test...")
add_executable(tcpEcho tests/tcpEcho.cpp)
target_link_libraries(tcpEcho yaknet)



message(STATUS "tcp chat test...")
add_executable(tcpChat
               tests/chatEvents.cpp
               tests/tcpChat.cpp)
target_link_libraries(tcpChat yaknet)



message(STATUS "websocket echo test...")
add_executable(websockEcho tests/webSockEcho.cpp)
target_link_libraries(websockEcho yaknet)



message(STATUS "websocket chat test...")
add_executable(websockChat
               tests/chatEvents.cpp
               tests/webSockChat.cpp)
target_link_libraries(websockChat yaknet)



message(STATUS "IRC test...")
add_executable(tcpstreamIRC tests/tcpstreamIRC.cpp)
target_link_libraries(tcpstreamIRC yaknet)



message(STATUS "UDP Echo test...")
add_executable(udpEcho tests/udpEcho.cpp)
target_link_libraries(udpEcho yaknet)


## these examples rely on external libraries, comment them out if you don't care

message(STATUS "armored chat test...")
add_executable(armoredChat
               tests/chatEvents.cpp
               tests/armoredChat.cpp)
target_link_libraries(armoredChat yaknet)
#i think gnutls can be static linked too...
target_link_libraries(armoredChat gnutls)

#requires openal for cross platform simplicity sake
#http://kcat.strangesoft.net/openal.html
#openal.org
find_package( OpenAL REQUIRED )

message(STATUS "UDP Audio Stream test...")
add_executable(udpAudio
               tests/WavUtil.cpp
               tests/udpAudioStream.cpp)
target_link_libraries(udpAudio yaknet)                
target_link_libraries(udpAudio ${OPENAL_LIBRARY})
