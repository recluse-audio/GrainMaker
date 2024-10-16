//
//  Made by Ryan Devens, 2024-07-12 
//

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BUFFER_SIZE 512
#define DEFAULT_THRESHOLD 0.15

/*
    A class to detect and track pitch.
    ATP handles this with two classes I believe, but I will handle it will a single class and two modes.
    Detection and Tracking.

    There are different optimizations and callbacks once we have "detected" a pitch and enter tracking mode
*/

#include "Util/Juce_Header.h"
class PitchDetector
{
public:

    PitchDetector();
    ~PitchDetector();

    void prepareToPlay(double sampleRate, int blockSize);

    // split it up however you like, this tells you what pitch is the fundamental in that buffer.
    float process(juce::AudioBuffer<float>& buffer);

    const double getCurrentPitch();

private:
    // Conditions of the environment
    double mSampleRate = DEFAULT_SAMPLE_RATE;
    int mHalfBlock = 0;

    // Allowed amount of uncertainty, inverse of minimum probability needed to count as a pitch
    std::atomic<double> mThreshold = DEFAULT_THRESHOLD;

    // These are what we are calculating for
    std::atomic<double> mCurrentPitchHz = -1.0;
    std::atomic<double> mProbability = -1.0;

    juce::AudioBuffer<float> differenceBuffer;
    juce::AudioBuffer<float> cmndBuffer; // "Cumulative Mean Normalized Difference" buffer, you can thank YIN for this abbrev.


    
};