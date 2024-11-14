/**
 * @file GrainCorrector.h
 * @author ryandevens
 * @brief 
 * @version 0.1
 * @date 2024-10-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "UTIL/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "PitchMarkedCircularBuffer.h"

static int kOUTPUT_DELAY = 512;

/**
 * @brief A class for pitch correction using a granula psola method.
 * 
 * 
 *  Doing it with transposition only first
 */

class GrainCorrector
{
public:
    GrainCorrector(PitchMarkedCircularBuffer& pitchMarkedBuffer);
    ~GrainCorrector();

    /**
     * @brief You may choose to call this in a juce prepareToPlay().
     * But the idea is that you are not limited to the processBlock() size
     * 
     * Maybe this feeds into another circular buffer that allows for grains larger than this
     * 
     * @param sampleRate 
     * @param maxBufferSize 
     */
    void prepare(double sampleRate, int maxBufferSize);

    /**
     * @brief Set the look-a-head for this corrector.
     * Functionally this produces an output delay so I am calling
     * the local variable 'mOutputDelay'
     * 
     * @param outputDelay 
     */
    void setOutputDelay(int outputDelay);

    ////////////////
    /**
     * 
     */
    void process(juce::AudioBuffer<float>& processBuffer);
    /**
     * @brief This takes an input of audio signal that has been analyzed for pitch. (presumably)
     * Get weird if you want I guess!
     * 
     * HINT: This is not directly referencing the PitchMarkedCircularBuffer, but that would make
     * getting these two in syncronized fashion easier.
     * 
     * It will create grains that are not more than half the buffers in length.  
     * then it will make grains and overlap them to pitch up and down 
     * 
     * @param audioBuffer 
     * @param pitchBuffer 
     */
    void correct(juce::AudioBuffer<float>& audioBuffer, juce::AudioBuffer<float>& pitchBuffer);

    /**
     * @brief Sets internal m
     * 
     */

private:
    friend class GrainCorrectorTest;
    PitchMarkedCircularBuffer& mPitchMarkedBuffer;
    Window mWindow;
    
    // For reading from PitchMarkedCircularBuffer at the block level.
    // Enable granulation and separate it from the pitch buffer.
    juce::AudioBuffer<float> mAnalysisAudioBuffer;
    juce::AudioBuffer<float> mAnalysisPitchMarksBuffer;

    // This is supposed to allow correct resizing of anaylsys buffers after calling
    // prepare() or setOutputDelay() in either order
    void _updateAnalysisSize();

    // 
    void _makeGrain(juce::dsp::AudioBlock<float> readBlock, juce::dsp::AudioBlock<float> writeBlock);

    float mTransposeRatio = 1.0; // default, no transpose
    int mOutputDelay = 0;
    // storing this so mOutputDelay can be adjusted and analysis buffers can be resized accurately outside prepare()
    int mProcessBlockSize = 0; 


};