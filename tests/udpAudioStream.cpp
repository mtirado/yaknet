/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "Yaknet/NetEvents.h"
#include "Yaknet/Utilities/Logger.h"
#include <iostream>
#include <unistd.h>
#include "Yaknet/UDPConnection.h"
#include "Yaknet/Utilities/console_stuff.h"
#include "stdlib.h"
//usage: place a 16bit 44100hz stereo wave file "testWave.wav" in the directory with the binary
//run server
//run client
//press s to begin stream
//connect more clients if you'd like

////////////  openal stuffs ///////////////
#include <AL/al.h>
#include <AL/alc.h>
//if you need ext prototypes
//#define AL_ALEXT_PROTOTYPES
#include <AL/efx.h>
#include "WavUtil.h"
#include "Yaknet/DataStructures/NetPCMStream.h"
#include <sys/time.h>

ALint error;
ALCcontext      *context = 0;
ALCdevice       *device  = 0;
ALfloat listenerPos[]={0.0,0.0,0.0};
ALfloat listenerVel[]={0.0,0.0,0.0};
ALfloat listenerOri[]={0.0,0.0,-1.0, 0.0,1.0,0.0};      // Listener facing into the screen
WaveSource waveSource;
const char * gFile = "testWave.wav";
int packetsPerTick = 12; //how many packets server sends per tick
const int packetsPerAudioBuff = 300; //how many packets make a complete audio buffer
const int pcmDataPacketSize = 400; //packet header + seq number + data = udp packet size

//server reads from this file stream
FileStream fileStream(pcmDataPacketSize * packetsPerTick);
//client fills this pcm stream from packets received
NetPCMStream pcmStream(pcmDataPacketSize, packetsPerAudioBuff);
size_t dataPos;//position in the file where PCM data starts

bool InitSoundSystem();
void InitialBufferFill(); //fills the first 2 buffers so we can call play safely
void UpdateSoundSystem();
void PlaySource()  {  alSourcePlay(waveSource.source);  }
void StopSource();
void PauseSource() {  alSourcePause(waveSource.source); }
////////////////////////////////////////////

unsigned int curSeq = 0; //current sequence number

bool streamStarted = false;
bool startPlaying = false;
bool initOnce = true;
using std::cout;
using std::endl;
NetEvents *netEvents = 0;
UDPConnection *masterConn;

void Server(unsigned short port);
void Client(const char *ip, unsigned short port);

void cl_stream(Connection *netId, void *data)
{
    if (!streamStarted) //print when the first packet arrives
    {
        LogInfo("server started the stream, filling initial buffers now.");
    }
    streamStarted = true;
    //client stream event,  write this data to the tcpStream object
    NetBuffer *nb = (NetBuffer *)data;
    nb->SetPosition(4); //skip over header info
    unsigned int seq = nb->ReadInt(); //read the sequence number of this packet
    //pcmStream will attempt to put the packets in the right seqeuence
    //hoewever, there is no retransmission logic at this time
    pcmStream.WritePacket(seq, nb->ReadBytes(pcmDataPacketSize));

    //when we have 3 buffers, its safe to start playing(on local net at least)
    if (pcmStream.numBuffers() == 3 && initOnce) //2 buffers for initial play, and 1 for padding
    {
        startPlaying = true;
        initOnce = false;
    }
}

static bool client_disconnected = false;

void cl_Disconnect(Connection *netId, void *data)
{
   client_disconnected = true;
}
int main()
{

    LogInfo("Launching Echo Test");
    NetEvents ne;
    netEvents = &ne;

    if (!netEvents->Init(1))
    {
        LogError("could not initialize networking system.");
        return -1;
    }

    LogInfo("run (s)erver or (c)lient ?");
    char c = getch();

    if (c == 'c')
        Client("127.0.0.1", 1337);
    else
        Server(1337);

    return 0;
}



