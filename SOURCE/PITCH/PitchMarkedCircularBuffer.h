#pragma once
#include "UTIL/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/CircularBuffer.h"


/**
 * @brief A purpose built class that is an owner of a CircularBuffer.  
 * It interfaces with rather than inherits the CircularBuffer for simplicity as of right now
 * 
 * In addition to the inheritance from the CircularBuffer, it stores PitchMarks, a simple struct that 
 * records what pitch was pushed into the buffer and when it occurred.
 * 
 * Still not sure if this should inherit or own two circular buffers.  Choosing to inherit for now.
 * Conceptually this means that the PitchMarkedCircularBuffer is just one circular
 */
 

class PitchMarkedCircularBuffer 
{
public:
    enum class MarkingRule
    {
        kNone = 0,
        kMagnitude = 1,
    };

    PitchMarkedCircularBuffer();
    ~PitchMarkedCircularBuffer();

    void prepare(double sampleRate, int maxBlockSize);

    // sets size of internal buffers
    void setSize(int numChannels, int numSamples);

    // Allows you to push a buffer of audio samples to this circular buffer
    // Notice this does not override "pushBuffer" from the base class
    bool pushBufferAndPeriod(juce::AudioBuffer<float>& buffer, float periodInSamples);

    // fills buffer with audio data starting from mReadPos over the range of the buffer's length in samples
    // returns float estimate of this range
    float popBufferAndPitch(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& pitchBuffer);

    void popAudioBuffer(juce::AudioBuffer<float>& buffer);

    void setMarkingRule(PitchMarkedCircularBuffer::MarkingRule newRule);

    PitchMarkedCircularBuffer::MarkingRule getMarkingRule();

    void checkWritePos(int& audioWritePos, int& pitchMarkWritePos);

private: 
    friend class PitchBufferTest; 
    friend class SineWavePitchBuffer;

    MarkingRule mMarkingRule;

    // reads audioData and accordingly marks mPitchBuffer by writing to an AudioBlock taken from its internal buffer
    void _pitchMarkBlock(juce::dsp::AudioBlock<float> audioData, juce::dsp::AudioBlock<float> markData, int period);
    
    // given two buffers, this will clear and resize them to be the same size as the underlying circular buffers
    // fills them with the corresponding audio and pitch data.  
    // initially making this for testing purposes
    void _loadDataIntoBuffer(juce::AudioBuffer<float>& audioBuffer, juce::AudioBuffer<float>& pitchBuffer);


    // Returns sample index in a given range of a buffer. Index is relative to 'start' argument, not buffer[0][0]
    // TODO: handle stereo 
    int _findMaxSampleIndex(juce::AudioBuffer<float>& buffer, int start, int length);

    // temp buffer allocated ahead of time to do block sized markings and push into mPitchBuffer.
    juce::AudioBuffer<float> mMarkingBuffer;
    CircularBuffer mAudioBuffer;
    CircularBuffer mPitchBuffer;
};