#include <libfreenect.hpp>
#include <iostream>
#include <memory>

#include <sys/socket.h>
#include <netinet/in.h> // consts and structs for use with sockets
#include <errno.h> // to get error numbers after system call errors


const freenect_resolution RESOLUTION = FREENECT_RESOLUTION_MEDIUM;
unsigned short SERVER_PORT = 6969;


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


void runServer(VRCarVision* kinect) 
{
    int server = socket(AF_INET, SOCK_DGRAM, 0); // AF_INET = ipv4, SOCK_DGRAM = datagram socket, 0 = default protocol = UDP (since it's a datagram socket) 
    errIfNegative(server, "couldn't create server socket");
    
    // server IP object
    in_addr serverIp; // in_addr is a struct with few members but the only one we need to set is IP
    serverIp.s_addr = INADDR_ANY; // INADDR_ANY is equivilant to '0.0.0.0' (listen on all available IPs)

    // full server address struct
    sockaddr_in serverAddress; // sockaddr_in is an internet socket address
    serverAddress.sin_family = AF_INET; // AF_INET because internet socket
    serverAddress.sin_port = htons(SERVER_PORT); // htos to convert to big endian (aka network byte order)
    serverAddress.sin_addr = serverIp;

    // 0 fill garbarge part of address object that has to be all 0 (why does this exist?)
    uint8_t* sin_zeroStart = static_cast<uint8_t*>(serverAddress.sin_zero);
    uint8_t* sin_zeroEnd = sin_zeroStart + sizeof(serverAddress.sin_zero);
    for (uint8_t* chr = sin_zeroStart; chr < sin_zeroEnd; chr++) {
        *chr = '\0'; 
    }
    
    int result = bind(
        server, 
        reinterpret_cast<sockaddr*>(&serverAddress), // have to cast server address to more generic sockaddr type (CAN SAFELY USE reinterpret_cast HERE BECAUSE IT'S JUST GOING FROM ONE POINTER TO ANOTHER AND WE'RE PASSING IN THE SIZE OF serverAddress SO IT KNOWS HOW MUCH TO READ)
        sizeof(serverAddress)
    );
    errIfNegative(result, "binding server socket failed");

    /*
    char buffer[100];
    sockaddr_storage sender_address; // sender address will be stored in here when message is received
    socklen_t sender_address_length = sizeof(sender_address); // amazing that socklen_t is its own type
    recvfrom(
        server, buffer, sizeof(buffer), 0, // receive message using server, starting at address 0 of buffer
        reinterpret_cast<sockaddr*>(&sender_address), &sender_address_length // for some reason they want a pointer to the size of the address
    );
    */
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