void Server(unsigned short port)
{    
    LogInfo("starting echo server...");
    UDPConnection *conn = new UDPConnection(netEvents);
    masterConn = conn;
    UDPConnection *client = 0;
    conn->Listen(port);

    char packet[MAX_MSG_SIZE] = {0};
    char *pData = packet;
    int nSize;

    Connection *accepted;
    bool sending = false;

    NetBuffer nb(512);
    //open the file stream
    ALsizei size;
    Engine::WavFormat fmt;
    if (!GetWavInfo(gFile, &size, &fmt, &dataPos))
    {
        cout << "couldn't open wav file: " << gFile << endl;
        return;
    }
    cout << "-----------------------------\n"   <<
            "retreiving wav info: \n"           <<
            "-----------------------------\n"   <<
            "files size: " << size/(1024.0*1024.0) << "MB" << endl <<
            "sample rate: " <<fmt.sampleRate << endl <<
            "dataStart: " << dataPos << endl <<
            "channels: " << fmt.numChannels << endl <<
            "bps: " << fmt.bitsPerSample << endl <<
            "-----------------------------\n";
    fileStream.OpenFile(gFile);
    fileStream.SetPosition(dataPos);
    cout << "press 's' to begin streaming\n";
    while(1)
    {
        conn->BeginTick();
        accepted = conn->AcceptConnection();
        if (accepted)
            cout << "\n\n*new client accepted*\n\n";
        if (conn->RecvFromAllConnections() < 0)
            break;
        conn->ProcessEvents();

        //check input
        if (kbhit())
        {
            char data = getch();

            if (data == 's')
            {
                LogInfo("sending stream to clients.");
                fileStream.SetPosition(dataPos);
                sending = true;
            }
        }

        if (sending)
        {
            //read in n packets * packetSize to be sent
            char *ioBuff = fileStream.StreamData(pcmDataPacketSize * packetsPerTick);
            if (ioBuff)
            {
                for (int i = 0; i < packetsPerTick; i++)
                {
                    nb.ClearBuffer();
                    nb.WriteInt(curSeq);
                    nb.WriteBytes(&ioBuff[i * pcmDataPacketSize], pcmDataPacketSize);
                    nb.Prepare();
                    conn->SendDataToAllConnections(nb.GetBuffer(), 0);
                    curSeq++; //incrememnt sequence #
                }
            }
            else
            {
                LogInfo("file stream reached EOF, press 's' to start over.");
                fileStream.SetPosition(dataPos);
                sending = false;
            }
        }
        
        usleep(1000);

    }
}

void Client(const char *ip, unsigned short port)
{
    struct timeval time;
    gettimeofday(&time,NULL);
    srand((time.tv_sec * 1000) + (time.tv_usec / 1000));
    
    if (!InitSoundSystem())
    {
        LogError("Could not initialize OpenAL");
        return;
    }
    netEvents->RegisterDisconnectEvent(cl_Disconnect);
    unsigned short lPort = 10000 + rand()%40000;
    LOGOUTPUT << "starting client with random port between 10000 and 50000: " << lPort;
    LogInfo();
    
    UDPConnection *conn = new UDPConnection(netEvents);
    
    if (!conn->Listen(lPort)) //all master connections need to listen, this creates the socket
        return;
    Connection *toServer = conn->Connect(ip, port);

    cout << endl << endl;

    //register echo callback
    netEvents->RegisterNetEvent(0, cl_stream); //only 1 event, id: 0

    NetBuffer nb(MAX_MSG_SIZE);

    if (alGetError() != AL_NO_ERROR)
        LogInfo("there was a frickin error");
    bool playing = false;
    while(1) //if using recvFromAllConnections, will need disconnect event callback
    {
        if (startPlaying)
        {
            InitialBufferFill();
            PlaySource();
            playing = true;
            startPlaying = false;
        }
        if (playing)
            UpdateSoundSystem();
        conn->BeginTick(); //runs handshake logic
        if (kbhit())
        {
            char data = getch();

            if (data == 'p')
            {
                if (playing)
                {
                    PauseSource();
                    LogInfo("pausing...");
                }
                else
                {
                    PlaySource();
                    LogInfo("Playing...");
                }
                playing = !playing;
            }
            if (data == 's')
            {
                playing = false;
                StopSource();
                LogInfo("Stopping...");
            }

        }

        if(conn->RecvFromAllConnections() < 0)
            return;

        //events go to containing connections
        conn->ProcessEvents();

        usleep(1000);
    }
}

