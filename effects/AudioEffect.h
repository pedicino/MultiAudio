#ifndef AUDIO_EFFECT_H
#define AUDIO_EFFECT_H

#include "../common.h"

#include <atomic>
#include <cstddef>

namespace audio {

/**
 * Abstract base class for audio effects.
 *
 * Defines a common interface for all audio processing effects.
 * Derived classes must implement the process method to apply
 * their specific audio transformation.
 */
class AudioEffect
{
protected:
    //--------------------------------------------------------------------------
    // Internal State
    //--------------------------------------------------------------------------
    unsigned int sampleRate;
    std::atomic<bool> effectActive;

public:
    //--------------------------------------------------------------------------
    // Lifecycle
    //--------------------------------------------------------------------------
    /**
     * Creates an audio effect with specified sample rate.
     * @param rate Sample rate in Hz (default: SAMPLE_RATE from common.h)
     */
    explicit AudioEffect(unsigned int rate = SAMPLE_RATE)
        : sampleRate(rate), effectActive(false) {}

    /**
     * Virtual destructor for proper polymorphic cleanup.
     */
    virtual ~AudioEffect() = default;

    //--------------------------------------------------------------------------
    // Audio Processing Interface
    //--------------------------------------------------------------------------
    /**
     * Processes a block of audio samples.
     * @param inputBuffer Source audio data
     * @param outputBuffer Destination for processed audio
     * @param numFrames Number of audio frames to process
     */
    virtual void process(const float* inputBuffer, float* outputBuffer, std::size_t numFrames) = 0;

    //--------------------------------------------------------------------------
    // Effect Control
    //--------------------------------------------------------------------------
    /**
     * Enables or disables the effect.
     * @param isEnabled true to enable, false to disable
     */
    virtual void setEnabled(bool isEnabled)
    {
        effectActive.store(isEnabled);
        if (!isEnabled)
        {
            reset();
        }
    }

    /**
     * Checks if the effect is currently enabled.
     * @return true if active, false otherwise
     */
    virtual bool isEnabled() const
    {
        return effectActive.load();
    }

    /**
     * Resets the internal state of the effect.
     * Derived classes should override to clear buffers, reset filters, etc.
     */
    virtual void reset()
    {
        // Base implementation does nothing
    }

    //--------------------------------------------------------------------------
    // Object Semantics
    //--------------------------------------------------------------------------
    AudioEffect(const AudioEffect&) = delete;
    AudioEffect& operator=(const AudioEffect&) = delete;
    AudioEffect(AudioEffect&&) = default;
    AudioEffect& operator=(AudioEffect&&) = default;
};

} // namespace audio

#endif // AUDIO_EFFECT_H
