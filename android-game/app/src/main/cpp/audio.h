#pragma once

#include <aaudio/AAudio.h>

#include <atomic>

class AudioEngine {
public:
    bool start();
    void stop();
    void update(float speedMetersPerSecond, float throttle, bool horn);

private:
    static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream, void* userData, void* audioData, int32_t frameCount);
    aaudio_data_callback_result_t render(void* audioData, int32_t frameCount);

    AAudioStream* stream_ = nullptr;
    std::atomic<float> speed_{0.0f};
    std::atomic<float> throttle_{0.0f};
    std::atomic<bool> horn_{false};
    double motorPhase_ = 0.0;
    double hornPhase_ = 0.0;
};
