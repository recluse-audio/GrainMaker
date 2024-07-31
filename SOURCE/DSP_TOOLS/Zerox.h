/*
  ==============================================================================

    Zerox.h - Zero crossing detector
    Created: 24 Jul 2020 10:17:43am
    Author:  A Kimpel

  ==============================================================================
*/

#pragma once

#include <cmath>
#include <atomic>
#include "../math/TimeMath.h"

template <typename dspfloat_t>
class Zerox
{
public:
    Zerox() { init(44100., 2., 20); }
    ~Zerox() { }
    
    void init(float fs, float twindow_ms, float tsmooth_ms)
    {
		fs_ = fs;
		nwin_ = TimeMath::msecToSamples(fs_, twindow_ms);
        dcAlpha_ = TimeMath::onePoleCoeff<dspfloat_t>(10.f, fs);
        smoothTc_ = TimeMath::onePoleCoeff<dspfloat_t>(tsmooth_ms, fs, TimeMath::kDecayZolger);
		reset_.store(true);
    }
    
    void reset(void) { reset_.store(true); }
    
    float run(float xin)
    {
        if (reset_.exchange(false))
        {
            dcx_ = 0;
            dcy_ = 0;
            zcx_ = 0;
            zcr_ = 0;
            ncnt_ = 0;
            zcnt_ = 0;
        }
        
        // apply one pole + one zero DC blocker filter to remove any offset
        dcy_ = xin - dcx_ + dcAlpha_ * dcy_;
        dcx_ = xin;
        
        // test for zero crossing
        if ((dcy_ * zcx_) < 0)
            zcnt_++;
        
        zcx_ = dcy_;
        
        // compute zero crossing rate (ZCR)
        if (++ncnt_ == nwin_)
        {
            zcr_ = zcnt_ / nwin_;
            zcnt_ = 0;
            ncnt_ = 0;
        }
		
		// do smoothing
		zcrState_ = smoothTc_ * zcrState_ + (1.f - smoothTc_) * zcr_;
        return static_cast<dspfloat_t>(zcrState_);
    }
	
	float getzcr(void) { return static_cast<float>(zcrState_); }

private:
	float fs_ = 0;
    std::atomic<bool> reset_ { true };
	
    long nwin_ = 0;
    long ncnt_ = 0;
    long zcnt_ = 0;
	
    dspfloat_t zcr_ = 0.;
    dspfloat_t zcx_ = 0.;

	// DC blocker
    dspfloat_t dcx_ = 0;
    dspfloat_t dcy_ = 0;
    dspfloat_t dcAlpha_;

	// smoother
    dspfloat_t zcrState_ = 0; // no smoothing
    dspfloat_t smoothTc_ = 0; // no smoothing
};


