#include <libfreenect.hpp>
#include <iostream>

/*
class Controller : public FreenectDevice {

}
*/

int main() 
{
    Freenect::Freenect freenect;
    Freenect::FreenectDevice* device = &freenect.createDevice<Freenect::FreenectDevice>(0);
    device->setLed(LED_GREEN);
    while (1) {
        double degrees;
        std::cout << "degrees to move: ";
        std::cin >> degrees;
        device->setTiltDegrees(degrees);
    }
    return 0;
}
