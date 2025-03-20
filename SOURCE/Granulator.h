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
#include "../SUBMODULES/RD/SOURCE/Window.h"

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


    void setGrainShape(Window::Shape newShape);

    void prepare(double sampleRate);
    const double getCurrentSampleRate();

    void setGrainLengthInMs(double lengthInMs);
    void setGrainLengthInSamples(int numSamples);
    const int getGrainLengthInSamples();

    void setEmissionRateInHz(double rateInHz);
    void setEmissionPeriodInSamples(int numSamples);
    const int getEmissionPeriodInSamples();

    void process(juce::AudioBuffer<float>& buffer);
private:
    double mSampleRate = -1;
    Window mWindow;

    int mGrainLengthInSamples = -1; // length of audio segment being windowed
    /**
     * @brief Length between one grain emission and the next.
     * Can be longer, shorter that grain length itself.
     * 
     * If this is longer that the grain it will result in a tremolo like effect, adding silence between audible sounds
     * 
     */
    int mEmissionPeriodInSamples = -1; // length between one grain emission and the next.  

    int mCurrentPhaseIndex = 0; // Index within grain emission
    void _incrementPhase();



};