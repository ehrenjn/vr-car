/*
WHAT IF THE SERVER IS SENDING DATA TOO FAST? IS THAT POSSIBLE?
    seems like it, maybe make a client first to check
    MAKE SURE TO CHECK HOW MUCH THE SERVER IS ACTUALLY SENDING WHEN TESTING CLIENT
    SHOULD ALSO GOOGLE ABOUT HOW VIDEO STREAMS ARE SUPPOSED TO WORK
        these systems aren't "supposed" to receive every bit of video... so missing bits becuase we're too slow actually makes sense
IT WOULD BE MORE OOP FOR SEND AND RECEIVE CALLS TO TAKE OR RETURN AN OBJECT THAT CONTAINS AN ADDRESS AND POINTER TO DATA, BUT MIGHT BE A MESS, IDK
    only bother doing this if you need to know length of message received I guess
DATA STRUCTURE PADDING IN PartialArrayMessageBuffer MIGHT GET WEIRD IF ARCHETECTURES DONT PLAY WELL TOGETHER, MIGHT WANT TO DO PROPER SERIALIZATION (but thats annoying)
    proper serialization would be close to what I had before realizing I could use POD
THERES LOTS OF VISUAL GLITCHES RIGHT NOW, PROBABLY NEED SOME KIND OF CHECKSUM
STREAM SEEMS TO GET LAGGIER WHEN IT'S ON FOR A WHILE???
FOR SOME REASON THE VERY LAST (BOTTOM) SEGMENT OF DATA IS PICKED UP BY THE CLIENT LESS OFTEN THAN OTHER SEGMENTS?
FOR SOME REASON WHEN THE CLIENT TRIES TO USE THE SIZE OF THE ACTUAL PACKET INSTEAD OF messageSize THATS SENT IN THE PACKET ITSELF THEN IT GETS AN EXTRA 3 BYTES... BUT ONLY ON THE FINAL SEGMENT
*/


#include <libfreenect.hpp>
#include <iostream>
#include <memory>

#include <sys/socket.h>
#include <netinet/in.h> // consts and structs for use with sockets
#include <errno.h> // to get error numbers after system call errors


const freenect_resolution RESOLUTION = FREENECT_RESOLUTION_MEDIUM;
const unsigned short SERVER_PORT = 6969;


class VRCarVision : public Freenect::FreenectDevice 
{
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

    void VideoCallback(void* newFrame, uint32_t) override 
    {
        if (_rgbData != nullptr) {
            uint8_t* frameData = static_cast<uint8_t*>(newFrame);
            std::copy(frameData, frameData + getVideoBufferSize(), _rgbData.get());
            _hasRGBData = true;
        }
    }

    void DepthCallback(void* newFrame, uint32_t)
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



class UDPServer 
{
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


    void send(sockaddr_storage& clientAddress, uint8_t* messageStart, int messageLength)
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



namespace DataType {
    const uint8_t RGB = 'r';
    const uint8_t DEPTH = 'd';
}


const int MESSAGE_CHUNK_SIZE = 49998; // should be divisable by 6 (rgb triplets must be divisible by 3, 16 bit depth data must be divisible by 2)

// needs to be POD in order to be reliably serialized
struct PartialArrayMessageBuffer
{
    uint8_t dataType;
    uint32_t initialIndex;
    uint32_t messageLength;
    uint8_t message[MESSAGE_CHUNK_SIZE];

    void setMessage(uint8_t* array, int arraySize, uint8_t* startAddress, uint8_t dataType)
    {
        // calculate final address in array for copying
        uint8_t* arrayEnd = array + arraySize;
        uint8_t* finalAddress;
        if ((startAddress + MESSAGE_CHUNK_SIZE) < arrayEnd)
            finalAddress = startAddress + MESSAGE_CHUNK_SIZE;
        else
            finalAddress = arrayEnd;
        
        // update data
        std::copy(startAddress, finalAddress, this->message);
        this->dataType = dataType;
        this->initialIndex = startAddress - array;
        this->messageLength = finalAddress - startAddress;
    }

    int getTotalLength()
    {
        return (sizeof(PartialArrayMessageBuffer) - MESSAGE_CHUNK_SIZE) + this->messageLength;
    }
};

// ensure that PartialArrayMessageBuffer is plain old data at compile time
static_assert(std::is_pod<PartialArrayMessageBuffer>::value, "PartialArrayMessageBuffer is not POD");



void runServer(VRCarVision* kinect) 
{
    UDPServer server(SERVER_PORT);
    auto client = server.receive();
    PartialArrayMessageBuffer messageBuffer;
    std::cout << kinect->depthDataSize() << std::endl;

    for(;;) {

        uint8_t* dataEnd = kinect->rgbData() + kinect->rgbDataSize();
        for (uint8_t* messageStart = kinect->rgbData(); messageStart < dataEnd; messageStart += MESSAGE_CHUNK_SIZE) {
            messageBuffer.setMessage(kinect->rgbData(), kinect->rgbDataSize(), messageStart, DataType::RGB);
            server.send(client, reinterpret_cast<uint8_t*>(&messageBuffer), messageBuffer.getTotalLength());
        }

	    dataEnd = kinect->depthData() + kinect->depthDataSize();
        for (uint8_t* messageStart = kinect->depthData(); messageStart < dataEnd; messageStart += MESSAGE_CHUNK_SIZE) {
            messageBuffer.setMessage(kinect->depthData(), kinect->depthDataSize(), messageStart, DataType::DEPTH);
            server.send(client, reinterpret_cast<uint8_t*>(&messageBuffer), messageBuffer.getTotalLength());
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
