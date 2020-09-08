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
WHEN RECEIVING COMMANDS KEEP TRACK OF COMMANDS IN A STRUCT AND USE A SWITCH INSTEAD OF IF ELSE 
    CAN USE SAME CLASS FOR SENDING AND RECEIVING MESSAGES
*/


#include <libfreenect.hpp>
#include <iostream>
#include <memory>
#include <string>

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

        // To do: Use format FREENECT_DEPTH_11BIT_PACKED or FREENECT_DEPTH_10BIT_PACKED instead to save bandwidth
        // Maybe do: Modify libfreenect to support 320x240 QVGA depth resolution like the official SDK
        //           640x480 has a lot of noise, not very worth the 4x bandwidth usage
        setDepthFormat(FREENECT_DEPTH_11BIT, resolution); 

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



class TCPServer 
{
public:

    TCPServer(unsigned short port) 
    {
        _connection = -1; // initialize connection to value that will throw error if send or receive are called before a connection is established
        
        _listener = socket(AF_INET, SOCK_STREAM, 0); // AF_INET = ipv4, SOCK_STREAM = stream socket (as opposed to datagram), 0 = default protocol = TCP (since it's a stream socket) 
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
            _listener, 
            reinterpret_cast<sockaddr*>(&serverAddress), // have to cast server address to more generic sockaddr type (CAN SAFELY USE reinterpret_cast HERE BECAUSE IT'S JUST GOING FROM ONE POINTER TO ANOTHER AND WE'RE PASSING IN THE SIZE OF serverAddress SO IT KNOWS HOW MUCH TO READ)
            sizeof(serverAddress)
        );
        errIfNegative(result, "binding server socket failed");
    }


    void connect_to_client() 
    {
        listen(_listener, 0); // 2nd arg is how long the queue of pending connections should be (so that if you're running a server with multiple connections then other people can still try to connect even when you're not calling listen), this server only handles one client at a time so we can set it to 0
        sockaddr_in clientAddress; // even though we're about to cast this to a sockaddr it actually matters that it starts out as a sockaddr_in because that's how accept() knows that we want an internet address (so we cant just make a raw sockaddr)
        socklen_t clientAddressLength = sizeof(clientAddress); // amazing that socklen_t is its own type
        _connection = accept(
            _listener, 
            reinterpret_cast<sockaddr*>(&clientAddress), 
            &clientAddressLength // for some reason they want a pointer to the size of the address
        );
        errIfNegative(_connection, "connecting to client failed");
    }


    int _receive(uint8_t* messageBuffer, int messageBufferLength, int flags = 0)
    {
        return recv(_connection, messageBuffer, messageBufferLength, flags);
    }


    void receive(uint8_t* messageBuffer, int messageBufferLength)
    {
        int result = _receive(messageBuffer, messageBufferLength);
        errIfNegative(result, "recv failed");
    }


    bool receiveNonBlocking(uint8_t* messageBuffer, int messageBufferLength)
    {
        int result = _receive(
            messageBuffer, messageBufferLength,
            MSG_DONTWAIT, // add special flag to make call nonblocking
        )

        if (result < 0 && (errno == EAGAIN  || errno == EWOULDBLOCK)) { // check if a message was received
            return false;
        }
        errIfNegative(result, "nonblocking recv failed");
        return true;
    }


    void send(uint8_t* messageStart, int messageLength)
    {
        int result = send(
            _connection, messageStart, messageLength, 
            0 // no special flags
        );
        errIfNegative(result, "send failed");
    }


private:
    int _listener; // file descriptor for the server's socket object
    int _connection; // file descriptor for the connected socket object (only 1 because only 1 client)
};



namespace DataType {
    // DataTypes for outgoing messages
    const uint8_t RGB = 'r';
    const uint8_t DEPTH = 'd';

    // DataTypes for incoming messages
    const uint8_t DISCONNECT = 'd';
    const uint8_t TILT = 't';
    const uint8_t STOP_SERVER = 's';
}


// these should be set to bigger numbers than we actually need because they need to take the amount of metadata in a Message object into account and that's prone to changing
const int MAX_INCOMING_MESSAGE_SIZE = 100; // The size of buffer to use for storing an incoming message
const int MAX_OUTGOING_MESSAGE_SIZE = ;


