/*
Really nasty that vision_server doesn't use this code but code it's faster for now to just keep everything seperate
*/

#include <sys/socket.h>
#include <unistd.h> // defines close function (for all file descriptors)
#include <netinet/in.h> // consts and structs for use with sockets
#include <errno.h> // to get error numbers after system call errors

#include <iostream> 
#include <cstdlib> // for exit()
#include <cstring> // for memset()

#include "TCPCommunicator.h"




void errIfNegative(const int val, const char* errorMessage) 
{
    if (val < 0) {
        std::cerr << "ERROR: " << errorMessage << std::endl; // print to stderr instead of stdout
        std::cerr << "last error code: " << errno << std::endl; // print latest system error
        exit(69); // a classic
    }
}




Message Message::createEmptyMessage(uint8_t* backingArray) 
{
    return Message(backingArray);
}


Message Message::deserialize(uint8_t* rawData) 
{
    Message message(rawData);

    // update metadata
    uint8_t* nextAddress = message._rawBytes;
    message._messageType = *nextAddress;
    nextAddress += sizeof(_messageType);
    message._rawBytesLength = *reinterpret_cast<uint32_t*>(nextAddress);

    return message;
}


void Message::setData(uint8_t messageType, uint8_t* messageContents, uint32_t dataLength) 
{
    uint8_t* nextAddress = _rawBytes;

    _messageType = messageType;
    *nextAddress = _messageType;
    nextAddress += sizeof(_messageType);

    _rawBytesLength = dataLength + headerSize;
    *reinterpret_cast<uint32_t*>(nextAddress) = _rawBytesLength;
    nextAddress += sizeof(_rawBytesLength);

    std::copy(messageContents, messageContents + dataLength, nextAddress);
}


uint8_t* Message::getSerialized() { return _rawBytes; }
uint8_t* Message::getStartOfData() { return _rawBytes + headerSize; }
uint32_t Message::getSerializedLength() { return _rawBytesLength; }
uint32_t Message::getDataLength() { return _rawBytesLength - headerSize; }
uint8_t Message::getMessageType() { return _messageType; }





TCPCommunicator::TCPCommunicator() 
{
    _connection = -1; // initialize connection to value that will throw error if send or receive are called before a connection is established
}


void TCPCommunicator::receive(uint8_t* messageBuffer, int numBytes)
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


Message TCPCommunicator::receiveMessage(uint8_t* backingArray)
{
    receive(backingArray, Message::headerSize); // receive header
    Message message = Message::deserialize(backingArray); // parse header
    receive(message.getStartOfData(), message.getDataLength()); // receive rest of Message
    return message;
}


bool TCPCommunicator::dataIsAvailable()
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


void TCPCommunicator::sendData(uint8_t* messageStart, int messageLength)
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


void TCPCommunicator::sendMessage(Message message) 
{
    sendData(message.getSerialized(), message.getSerializedLength());
}


void TCPCommunicator::closeConnection()
{
    if (_connection >= 0) { // only close _connection if it's currently connected to something
        int result = close(_connection);
        errIfNegative(result, "closing _connection failed");
    }
}


sockaddr_in TCPCommunicator::createAddressObject(uint32_t ip, unsigned short port, bool ipIsNetworkByteOrder) 
{
    sockaddr_in address; // sockaddr_in is an internet socket address

    // 0 fill whole struct because theres a garbage part of the object that has to be all 0 (why does this exist?)
    memset(
        reinterpret_cast<uint8_t*>(&address), 
        0, sizeof(address)
    );

    // convert ip to network byte order if it isn't already (FYI IF YOU CAST A uint8_t[4] TO A uint32_t IT WILL ALREADY BE NETWORK BYTE ORDER SO NO NEED TO DO THIS MOST OF THE TIME)
    if (! ipIsNetworkByteOrder) {
        ip = htonl(ip);
    }

    // fill struct
    address.sin_family = AF_INET; // AF_INET because internet socket
    address.sin_port = htons(port); // htons to convert to big endian (aka network byte order)
    address.sin_addr.s_addr = ip; // must be in network byte order

    return address;
}


int TCPCommunicator::_receive(uint8_t* messageBuffer, int messageBufferLength, int flags)
{
    return recv(_connection, messageBuffer, messageBufferLength, flags);
}




TCPServer::TCPServer(unsigned short port)
{
    _listener = socket(AF_INET, SOCK_STREAM, 0); // AF_INET = ipv4, SOCK_STREAM = stream socket (as opposed to datagram), 0 = default protocol = TCP (since it's a stream socket) 
    errIfNegative(_listener, "couldn't create server socket");

    sockaddr_in serverAddress = createAddressObject(
        INADDR_ANY, // INADDR_ANY is equivilant to '0.0.0.0' (listen on all available IPs)
        port, true // true because INADDR_ANY is network byte order already
    );
    
    // bind to address
    int result = bind(
        _listener, 
        reinterpret_cast<sockaddr*>(&serverAddress), // have to cast server address to more generic sockaddr type (CAN SAFELY USE reinterpret_cast HERE BECAUSE IT'S JUST GOING FROM ONE POINTER TO ANOTHER AND WE'RE PASSING IN THE SIZE OF serverAddress SO IT KNOWS HOW MUCH TO READ)
        sizeof(serverAddress)
    );
    errIfNegative(result, "binding server socket failed");
}


void TCPServer::connectToClient() 
{
    closeConnection(); // close current _connection if it's connected to anything
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


void TCPServer::stop() 
{
    closeConnection();
    errIfNegative(close(_listener), "closing _listener failed");
}




TCPClient::TCPClient(uint8_t ip[4], unsigned short port) {
    _connection = socket(AF_INET, SOCK_STREAM, 0); // same as server
    errIfNegative(_connection, "couldn't create client socket");

    sockaddr_in serverAddress = createAddressObject(
        *reinterpret_cast<uint32_t*>(ip), // casting an array to a uint32_t will actually put it in network byte order for us so we can use default ipIsNetworkByteOrder (true)
        port
    );

    // connect to server
    int result = connect(
        _connection, 
        reinterpret_cast<sockaddr*>(&serverAddress), // same weirdness as when calling bind() in the server
        sizeof(serverAddress)
    );
    errIfNegative(result, "connecting to server failed");
}

