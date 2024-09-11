//===================================
// RYAN DEVENS 2024-08-31

#include "Util/Juce_Header.h"
class CircularBuffer
{
public:
    CircularBuffer();
    ~CircularBuffer();

    // resizes when samplerate changes to make sure it is always sufficiently long enough
    void setSize(int numChannels, int numSamples);

    // writes buffer to private mCircularBuffer.  Except if the channel nums don't align, in which case it returns false.  Get that in order first
    bool pushBuffer(juce::AudioBuffer<float>& buffer);

    // reads buffer from private mCircularBuffer.  Except if the channel nums don't align, in which case it returns false.  Get that in order first
    // unlike readRange, this will update the local mReadPos
    bool popBuffer(juce::AudioBuffer<float>& buffer);

    // Reads audio data from circular buffer and writes it to the buffer passed in.
    // Starts at given readPos arg, fills entire buffer passed in - if circular buffer is smaller than buffer passed in, it will not clear or fill the remaining sample indices
    // Does not increment local mReadPos.  This could be useful for a multi-tap delay or PSOLA (hopefully!!!)
    bool readRange(juce::AudioBuffer<float>& buffer, int startingReadIndex);


private:
    friend class CircularBufferTest; // see test_CircularBuffer.cpp
    juce::AudioBuffer<float> mCircularBuffer;

    int mWritePos = 0;
    int mReadPos = 0;


    // Takes a reference to an incoming buffer, and writes it's audio data to the circular buffer
    // returns false if the channels aren't equal
    bool _writeToCircularBuffer(juce::AudioBuffer<float>& buffer);

    bool _readFromCircularBuffer(juce::AudioBuffer<float>& buffer, int startingReadIndex);

    // updates mReadPos and wraps it if necessary
    void _incrementReadPosition(int numSamples);


};