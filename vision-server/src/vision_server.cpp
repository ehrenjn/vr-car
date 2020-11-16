/*
WHAT IF THE SERVER IS SENDING DATA TOO FAST? IS THAT POSSIBLE?
    seems like it, maybe make a client first to check
    MAKE SURE TO CHECK HOW MUCH THE SERVER IS ACTUALLY SENDING WHEN TESTING CLIENT
    SHOULD ALSO GOOGLE ABOUT HOW VIDEO STREAMS ARE SUPPOSED TO WORK
        these systems aren't "supposed" to receive every bit of video... so missing bits becuase we're too slow actually makes sense
IT WOULD BE MORE OOP FOR SEND AND RECEIVE CALLS TO TAKE OR RETURN AN OBJECT THAT CONTAINS AN ADDRESS AND POINTER TO DATA, BUT MIGHT BE A MESS, IDK
    only bother doing this if you need to know length of message received I guess
udp only problems:
    THERES LOTS OF VISUAL GLITCHES RIGHT NOW, PROBABLY NEED SOME KIND OF CHECKSUM
    STREAM SEEMS TO GET LAGGIER WHEN IT'S ON FOR A WHILE???
    FOR SOME REASON THE VERY LAST (BOTTOM) SEGMENT OF DATA IS PICKED UP BY THE CLIENT LESS OFTEN THAN OTHER SEGMENTS?
    FOR SOME REASON WHEN THE CLIENT TRIES TO USE THE SIZE OF THE ACTUAL PACKET INSTEAD OF messageSize THATS SENT IN THE PACKET ITSELF THEN IT GETS AN EXTRA 3 BYTES... BUT ONLY ON THE FINAL SEGMENT
Use format FREENECT_DEPTH_11BIT_PACKED or FREENECT_DEPTH_10BIT_PACKED instead to save bandwidth
    Maybe do: Modify libfreenect to support 320x240 QVGA depth resolution like the official SDK
    640x480 has a lot of noise, not very worth the 4x bandwidth usage
CAN MAKE EVERYTHING MORE EFFICIENT BY HAVING A COMPLETELY SEPERATE BYTE ARRAY FOR HEADER AND DATA IN THE Message CLASS
    CAN DO THIS NOW BECAUSE TCP IS STREAM BASED
    might not be worth it because the copying doesn't seem to be wasting too much time... but it is kinda dumb so it'd be nice to avoid
DEPTH DATA DOESN'T MATCH UP WITH RGB DATA, GONNA HAVE TO USE MUTEXES OR WHATEVER
    should add 2 things:
        1.) lock method on the VRCarVision that would just set a boolean telling the rgb and depth callbacks to stop updating (this would basically solve your problem without any mutexes)
        2.) mutexes in depth and rgb callbacks so that we can't read potentially funky half written data (if you do this you would have to make the lock method incorporate the mutexes too but it would be pretty nice)
    Should probably use unique_lock instead of lock_guard but dunno yet
    Also almost all this stuff would be fixed by just having 2 message objects but locking is still nice for not having half read data... That being said it would still be nice to have 2 messages because it just looks better and the mutexes wouldnt be locked for as long
*/


#include <libfreenect.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <mutex>

#include <sys/socket.h>
#include <unistd.h> // defines close function (for all file descriptors)
#include <netinet/in.h> // consts and structs for use with sockets
#include <errno.h> // to get error numbers after system call errors


const freenect_resolution RESOLUTION = FREENECT_RESOLUTION_MEDIUM;
const unsigned short SEND_PORT = 6969;
const unsigned short RECEIVE_PORT = 7878;



class VRCarVision : public Freenect::FreenectDevice 
{
public:

