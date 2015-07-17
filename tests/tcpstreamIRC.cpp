/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Yaknet/TCPStream.h"
#include "Yaknet/Utilities/Logger.h"
#include <iostream>
#include <unistd.h>
#include "Yaknet/Utilities/console_stuff.h"

bool client_disconnected = false;
void ParseStream(TCPStream *conn, unsigned int size);

int main()
{
    //netEvents->RegisterDisconnectEvent(cl_Disconnect);
    NetEvents ne;
    ne.Init(0); //doesnt use normal events(not packetized)
    LogInfo("starting client");
    TCPStream  *conn = new TCPStream(&ne);
    TCPStream  *toServer = (TCPStream *)conn->Connect("130.239.18.172", 6667);

    int nBytes = 0;
    char packet[MAX_MSG_SIZE] = {0};

    NetBuffer nb(MAX_MSG_SIZE, false);
    std::string input;

    char user[] = "USER g76780hgbck . . : nameless\r\n";
    nb.WriteBytes(user, strlen(user));
    if (!toServer->SendData(nb.GetBuffer(), nb.GetWritePosition()))
        LogInfo("send failed");
    nb.PrintBytes();
    nb.ClearBuffer();
    char nick[] = "NICK t_e_s_t__\r\n";
    nb.WriteBytes(nick, strlen(nick));
    if (!toServer->SendData(nb.GetBuffer(), nb.GetWritePosition()))
        LogInfo("send failed");
    nb.PrintBytes();
    nb.ClearBuffer();
    LogInfo("sending login info...");
    
    while(!client_disconnected) //if using recvFromAllConnections, will need disconnect event callback
    {
        if (kbhit())
        {
            char data = getch();
            std::cout << ">" << data;

            getline(std::cin, input);
            input = data + input + "\r\n";
            nb.WriteBytes(input.c_str(), strlen(input.c_str()));
            nb.PrintBytes();

           if ( !toServer->SendData(nb.GetBuffer(), nb.GetWritePosition()))
                LogInfo("send failed");

            nb.ClearBuffer();

        }

        //conn->RecvFromAllConnections(); //this works too
        unsigned int size = toServer->RecvData();
        if (size > 0)
        {
            std::cout << "recv(" << size << ") bytes\n";
            //parse data received
            ParseStream(toServer, size);
        }
        usleep(4000);
    }
    
    return 0;
}
char output[2048];
void ParseStream(TCPStream* conn, unsigned int size)
{
    if (size >= 2048)
        return;
    //ehh, just print no actions really needed
    memcpy(output, conn->getBufferData(), size);
    output[size] = 0;

    //remove \r
//     for (int i = 0; i < size; i++)
//     {
//         if (output[i] == '\r')
//             output[i] = ' ';
//     }
    std::cout << output << std::endl;
}
