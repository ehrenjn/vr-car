#include "TCPCommunicator.h"
#include <iostream> 


int main() {
    std::cout << "server main started" << std::endl;

    TCPServer server(7787);
    uint8_t messageArray[10];
    uint8_t receivedData[10];
    Message message = Message::createEmptyMessage(messageArray);

    std::cout << "server listening..." << std::endl;
    server.connectToClient();
    std::cout << "client connected" << std::endl;

    Message received = server.receiveMessage(receivedData);
    for (unsigned int i = 0; i < received.getDataLength(); i++) {
        std::cout << received.getStartOfData()[i];
    }
    std::cout << std::endl;

    uint8_t toSend[] = {'r', 'e', 's', 'p', 'o', 'n', 's', 'e'};
    message.setData(0, toSend, sizeof(toSend));
    server.sendMessage(message);

    return 0;
}