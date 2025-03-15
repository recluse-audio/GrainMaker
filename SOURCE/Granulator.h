/**
 * @file Granulator.h
 * @author Ryan Devens
 * @brief 
 * @version 0.1
 * @date 2025-03-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */


 #pragma once
 #include "Util/Juce_Header.h"


 /**
  * @brief You pump in audio to this class, it chops it up into grains.
  * 
  * It does not store the audio data, but keeps track of grain position and windows accordingly.
  * 
  * 
  * 
  */
 class Granulator
 {
public:
    Granulator();
    ~Granulator();

    void prepare(double sampleRate);
    const double getCurrentSampleRate();

    void setGrainLengthInMs(double lengthInMs);
    const int getGrainLengthInSamples();

    void setEmissionRateInHz(double rateInHz);
    const int getEmissionPeriodInSamples();

    void process(juce::AudioBuffer<float>& buffer);
private:
    double mSampleRate = -1;

    int mGrainLengthInSamples = -1; // length of audio segment being windowed
    /**
     * @brief Length between one grain emission and the next.
     * Can be longer, shorter that grain length itself.
     * 
     * If this is longer that the grain it will result in a tremolo like effect, adding silence between audible sounds
     * 
     */
    int mEmissionPeriodInSamples = -1; // length between one grain emission and the next.  
    int mEmissionRateInHz = -1; // using this for now so a change of samplerate can maintain the emission rate, redundant with mEmissionPeriodInSamples though

    int mCurrentPhaseIndex = 0; // Index within grain emission
    void _incrementPhase();
    
    // Called when emission rate or sample rate change, potentially more occasions
    void _updateEmissionPeriod();
};