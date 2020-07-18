#include <libfreenect.hpp>
#include <iostream>
#include <vector>
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

    void VideoCallback(void* rgbData, uint32_t timestamp) override 
    {
        if (auto videoData = _videoData.lock()) { // create shared_ptr if _videoData is null or deleted
            /*Mutex::ScopedLock lock(m_rgb_mutex);
            uint8_t* rgb = static_cast<uint8_t*>(_rgb);
            std::copy(rgb, rgb+getVideoBufferSize(), m_buffer_video.begin());
            m_new_rgb_frame = true;*/
            _hasRGBData = true;
        }
    }

    void DepthCallback(void* depthData, uint32_t timestamp)
    {
        if (auto videoData = _videoData.lock()) { // create shared_ptr if _videoData is null or deleted
            _hasDepthData = true;
        }
    }

    void initializeVideo(std::weak_ptr<std::vector<uint8_t>> _videoData, freenect_resolution resolution)
    {
        this->_videoData = _videoData;
        setVideoFormat(FREENECT_VIDEO_RGB, resolution);
        startVideo();
        startDepth();
    }

    bool hasVideoData() { return _hasDepthData && _hasRGBData; }

private:
    std::weak_ptr<std::vector<uint8_t>> _videoData; // HAVE TO DO THIS WHOLE POINTER THING BECAUSE VRCarVision CAN ONLY BE CONSTRUCTED USING freenect.createDevice SO CONSTRUCTOR CANT TAKE ANY REFERENCES WHICH MEANS _videoData CANT BE A REFERENCE
    bool _hasDepthData; // to avoid sending garbage, keep track of whether _videoData has been filled with real data or not
    bool _hasRGBData; 
};



int main() 
{
    Freenect::Freenect freenect;
    VRCarVision* device = &freenect.createDevice<VRCarVision>(0);
    device->setLed(LED_RED);

    std::shared_ptr<std::vector<uint8_t>> videoData(
        new std::vector<uint8_t> // HAVE TO ALLOCATE FROM HEAP because otherwise we'd be tossing around a variable AND a shared_ptr to that variable and when they both go out of scope the original variable will be destructed first, leading to the shared_ptr throwing an error about being unable to free()
    );
    device->initializeVideo(videoData, RESOLUTION);

    device->setLed(LED_YELLOW);
    while (! device->hasVideoData()) {}
    device->setLed(LED_GREEN);

    while (1) {
        double degrees;
        std::cout << "degrees to move: ";
        std::cin >> degrees;
        if (degrees > 30) {
            break;
        }
        device->setTiltDegrees(degrees);
    }

    return 0;
}
