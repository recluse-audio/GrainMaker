/*
  ==============================================================================

    Delay.h
    Created: 2 Feb 2020 8:54:24pm
    Author:  Antares

  ==============================================================================
*/

#pragma once
#include <cmath>
#include <atomic>
#include <vector>
#include <cassert>

template <typename dspfloat_t>
class FixedDelay
{
public:
	enum Type {
		kMono=0,
		kStereo
	};
	
	FixedDelay(bool stereo = true)
	{
		stereo_ = stereo;
	}
	
	~FixedDelay()
	{
        delayBufL.clear();
        if (stereo_)
            delayBufR.clear();
	}
	
    void setMaxDelaySamples(int samples)
    {
        maxDelaySamples = samples;
        
        delayBufL.resize((size_t)samples, 0);
        if (stereo_)
            delayBufR.resize((size_t)samples, 0);
        
        reset.store(true);
    }
    
    void setMaxDelayTimeMs(float fs, float delay_ms)
    {
        fs_ = fs;
        setMaxDelaySamples(static_cast<int>(fs_ * delay_ms * .001f));
    }

    void setDelaySamples(int samples)
    {
        assert (samples >= 0 && samples < maxDelaySamples);
        delaySamples = samples;
        update.store(true);
    }
    
	void setDelayTimeMs(float delay_ms)
	{
		setDelaySamples(static_cast<int>(fs_ * delay_ms * .001f));
	}
	
	// mono delay
	inline dspfloat_t run(dspfloat_t x)
	{
        if (reset.exchange(false))
        {
            // clear delay buffers
            for (long i=0; i < maxDelaySamples; i++)
                delayBufL[(size_t)i] = 0;
        }
        
        if (update.exchange(false))
        {
            // TODO smooth fade
        }
        
        // run the delay line (mono)
        
		delayBufL[(size_t)delayIndex] = x;
		
		long outIndex = delayIndex - delaySamples;
		if (outIndex < 0) outIndex += maxDelaySamples;
		if (++delayIndex == maxDelaySamples) delayIndex = 0;
		
		return delayBufL[(size_t)outIndex];
	}

	// stereo delay
	inline void run(dspfloat_t ( &x )[2])
	{
        if (reset.exchange(false))
        {
            // clear delay buffers
            for (long i=0; i < maxDelaySamples; i++) {
                delayBufL[(size_t)i] = 0;
                if (stereo_)
                    delayBufR[(size_t)i] = 0;
            }
        }
        
        if (update.exchange(false))
        {
            // TODO smooth fade
        }
        
        // run the delay line (mono | stereo)
        
		delayBufL[(size_t)delayIndex] = x[0];
        if (stereo_)
            delayBufR[(size_t)delayIndex] = x[1];

		long outIndex = delayIndex - delaySamples;
		if (outIndex < 0) outIndex += maxDelaySamples;
		if (++delayIndex == maxDelaySamples) delayIndex = 0;

		x[0] = delayBufL[(size_t)outIndex];
        if (stereo_)
            x[1] = delayBufR[(size_t)outIndex];
	}
	
private:
	float fs_ = 44100;
	bool stereo_ = true;
    
	long delayIndex = 0;
	std::atomic<long> delaySamples = 0;
	std::atomic<long> maxDelaySamples = 0;
        
    std::atomic<bool> reset { false };
    std::atomic<bool> update { false };

    std::vector<dspfloat_t> delayBufL;
    std::vector<dspfloat_t> delayBufR;
};
