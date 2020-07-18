#include <libfreenect.hpp>
#include <iostream>

/*
class Controller : public FreenectDevice {

}
*/

int main() 
{
    std::cout << "test\n";
    Freenect::Freenect freenect;
    Freenect::FreenectDevice* device = &freenect.createDevice<Freenect::FreenectDevice>(0);
    device->setLed(LED_RED);
    return 0;
}