    std::mutex depthDataMutex;
    std::mutex rgbDataMutex;

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
        std::lock_guard<std::mutex> lock(rgbDataMutex);
        if (_rgbData != nullptr) {
            uint8_t* frameData = static_cast<uint8_t*>(newFrame);
            std::copy(frameData, frameData + getVideoBufferSize(), _rgbData.get());
            _hasRGBData = true;
        }
    }

    void DepthCallback(void* newFrame, uint32_t)
    {
        std::lock_guard<std::mutex> lock(depthDataMutex);
        if (_depthData != nullptr) {
            uint8_t* frameData = static_cast<uint8_t*>(newFrame);
            std::copy(frameData, frameData + getDepthBufferSize(), _depthData.get());
            _hasDepthData = true;
        }
    }

    void initializeVideo(freenect_resolution resolution)
    {
        setVideoFormat(FREENECT_VIDEO_RGB, resolution);
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
    bool _hasDepthData; // to avoid sending garbage, keep track of whether _rgbData and _depthData have real data or not
    bool _hasRGBData; 
    std::unique_ptr<uint8_t[]> _rgbData;
    std::unique_ptr<uint8_t[]> _depthData;
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
const uint32_t MAX_INCOMING_MESSAGE_SIZE = 100; // The size of buffer to use for storing an incoming message
const uint32_t MAX_OUTGOING_MESSAGE_SIZE = 1000000;


// not using a POD because then serialization wouldn't be architecture agnostic
class Message 
{
private:
    uint8_t _messageType;
    uint8_t* _rawBytes;
    uint32_t _rawBytesLength;

    Message(uint8_t* backingArray) {
        _rawBytes = backingArray;
    }

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

        _rawBytesLength = dataLength + headerSize;
        *reinterpret_cast<uint32_t*>(nextAddress) = _rawBytesLength;
        nextAddress += sizeof(_rawBytesLength);

        std::copy(messageContents, messageContents + dataLength, nextAddress);
    }

    uint8_t* getSerialized() { return _rawBytes; }
    uint8_t* getStartOfData() { return _rawBytes + headerSize; }
    uint32_t getSerializedLength() { return _rawBytesLength; }
    uint32_t getDataLength() { return _rawBytesLength - headerSize; }
    uint8_t getMessageType() { return _messageType; }

    static const uint32_t headerSize = sizeof(_messageType) + sizeof(_rawBytesLength);
};


void printArray(uint8_t* array, uint32_t length)
{
    std::cout << "[";
    for (uint32_t i = 0; i < length; i++) {
        std::cout << (int) array[i];
        if (i != length - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]" << std::endl;
}


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
        errIfNegative(_listener, "couldn't create server socket");

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
        close_connection(); // close current _connection if it's connected to anything
        int result = listen(_listener, 0); // 2nd arg is how long the queue of pending connections should be (so that if you're running a server with multiple connections then other people can still try to connect even when you're not calling listen), this server only handles one client at a time so we can set it to 0
        errIfNegative(result, "couldn't listen for new connection");

        sockaddr_in clientAddress; // even though we're about to cast this to a sockaddr it actually matters that it starts out as a sockaddr_in because that's how accept() knows that we want an internet address (so we cant just make a raw sockaddr)
        socklen_t clientAddressLength = sizeof(clientAddress); // amazing that socklen_t is its own type
        _connection = accept(
            _listener, 
            reinterpret_cast<sockaddr*>(&clientAddress), 
            &clientAddressLength // for some reason they want a pointer to the size of the address
        );
        errIfNegative(_connection, "connecting to client failed");
    }


    void receive(uint8_t* messageBuffer, int numBytes)
    {
        int amountAcquiredLastRecv = 0;

        for (int totalBytesReceived = 0; totalBytesReceived < numBytes; totalBytesReceived += amountAcquiredLastRecv) {
            amountAcquiredLastRecv = _receive(
                messageBuffer + totalBytesReceived, 
                numBytes - totalBytesReceived
            );
            errIfNegative(amountAcquiredLastRecv, "recv failed"); 
        }
    }


    Message receiveMessage(uint8_t* backingArray)
    {
        receive(backingArray, Message::headerSize); // receive header
        Message message = Message::deserialize(backingArray); // parse header
        receive(message.getStartOfData(), message.getDataLength()); // receive rest of Message
        return message;
    }


    bool dataIsAvailable()
    {
        uint8_t dummyArray;
        int result = _receive(
            &dummyArray, 1,
            MSG_DONTWAIT | MSG_PEEK // MSG_DONTWAIT: make call nonblocking, MSG_PEEK: peek at data in socket queue instead of popping it
        );

        // check if a message was received
        if (result < 0 && (errno == EAGAIN  || errno == EWOULDBLOCK)) {
            return false;
        } else {
            errIfNegative(result, "nonblocking recv failed");
            return true;
        }
    }


    void sendData(uint8_t* messageStart, int messageLength)
    {
        int amountSentLastSend = 0;

        // need to call send multiple times because send will not send the whole message all at once if the message is too large
        for (int totalAmountSent = 0; totalAmountSent < messageLength; totalAmountSent += amountSentLastSend) {
            amountSentLastSend = send(
                _connection, 
                messageStart + totalAmountSent, // if the whole message isn't sent in the send() then we issue another that's only trying to send the rest of the message
                messageLength - totalAmountSent, 
                0 // no special flags
            );
            errIfNegative(amountSentLastSend, "send failed");
        }
    }


    void sendMessage(Message message) 
    {
        sendData(message.getSerialized(), message.getSerializedLength());
    }

    
    void stop() 
    {
        close_connection();
        errIfNegative(close(_listener), "closing _listener failed");
    }


