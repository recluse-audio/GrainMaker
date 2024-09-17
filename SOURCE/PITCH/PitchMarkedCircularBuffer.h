#include "UTIL/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/CircularBuffer.h"


/**
 * @brief A purpose built class that is an owner of a CircularBuffer.  
 * It interfaces with rather than inherits the CircularBuffer for simplicity as of right now
 * 
 * In addition to the inheritance from the CircularBuffer, it stores PitchMarks, a simple struct that 
 * records what pitch was pushed into the buffer and when it occurred.
 * 
 */
 

class PitchMarkedCircularBuffer : public CircularBuffer
{
public:
    PitchMarkedCircularBuffer();
    ~PitchMarkedCircularBuffer();

    // Allows you to push a buffer of audio samples to this circular buffer, and an associated pitch marking
    void pushBufferAndPitch(juce::AudioBuffer<float>& buffer);

private: 

};