//nothing fancy, just simple playback
bool InitSoundSystem()
{
    device = alcOpenDevice(NULL); //open 'preferred' device
    if (!device)
        return false;

    context = alcCreateContext(device, 0);
    if (!context)
    {
        LogError("Could not create OpenAL context");
        return false;
    }
    alcMakeContextCurrent(context);
    if ((error = alGetError()) != AL_NO_ERROR)
        return false; //TODO: error reporting

    alListenerfv(AL_POSITION, listenerPos);    // Position ...
    alListenerfv(AL_VELOCITY, listenerVel);    // Velocity ...
    alListenerfv(AL_ORIENTATION, listenerOri); // Orientation ...

    ALfloat listenerPosv[]={0.0,0.0,0.0};
    ALfloat listenerVelv[]={0.0,0.0,0.0};

    alGenBuffers(2, &waveSource.buffer[0]);
    alGenSources(1, &waveSource.source);
    alSourcef(waveSource.source,  AL_PITCH, 1.0f);
    alSourcef(waveSource.source,  AL_GAIN, 0.64f);
    alSourcefv(waveSource.source, AL_POSITION, listenerPosv);
    alSourcefv(waveSource.source, AL_VELOCITY, listenerVelv);
    //alSourcei(waveSource.source,  AL_BUFFER, waveSource.buffer[0]);
    alSourcei(waveSource.source,  AL_LOOPING, AL_FALSE);
    alSourcei(waveSource.source,  AL_SOURCE_RELATIVE, AL_TRUE); //puts source directly on listener

    return true;//LoadSoundWav("testWave.wav", &waveSource);
}

void InitialBufferFill()
{
    pcmStream.CheckSeqKeys();
    std::pair<char *, unsigned int> aBuff = pcmStream.GetAudioBuffer();
    if (aBuff.first == 0)
    {
        LogError("no audio in pcmStream");
        return;
    }
    alBufferData(waveSource.buffer[0], AL_FORMAT_STEREO16, aBuff.first, aBuff.second, 44100);

    aBuff = pcmStream.GetAudioBuffer();
    if (aBuff.first == 0)
    {
        LogError("no audio in pcmStream");
        return;
    }
    alBufferData(waveSource.buffer[1], AL_FORMAT_STEREO16, aBuff.first, aBuff.second, 44100);
    alSourceQueueBuffers(waveSource.source, 2, waveSource.buffer);
}

void UpdateSoundSystem()
{
    int processed;
    alGetSourcei(waveSource.source, AL_BUFFERS_PROCESSED, &processed);
    while(processed > 0 && processed < 3)
    {

        //pop a full audio buffer from pcmstream
        std::pair<char *, unsigned int> aBuff = pcmStream.GetAudioBuffer();
        if (aBuff.first == 0)
        {
            //if this happens than our buffers are empty! reset state to refill them
            LogError("no audio in pcmStream -- buffers are empty -- refilling... bufferrring.....");
            StopSource();
            return;
        }
        processed--;
        ALuint buff;
        alSourceUnqueueBuffers(waveSource.source, 1, &buff);
        alBufferData(buff, AL_FORMAT_STEREO16, aBuff.first, aBuff.second, 44100);
        alSourceQueueBuffers(waveSource.source, 1, &buff);
        delete[] aBuff.first;
    }
    alGetSourcei(waveSource.source, AL_BUFFERS_QUEUED, &processed);
    if (!processed)
    {
        StopSource();
    }
}


void StopSource()
{
    //reinitialize the stream when we start back up
    initOnce = true;
    startPlaying = false;
    pcmStream.ClearStream();
    alSourceStop(waveSource.source);
    int processed;
    ALuint buff[2];
    alGetSourcei(waveSource.source, AL_BUFFERS_PROCESSED, &processed);
    alSourceUnqueueBuffers(waveSource.source, processed, buff);
}
