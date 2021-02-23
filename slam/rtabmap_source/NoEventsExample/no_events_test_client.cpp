#include "communicator/TCPCommunicator.h"
#include <iostream> 


int main() {
    std::cout << "client main started" << std::endl;

    uint8_t ip[] = {127, 0, 0, 1};
    std::cout << "connecting to server...";
    TCPClient client(ip, 7787);
    std::cout << "connected to server";
    uint8_t receivedData[1000000];

    while(true) {
        Message received = client.receiveMessage(receivedData);
        int num_floats_received = received.getDataLength() / sizeof(float);
        float* next_float = reinterpret_cast<float*>(received.getStartOfData());
        float* last_float = next_float + num_floats_received;

        /*
        for (; next_float < last_float; next_float++) {
            std::cout << *next_float << "(" << *reinterpret_cast<uint32_t*>(next_float) << ")" << std::endl;
        }
        */
        for (int i = 0; i < 7; i++) {
            std::cout << next_float[i] << std::endl;
        }

        std::cout << "amount of data (bytes): " << received.getDataLength() << std::endl;
        std::cout << std::endl;
    }

    return 0;
}