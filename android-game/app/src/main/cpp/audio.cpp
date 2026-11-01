#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef __ANDROID__
#include <android/log.h>
#endif

bool AudioEngine::start() {
    if (stream_) return true;
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || !builder) return false;
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, 48000);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(builder, &AudioEngine::dataCallback, this);
    AAudioStreamBuilder_setErrorCallback(
        builder, [](AAudioStream*, void* userData, aaudio_result_t) {
            auto* engine = static_cast<AudioEngine*>(userData);
            engine->horn_.store(false, std::memory_order_relaxed);
        },
        this);
    const aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK || !stream_) {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_WARN, "IMISUBEBE",
                            "AAudio unavailable; gameplay continues without sound.");
#endif
        stream_ = nullptr;
        return false;
    }
    if (AAudioStream_requestStart(stream_) != AAUDIO_OK) {
        stop();
        return false;
    }
    return true;
}

void AudioEngine::stop() {
    if (!stream_) return;
    AAudioStream_requestStop(stream_);
    AAudioStream_close(stream_);
    stream_ = nullptr;
}

void AudioEngine::update(float speedMetersPerSecond, float throttle, bool horn) {
    speed_.store(std::abs(speedMetersPerSecond), std::memory_order_relaxed);
    throttle_.store(std::clamp(throttle, 0.0f, 1.0f), std::memory_order_relaxed);
    horn_.store(horn, std::memory_order_relaxed);
}

aaudio_data_callback_result_t AudioEngine::dataCallback(
    AAudioStream*, void* userData, void* audioData, int32_t frameCount) {
    return static_cast<AudioEngine*>(userData)->render(audioData, frameCount);
}

aaudio_data_callback_result_t AudioEngine::render(void* audioData, int32_t frameCount) {
    auto* output = static_cast<std::int16_t*>(audioData);
    const float speed = speed_.load(std::memory_order_relaxed);
    const float throttle = throttle_.load(std::memory_order_relaxed);
    const bool horn = horn_.load(std::memory_order_relaxed);
    const double motorFrequency = 58.0 + speed * 17.5 + throttle * 28.0;
    const double hornFrequency = 440.0;
    constexpr double sampleRate = 48000.0;
    const double motorStep = 2.0 * 3.14159265358979323846 * motorFrequency / sampleRate;
    const double hornStep = 2.0 * 3.14159265358979323846 * hornFrequency / sampleRate;
    const float motorVolume = speed < 0.15f ? 0.015f : (0.032f + throttle * 0.05f);
    const float hornVolume = horn ? 0.13f : 0.0f;
    for (int32_t frame = 0; frame < frameCount; ++frame) {
        const float motor = static_cast<float>(
            std::sin(motorPhase_) * 0.7 + std::sin(motorPhase_ * 2.03) * 0.2);
        const float hornWave = static_cast<float>(
            std::sin(hornPhase_) + std::sin(hornPhase_ * 1.48) * 0.55);
        const auto sample = static_cast<std::int16_t>(
            std::clamp((motor * motorVolume + hornWave * hornVolume) * 32767.0f,
                       -32767.0f, 32767.0f));
        output[frame * 2] = sample;
        output[frame * 2 + 1] = sample;
        motorPhase_ += motorStep;
        hornPhase_ += hornStep;
        if (motorPhase_ > 2.0 * 3.14159265358979323846) {
            motorPhase_ -= 2.0 * 3.14159265358979323846;
        }
        if (hornPhase_ > 2.0 * 3.14159265358979323846) {
            hornPhase_ -= 2.0 * 3.14159265358979323846;
        }
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
