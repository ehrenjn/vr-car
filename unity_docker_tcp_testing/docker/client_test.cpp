#include "TCPCommunicator.h"
#include <iostream> 


int main() {
    std::cout << "client main started" << std::endl;

    uint8_t ip[] = {192,168,1,105};
    TCPClient client(ip, 7787);
    uint8_t messageArray[10];
    uint8_t receivedData[10];
    Message message = Message::createEmptyMessage(messageArray);

    uint8_t toSend[] = {'h', 'e', 'l', 'l', 'o'};
    message.setData(0, toSend, sizeof(toSend));
    client.sendMessage(message);

    Message received = client.receiveMessage(receivedData);
    for (unsigned int i = 0; i < received.getDataLength(); i++) {
        std::cout << received.getStartOfData()[i];
    }
    std::cout << std::endl;

    return 0;
}