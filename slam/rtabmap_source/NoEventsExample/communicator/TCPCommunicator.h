#pragma once

#include <stdint.h>
#include <netinet/in.h> // consts and structs for use with sockets


// not using a POD because then serialization wouldn't be architecture agnostic
class Message 
{

private:

    uint8_t _messageType;
    uint8_t* _rawBytes;
    uint32_t _rawBytesLength;

    Message(uint8_t* backingArray) // private functions must be defined in header file 
    {
        _rawBytes = backingArray;
    }

public:

    // creates an empty message using the specified backing array
    static Message createEmptyMessage(uint8_t* backingArray);

    // creates a new Message by wrapping a byte array from an already serialized Message
    static Message deserialize(uint8_t* rawData);

    // safe method of setting data and metadata (but messageContents array will be iterated through)
    void setData(uint8_t messageType, uint8_t* messageContents, uint32_t dataLength);

    // dangerous way of setting metadata (advantage is that you can call this and then use getStartOfData() to update the data directly, so you dont need to build an extra array just to set the data)
    void setMetaData(uint8_t messageType, uint32_t dataLength);

    uint8_t* getSerialized();
    uint8_t* getStartOfData();
    uint32_t getSerializedLength();
    uint32_t getDataLength();
    uint8_t getMessageType();

    static const uint32_t headerSize = sizeof(_messageType) + sizeof(_rawBytesLength);
};



class TCPCommunicator
{

public:

    TCPCommunicator();

    void receive(uint8_t* messageBuffer, int numBytes);
    Message receiveMessage(uint8_t* backingArray);

    bool dataIsAvailable();

    void sendData(uint8_t* messageStart, int messageLength);
    void sendMessage(Message message);

    void closeConnection();

    static sockaddr_in createAddressObject(uint32_t ip, unsigned short port, bool ipIsNetworkByteOrder=true);

protected:

    int _connection; // file descriptor for the connected socket object (only need 1 server for client and 1 client for server)

    int _receive(uint8_t* messageBuffer, int messageBufferLength, int flags = 0);
};



class TCPServer: public TCPCommunicator
{

public:

    TCPServer(unsigned short port);

    void connectToClient();

    void stop();

private:

    int _listener; // file descriptor for the server's socket object
};




class TCPClient: public TCPCommunicator
{
public:
    TCPClient(uint8_t ip[4], unsigned short port);
};