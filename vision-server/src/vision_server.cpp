/*
WHAT IF THE SERVER IS SENDING DATA TOO FAST? IS THAT POSSIBLE?
    seems like it, maybe make a client first to check
    MAKE SURE TO CHECK HOW MUCH THE SERVER IS ACTUALLY SENDING WHEN TESTING CLIENT
    SHOULD ALSO GOOGLE ABOUT HOW VIDEO STREAMS ARE SUPPOSED TO WORK
        these systems aren't "supposed" to receive every bit of video... so missing bits becuase we're too slow actually makes sense
IT WOULD BE MORE OOP FOR SEND AND RECEIVE CALLS TO TAKE OR RETURN AN OBJECT THAT CONTAINS AN ADDRESS AND POINTER TO DATA, BUT MIGHT BE A MESS, IDK
    only bother doing this if you need to know length of message received I guess
*/


#include <libfreenect.hpp>
#include <iostream>
#include <memory>

#include <sys/socket.h>
#include <netinet/in.h> // consts and structs for use with sockets
#include <errno.h> // to get error numbers after system call errors


const freenect_resolution RESOLUTION = FREENECT_RESOLUTION_MEDIUM;
const unsigned short SERVER_PORT = 6969;
const int MESSAGE_CHUNK_SIZE = 10000;


class VRCarVision : public Freenect::FreenectDevice {
public:

    VRCarVision(freenect_context* _ctx, int _index) : 
        Freenect::FreenectDevice(_ctx, _index),
        _hasDepthData(false),
        _hasRGBData(false)
    {}

    ~VRCarVision()
    {
        stopVideo();
        stopDepth();
        setLed(LED_BLINK_GREEN);
    }

    void VideoCallback(void* newFrame, uint32_t timestamp) override 
    {
        if (_rgbData != nullptr) {
            uint8_t* frameData = static_cast<uint8_t*>(newFrame);
            std::copy(frameData, frameData + getVideoBufferSize(), _rgbData.get());
            _hasRGBData = true;
        }
    }

    void DepthCallback(void* newFrame, uint32_t timestamp)
    {
        if (_depthData != nullptr) {
            uint8_t* frameData = static_cast<uint8_t*>(newFrame);
            std::copy(frameData, frameData + getDepthBufferSize(), _depthData.get());
            _hasDepthData = true;
        }
    }

    void initializeVideo(freenect_resolution resolution)
    {
        setVideoFormat(FREENECT_VIDEO_RGB, resolution);
        _rgbData.reset(new uint8_t[getVideoBufferSize()]);
        _depthData.reset(new uint8_t[getDepthBufferSize()]);
        startVideo();
        startDepth();
    }

    bool hasVideoData() { return _hasDepthData && _hasRGBData; }

    uint8_t* rgbData() { return _rgbData.get(); }
    uint8_t* depthData() { return _depthData.get(); }
    
    int rgbDataSize() { return getVideoBufferSize(); } // getVideoBufferSize is protected so make wrapper method to call it
    int depthDataSize() { return getDepthBufferSize(); }

private:
    bool _hasDepthData; // to avoid sending garbage, keep track of whether _videoData has been filled with real data or not
    bool _hasRGBData; 
    std::unique_ptr<uint8_t[]> _rgbData;
    std::unique_ptr<uint8_t[]> _depthData;
};



void errIfNegative(const int val, const char* errorMessage) 
{
    if (val < 0) {
        std::cerr << "ERROR: " << errorMessage << std::endl; // print to stderr instead of stdout
        std::cerr << "last error code: " << errno << std::endl; // print latest system error
        exit(69); // a classic
    }
}



class UDPServer {
public:

