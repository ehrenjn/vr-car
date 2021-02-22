#include "TCPCommunicator.h"
#include <iostream> 


int main() {
    std::cout << "server main startedn" << std::endl;

    TCPServer server(7787);
    uint8_t messageArray[10];
    uint8_t receivedData[10];
    Message message = Message::createEmptyMessage(messageArray);

    std::cout << "server listening..." << std::endl;
    server.connectToClient();
    std::cout << "client connected" << std::endl;

    Message received = server.receiveMessage(receivedData);
	std::cout << "loop" << std::endl;
    for (unsigned int i = 0; i < received.getDataLength(); i++) {
        std::cout << received.getStartOfData()[i];
		std::cout << "," << std::endl;
    }
    std::cout << std::endl;
	std::cout << "resp" << std::endl;

    uint8_t toSend[] = {'r', 'e', 's', 'p', 'o', 'n', 's', 'e'};
    message.setData(0, toSend, sizeof(toSend));
    server.sendMessage(message);
	std::cout << "done" << std::endl;

    return 0;
}