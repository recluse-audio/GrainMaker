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

    class Params
    {
        int mBufferSize;
        int mHalfBufferSize;
        float* yinBuffer;
        float probability;
        float threshold;
    };

    PitchDetector();
    ~PitchDetector();

    void init(PitchDetector::Params* pParams, int bufferSize, float threshold);
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    void process(juce::AudioBuffer<float>& buffer);

    const double getCurrentPitch(PitchDetector::Params* pParams, float* buffer);

private:
    // Conditions of the environment
    double mSampleRate = DEFAULT_SAMPLE_RATE;
    int mBufferSize = DEFAULT_BUFFER_SIZE;
    int mHalfBufferSize = DEFAULT_BUFFER_SIZE / 2;

    // These are what we are calculating for
    std::atomic<double> mCurrentPitchHz = -1.0;
    std::atomic<double> mProbability = -1.0;


    void _difference(PitchDetector::Params* pParams, int* buffer);

    void _cumulativeMeanNormalizedDifference(PitchDetector::Params* pParams);

    int _absoluteThreshold(PitchDetector::Params* pParams);

    float _parabolicInterpolation(PitchDetector::Params* pParams, int tauEstimate);

    
};