    UDPServer(unsigned short port) 
    {
        _server = socket(AF_INET, SOCK_DGRAM, 0); // AF_INET = ipv4, SOCK_DGRAM = datagram socket, 0 = default protocol = UDP (since it's a datagram socket) 
        errIfNegative(_server, "couldn't create server socket");

        // server IP object
        in_addr serverIp; // in_addr is a struct with few members but the only one we need to set is IP
        serverIp.s_addr = INADDR_ANY; // INADDR_ANY is equivilant to '0.0.0.0' (listen on all available IPs)

        // full server address struct
        sockaddr_in serverAddress; // sockaddr_in is an internet socket address
        serverAddress.sin_family = AF_INET; // AF_INET because internet socket
        serverAddress.sin_port = htons(port); // htons to convert to big endian (aka network byte order)
        serverAddress.sin_addr = serverIp;

        // 0 fill garbarge part of address object that has to be all 0 (why does this exist?)
        uint8_t* sin_zeroStart = static_cast<uint8_t*>(serverAddress.sin_zero);
        uint8_t* sin_zeroEnd = sin_zeroStart + sizeof(serverAddress.sin_zero);
        for (uint8_t* chr = sin_zeroStart; chr < sin_zeroEnd; chr++) {
            *chr = '\0'; 
        }
        
        // bind to address
        int result = bind(
            _server, 
            reinterpret_cast<sockaddr*>(&serverAddress), // have to cast server address to more generic sockaddr type (CAN SAFELY USE reinterpret_cast HERE BECAUSE IT'S JUST GOING FROM ONE POINTER TO ANOTHER AND WE'RE PASSING IN THE SIZE OF serverAddress SO IT KNOWS HOW MUCH TO READ)
            sizeof(serverAddress)
        );
        errIfNegative(result, "binding server socket failed");
    }


    sockaddr_storage receive(uint8_t* messageBuffer = nullptr, int messageBufferLength = 0)
    {
        sockaddr_storage clientAddress; // sender address will be stored in here when message is received
        socklen_t clientAddressLength = sizeof(clientAddress); // amazing that socklen_t is its own type
        int result = recvfrom(
            _server,
            messageBuffer, messageBufferLength, // if messageBuffer is null and messageBufferLength is 0 then just sender's address will be received
            0, // no special flags needed
            reinterpret_cast<sockaddr*>(&clientAddress), 
            &clientAddressLength // for some reason they want a pointer to the size of the address
        );
        errIfNegative(result, "recvfrom failed");
        return clientAddress;
    }


    bool receiveNonBlocking(sockaddr_storage* clientAddress, uint8_t* messageBuffer = nullptr, int messageBufferLength = 0)
    {
        socklen_t clientAddressLength = sizeof(sockaddr_storage);
        int result = recvfrom(
            _server,
            messageBuffer, messageBufferLength, // if messageBuffer is null and messageBufferLength is 0 then just sender's address will be received
            MSG_DONTWAIT, // add special flag to make call nonblocking
            reinterpret_cast<sockaddr*>(clientAddress), 
            &clientAddressLength // for some reason they want a pointer to the size of the address
        );
        if (result < 0 && (errno == EAGAIN  || errno == EWOULDBLOCK)) { // check if a message was received
            return false;
        }
        errIfNegative(result, "nonblocking recvfrom failed");
        return true;
    }


    void send(sockaddr_storage clientAddress, uint8_t* messageStart, int messageLength)
    {
        int result = sendto(
            _server, messageStart, messageLength, 
            0, // no special flags
            reinterpret_cast<sockaddr*>(&clientAddress), 
            sizeof(clientAddress)
        );
        errIfNegative(result, "sendto failed");
    }


private:
    int _server; // file descriptor for the server's socket object
};



void runServer(VRCarVision* kinect) 
{
    UDPServer server(SERVER_PORT);
    auto client = server.receive();

    for(;;) {

        uint8_t* dataEnd = kinect->rgbData() + kinect->rgbDataSize();
        for (uint8_t* messageStart = kinect->rgbData(); messageStart < dataEnd; messageStart += MESSAGE_CHUNK_SIZE) {

            int messageLength;
            if ((messageStart + MESSAGE_CHUNK_SIZE) < dataEnd)
                messageLength = MESSAGE_CHUNK_SIZE;
            else
                messageLength = dataEnd - messageStart;

            server.send(client, messageStart, messageLength);
        }

        sockaddr_storage newClient;
        uint8_t command = 0;
        if (server.receiveNonBlocking(&newClient, &command, sizeof(command))) {
            if (command == 'd')
                break;
            else
                client = newClient;
        }
    }
}



int main() 
{
    Freenect::Freenect freenect;
    VRCarVision* kinect = &freenect.createDevice<VRCarVision>(0);
    kinect->setLed(LED_RED);

    kinect->initializeVideo(RESOLUTION);

    kinect->setLed(LED_YELLOW);
    while (! kinect->hasVideoData()) {}
    kinect->setLed(LED_GREEN);

    runServer(kinect);

    while (1) {
        double degrees;
        std::cout << "degrees to move (enter value >30 to exit): ";
        std::cin >> degrees;
        if (degrees > 30) {
            break;
        }
        kinect->setTiltDegrees(degrees);
    }

    return 0;
}