private:
    int _listener; // file descriptor for the server's socket object
    int _connection; // file descriptor for the connected socket object (only 1 because only 1 client)


    void close_connection()
    {
        if (_connection >= 0) { // only close _connection if it's currently connected to something
            int result = close(_connection);
            errIfNegative(result, "closing _connection failed");
        }
    }


    int _receive(uint8_t* messageBuffer, int messageBufferLength, int flags = 0)
    {
        return recv(_connection, messageBuffer, messageBufferLength, flags);
    }
};



void runServer(VRCarVision* kinect) 
{
    std::cout << "server ready for connection" << std::endl;

    TCPServer sender(SEND_PORT);
    TCPServer receiver(RECEIVE_PORT);
    sender.connect_to_client();
    receiver.connect_to_client();

    std::cout << "client connected" << std::endl;

    uint8_t rgbMessageBackingArray[MAX_OUTGOING_MESSAGE_SIZE];
    uint8_t depthMessageBackingArray[MAX_OUTGOING_MESSAGE_SIZE]; // using 2 message buffers instead of reusing 1 for both rgb and depth because we want to keep mutexes locked for as short a time as possible and we can only do that if we can load depth and rgb data into Messages at the same time (without sending one of them)
    Message rbgMessageBuffer = Message::createEmptyMessage(rgbMessageBackingArray);
    Message depthMessageBuffer = Message::createEmptyMessage(depthMessageBackingArray);
    
    bool serverRunning = true;
    while (serverRunning) {

        {
            // make sure rgb and depth frames are finished writing by locking the data mutexes (also helps to ensure data for rgb and depth frames are more closely synced)
            std::lock_guard<std::mutex> rgbLock(kinect->rgbDataMutex);
            std::lock_guard<std::mutex> depthLock(kinect->depthDataMutex);
            rbgMessageBuffer.setData(DataType::RGB, kinect->rgbData(), kinect->rgbDataSize());
            depthMessageBuffer.setData(DataType::DEPTH, kinect->depthData(), kinect->depthDataSize());
        }
        sender.sendMessage(rbgMessageBuffer);
        sender.sendMessage(depthMessageBuffer);

        if (receiver.dataIsAvailable()) {

            uint8_t incomingMessageBackingArray[MAX_INCOMING_MESSAGE_SIZE];
            Message receivedMessage = receiver.receiveMessage(incomingMessageBackingArray);
            uint8_t messageType = receivedMessage.getMessageType();

            switch (messageType) {
                case DataType::DISCONNECT:
                    std::cout << "Recieved disconnect command. waiting for new client..." << std::endl;
                    sender.connect_to_client(); // wait for new client
                    receiver.connect_to_client();
                    kinect->setTiltDegrees(0);
                    std::cout << "new client connected" << std::endl;
                    break;
                case DataType::TILT: { // needs to be in its own scope or else the compiler will complain about the fact that we're initializing new variables which will still be accessible from other switch cases but will be initialized to garbage
                    uint8_t* tiltDataAddress = receivedMessage.getStartOfData();
                    signed char newTilt = *reinterpret_cast<signed char*>(tiltDataAddress);
                    std::cout << "Recieved tilt command: " << std::to_string(newTilt) << std::endl;
                    kinect->setTiltDegrees(newTilt);
                    break;
                }
                case DataType::STOP_SERVER:
                    std::cout << "Recieved stop server command." << std::endl;
                    serverRunning = false;
                    break;
                default:
                    std::cout << "Recieved unknown command: " << std::to_string(messageType) << std::endl;
                    break;
            }
        }
    }

    sender.stop();
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

    kinect->setTiltDegrees(0);

    runServer(kinect);

    return 0;
}