// not using a POD because then serialization wouldn't be architecture agnostic
class Message 
{
public:

    // creates an empty message using the specified backing array
    static Message createEmptyMessage(uint8_t* backingArray) {
        return Message(backingArray);
    }

    // creates a new Message by wrapping a byte array from an already serialized Message
    static Message deserialize(uint8_t* rawData) {
        Message message(rawData);

        // update metadata
        uint8_t* nextAddress = message._rawBytes;
        message._messageType = *nextAddress;
        nextAddress += sizeof(_messageType);
        message._rawBytesLength = *reinterpret_cast<uint32_t*>(nextAddress);

        return message;
    }

    void setData(uint8_t messageType, uint8_t* messageContents, uint32_t dataLength) {
        uint8_t* nextAddress = _rawBytes;

        _messageType = messageType;
        *nextAddress = _messageType;
        nextAddress += sizeof(_messageType);

        _rawBytesLength = dataLength + _totalMetaDataSize;
        *nextAddress = *reinterpret_cast<uint32_t*>(_rawBytesLength);
        nextAddress += sizeof(_rawBytesLength);

        std::copy(nextAddress, nextAddress + dataLength, messageContents);
    }

    uint8_t* getSerialized() { return _rawBytes; }
    uint8_t* getStartOfData() { return _rawBytes + _totalMetaDataSize; }
    uint32_t getSerializedLength() { return _rawBytesLength; }
    uint8_t getMessageType() { return _messageType; }

private:
    uint8_t _messageType;
    uint8_t* _rawBytes;
    uint32_t _rawBytesLength;
    static const uint32_t _totalMetaDataSize = sizeof(_messageType) + sizeof(_rawBytesLength);

    Message(uint8_t* backingArray) {
        _rawBytes = backingArray;
    }
}



void runServer(VRCarVision* kinect) 
{
    TCPServer server(SERVER_PORT);
    server.connect_to_client();

    uint8_t outgoingMessageBackingArray[MAX_OUTGOING_MESSAGE_SIZE];
    Message messageBuffer = Message.createEmptyMessage(outgoingMessageBackingArray);
    
    bool serverRunning = true;
    while (serverRunning) {

        messageBuffer.setData(DataType::RGB, kinect->rgbData(), kinect->rgbDataSize());
        server.send(messageBuffer.getSerialized(), messageBuffer.getSerializedLength());

        messageBuffer.setData(DataType::DEPTH, kinect->depthData(), kinect->depthDataSize());
        server.send(messageBuffer.getSerialized(), messageBuffer.getSerializedLength());

        uint8_t message[MAX_INCOMING_MESSAGE_SIZE];
        bool messageReceived = server.receiveNonBlocking((uint8_t*) message, sizeof(message));
        if (messageReceived) {
            Message receivedMessage = Message.deserialize(message);
            switch (receivedMessage.getMessageType()) {
                case DataType.DISCONNECT:
                    std::cout << "Recieved disconnect command. waiting for new client..." << std::endl;
                    server.connect_to_client(); // wait for new client
                    kinect->setTiltDegrees(0);
                    std::cout << "new client connected" << std::endl;
                    break;
                case DataType.TILT:
                    signed char newTilt = *std::static_cast<(signed char)*>(message.getStartOfData());
                    std::cout << "Recieved tilt command: " << std::to_string(newTilt) << std::endl;
                    kinect->setTiltDegrees((float) newTilt);
                    break;
                case DataType.STOP_SERVER:
                    std::cout << "Recieved stop server command." << std::endl;
                    serverRunning = false;
                    break;
                case default:
                    std::cout << "Recieved unknown command: " << std::to_string(messageType) << std::endl;
                    break;
            }
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

    // Temp code: Set to true if you want to manually set the tilt on startup. Otherwise tilt is set to 0 deg
    if ( 0 ) {
        while (1) {
            double degrees;
            std::cout << "degrees to move (enter value >30 to exit): ";
            std::cin >> degrees;
            if (degrees <= 30 && degrees >= -30) {
                kinect->setTiltDegrees(degrees);
                break;
            }
        }
    } else {
        kinect->setTiltDegrees(0);
    }

    runServer(kinect);

    return 0;
}
