#include <libfreenect.hpp>
#include <iostream>
#include <memory>


const freenect_resolution RESOLUTION = FREENECT_RESOLUTION_MEDIUM;



class VRCarVision : public Freenect::FreenectDevice {
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

    void VideoCallback(void* newFrame, uint32_t timestamp) override 
    {
        if (_rgbData != nullptr) {
            uint8_t* frameData = static_cast<uint8_t*>(newFrame);
            std::copy(frameData, frameData + getVideoBufferSize(), _rgbData.get());
            _hasRGBData = true;
        }
    }

    void DepthCallback(void* newFrame, uint32_t timestamp)
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
        _rgbData.reset(new uint8_t[getVideoBufferSize()]);
        _depthData.reset(new uint8_t[getDepthBufferSize()]);
        startVideo();
        startDepth();
    }

    bool hasVideoData() { return _hasDepthData && _hasRGBData; }

    uint8_t* rgbData() { return _rgbData.get(); }
    uint8_t* depthData() { return _depthData.get(); }

private:
    bool _hasDepthData; // to avoid sending garbage, keep track of whether _videoData has been filled with real data or not
    bool _hasRGBData; 
    std::unique_ptr<uint8_t[]> _rgbData;
    std::unique_ptr<uint8_t[]> _depthData;
};



int main() 
{
    Freenect::Freenect freenect;
    VRCarVision* device = &freenect.createDevice<VRCarVision>(0);
    device->setLed(LED_RED);

    device->initializeVideo(RESOLUTION);

    device->setLed(LED_YELLOW);
    while (! device->hasVideoData()) {}
    device->setLed(LED_GREEN);

    while (1) {
        double degrees;
        std::cout << "degrees to move (enter value >30 to exit): ";
        std::cin >> degrees;
        if (degrees > 30) {
            break;
        }
        device->setTiltDegrees(degrees);
    }

    return 0;
}
