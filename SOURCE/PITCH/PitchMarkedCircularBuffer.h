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

    PitchMarkedCircularBuffer();
    ~PitchMarkedCircularBuffer();

    // sets size of internal buffers
    void setSize(int numChannels, int numSamples);

    // Allows you to push a buffer of audio samples to this circular buffer
    // Notice this does not override "pushBuffer" from the base class
    bool pushBufferAndPeriod(juce::AudioBuffer<float>& buffer, float periodInSamples);

    // fills buffer with audio data starting from mReadPos over the range of the buffer's length in samples
    // returns float estimate of this range
    float popBufferAndPitch(juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& pitchBuffer);

    void popAudioBuffer(juce::AudioBuffer<float>& buffer);
private: 
    friend class PitchBufferTest; 

    // Returns sample index in a given range of a buffer. Index is relative to 'start' argument, not buffer[0][0]
    // TODO: handle stereo 
    int _findMaxSampleIndex(juce::AudioBuffer<float>& buffer, int start, int length);

    CircularBuffer mAudioBuffer;
    CircularBuffer mPitchBuffer;
};