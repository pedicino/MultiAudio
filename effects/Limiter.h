#ifndef LIMITER_H
#define LIMITER_H

#include "../common.h" 
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>

/*
Limiter Effect -
    - Prevents audio signal from exceeding a set threshold.
    - Uses gain reduction based on peak level within a buffer.
    - Includes attack and release times for smooth gain changes.
*/
class Limiter
{
private:
    unsigned int sampleRate;
    float threshold;       
    float attackTimeMs;    
    float releaseTimeMs;    
    float attackCoeff;     
    float releaseCoeff;     
    float currentGain;     
    std::atomic<bool> limiterActive; 

    void calculateCoeffs();

public:
    Limiter(unsigned int rate = SAMPLE_RATE,
            float thresh = 0.02f,   
            float attackMs = 5.0f, 
            float releaseMs = 100.0f);

    ~Limiter() = default;

    void process(const float* inputBuffer, float* outputBuffer, size_t bufferSize);

    void setThreshold(float newThreshold);
    float getThreshold() const;

    void setAttackTime(float ms);
    float getAttackTime() const;

    void setReleaseTime(float ms);
    float getReleaseTime() const;

    void setEnabled(bool isEnabled);
    bool isEnabled() const;
};

#endif