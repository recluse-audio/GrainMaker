//
//  Made by Ryan Devens, 2024-07-12 
//

// #define DEFAULT_SAMPLE_RATE 44100
// #define DEFAULT_BUFFER_SIZE 512
// #define DEFAULT_THRESHOLD 0.15

// /*
//     A class to detect and track pitch.
//     ATP handles this with two classes I believe, but I will handle it will a single class and two modes.
//     Detection and Tracking.

//     There are different optimizations and callbacks once we have "detected" a pitch and enter tracking mode
// */

// #include "Util/Juce_Header.h"
// class PitchDetector
// {
// public:

//     PitchDetector();
//     ~PitchDetector();

//     void prepareToPlay(double sampleRate, int samplesPerBlock);

//     void process(juce::AudioBuffer<float>& buffer);

//     const double getCurrentPitch();

// private:
//     // Conditions of the environment
//     double mSampleRate = DEFAULT_SAMPLE_RATE;
//     int mBufferSize = DEFAULT_BUFFER_SIZE;
//     int mHalfBufferSize = DEFAULT_BUFFER_SIZE / 2; // save on division later?

//     // Allowed amount of uncertainty, inverse of minimum probability needed to count as a pitch
//     std::atomic<double> mThreshold = DEFAULT_THRESHOLD;

//     // These are what we are calculating for
//     std::atomic<double> mCurrentPitchHz = -1.0;
//     std::atomic<double> mProbability = -1.0;

//     // Used to hold Auto-Correlation calculations
//     std::vector<float> mYinBuffer;   
//     // These buffers are probably temporary, the operations can be done on a single buffer
//     std::vector<float> mDifference;
//     std::vector<float> mNormalizedDifference;

//     void _difference(juce::AudioBuffer<float>& buffer);

//     void _cumulativeMeanNormalizedDifference();

//     // Smallest tau above a threshold 
//     // This threshold is the acceptable amount of normalized difference
//     int _absoluteThreshold();

//     // 
//     float _getBestTau(int tauEstimate);

    
// };