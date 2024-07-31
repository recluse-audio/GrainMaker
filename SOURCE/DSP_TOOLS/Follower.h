//
//  Follower.h
//
//  Created by Antares on 6/11/18.
//  Copyright © 2018 Antares Audio Technologies. All rights reserved.
//

#pragma once

#include <cmath>

//#define DO_SMOOTHING 1

template <typename dspfloat_t>
class Follower
{
public:
	enum Type {
		kSmoothBranching = 0,
		kSmoothDecoupled = 1,
		kSmoothAndyStyle = 2,
		kRootMeanSquared = 3
	};
	
	enum DecayStyle {
		kDecayAnalog = 0,
		kDecayZolger = 1,
		kDecayDrAndy = 2
	};
	
    Follower(float sampleRate = 44100, int decimation = 10, int decayStyle = kDecayAnalog)
    {
        sampleRate_ = sampleRate;
		dsInterval_ = decimation;
		decayStyle_ = decayStyle;
		
		// calculate downsampling ratio
		int ratio = 0.;
        if (sampleRate_ < 50000.) {
            ratio = 1.;
        }
        else
        if (sampleRate_ < 100000.) {
            ratio = 2.;
        }
        else
        if (sampleRate_ < 200000.) {
            ratio = 4.;
        }
		dsInterval_ *= ratio;
        
        setSampleRate(sampleRate_);
     }
    
    ~Follower() { /* nothing to see here */ }
    
    void setSampleRate(float fs)
    {
        sampleRate_ = fs;
        setAttackMs(attMsec_);
        setReleaseMs(relMsec_);
        // smoother
        envAlpha_ = exp(-2.f * kPi * 50.f / (sampleRate_ / dsInterval_)); // 50 Hz = 20 msec time constant
        dsCounter_ = 1; // prime for update on next sample
    }
	
	// process function
    inline dspfloat_t run(dspfloat_t x_in, int type = kSmoothBranching, bool hold = false)
    {
        if (hold) {
            return envelope_;
        }
            
        dspfloat_t x = 0;
        if (type == kSmoothDecoupled) {
            // smooth decoupled peak detector (Reiss)
            x = fabs(x_in);
            envState_ = fmax(x, relDecay_[1] * envState_ + (1.f - relDecay_[1]) * x);
            envelope_ = attDecay_[1] * envelope_ + (1.f - attDecay_[1]) * envState_;
        }
        else
		if (type == kRootMeanSquared) {
			// "RMS" detector (Zolger)
			x = fmax((x_in * x_in), 1e-06f);
            envState_ = relDecay_[1] * envState_ + (1.f - relDecay_[1]) * x;
            envelope_ = sqrt(envState_);
		} else {
            // smooth branching peak detector (Reiss)
            if (type == kSmoothBranching) {
				x = fabs(x_in);
            } else {
				// type == kSmoothAndyStyle
				if (x < fabs(x_in))
					x = fabs(x_in);
            }
            if (x > envelope_) {
                envelope_ = attDecay_[1] * envelope_ + (1.f - attDecay_[1]) * x;
            }
            else {
                envelope_ = relDecay_[1] * envelope_ + (1.f - relDecay_[1]) * x;
            }
        }
        
#ifdef DO_SMOOTHING
		// run parameter smoothing at decimated rate
		if (--dsCounter_ == 0) {
			dsCounter_ = dsInterval_;
			relDecay_[1] = envAlpha_ * relDecay_[1] + (1. - envAlpha_) * relDecay_[0];
			attDecay_[1] = envAlpha_ * attDecay_[1] + (1. - envAlpha_) * attDecay_[0];
		}
#endif
		return static_cast<dspfloat_t>(envelope_);
    }
    
    inline float getEnvelope(void) const { return (float)envelope_; }
	
	void setDecayStyle(int decayStyle) {
		decayStyle_ = decayStyle;
		computeDecay(attMsec_);
		computeDecay(relMsec_);
	}
	
	void setAttack(dspfloat_t val) {
        attDecay_[1] = val;
	}
	
	void setRelease(dspfloat_t val) {
		relDecay_[1] = val;
	}
	
    void setAttackMs(float val) {
		attMsec_ = val;
        attDecay_[1] = computeDecay(attMsec_);
	}
	
    void setReleaseMs(float val) {
		relMsec_ = val;
		relDecay_[1] = computeDecay(relMsec_);
	}
	
private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kLn0p5 = -0.69314718f; // ln(0.5)
	
    float sampleRate_;
	
	float attMsec_ = 1.;
	float relMsec_ = 20.;
	
    dspfloat_t envelope_ = 0;
    dspfloat_t envState_ = 0;
	dspfloat_t envAlpha_ = 0;

	dspfloat_t attDecay_[2] = {0, 0};
	dspfloat_t relDecay_[2] = {0, 0};
	
	int dsCounter_;
    int dsInterval_;
	
	int decayStyle_ = kDecayAnalog;

    // calculate the decay coefficient from time in milliseconds
	//
    dspfloat_t computeDecay(float time_msec) const
    {
        if (time_msec == 0)
            return 0;
        
        // large val_msec returns value close to 1.
        dspfloat_t decaySamples	= time_msec * .001f * sampleRate_;
		dspfloat_t tc;
		
		switch (decayStyle_) {
			default:
			case kDecayAnalog:
				// Reiss (63% of FS)
				tc = -1.0f;
				break;
			case kDecayZolger:
				// Zolger (90% of FS)
				tc = -2.2f;
				break;
			case kDecayDrAndy:
				// Andy H (50% of FS)
				tc = kLn0p5; // tc = ln(0.5) = -0.69
				break;
		}
		return exp(tc / decaySamples);
    }
};

