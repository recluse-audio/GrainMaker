/*
  ==============================================================================

    QuadLFO.h
    Created: 17 Oct 2018 10:07:29am
    Author:  Antares

  ==============================================================================
*/

#pragma once

#ifndef AKDSPLIB_MODULE
#include "BiquadFilter.h" // includes <cmath>
#endif

#include <atomic>
#include <cstdlib>

#define INTERPOLATE 1

class QuadLFO
{
public:
    enum QuadLfoParam {
        kWave=0,
        kFreq,
        kOnset,
        kVariation,
        kTransition,
		kAmplitude1,
		kAmplitude2
    };
    
    enum QuadLfoWaveform {
        kNone=0,
        kSine,
        kSquare,
        kRampUp,
        kRampDown,
        kTriangle,
        kNumWaveforms
    };
    
    enum QuadLfoOutput {
        kBipolar = 0,
        kUnipolar,
		kInverted
    };
    
    QuadLFO()
    {
        setSampleRate(sampleRate_);
    }
    
    ~QuadLFO()
    {
		// nothing to see here
    }
    
    void setSampleRate(float fs, float decimatedRateHz = kDecimatedRateHz)
    {
        decimationCount_ = (int)lround(fs / decimatedRateHz);
        
		sampleRate_ = decimatedRateHz;
        
        // BPF for 2 Hz, Q = 10, output sigma = .016
        noiseFilter_.design(sampleRate_, 2., 10., 0, BiquadFilter<float>::kBandpass);
        
        this->setRateHz(rateInHz_);
        this->reset();
    }
    
    void setControl(int paramId, float param)
    {
        switch(paramId) {
            case kWave:
                waveform_ = (int)param;
                this->setWaveform(waveform_);
                break;
            case kFreq:
				// 0.1 to 10 Hz, default = 5.5Hz
                rateInHz_ = param;
                this->setRateHz(rateInHz_);
                break;
			case kOnset:
				// 0 to 1500 msec, default = 500
				onsetDelay_ = param * 0.001f * sampleRate_;
				break;
			case kVariation:
				// 0 to 100%, default = 20
				variation_ = param * .2f;
				break;
            case kTransition:
				// 0 to 1500 msec, default = 500
                transition_ = param * 0.001f * sampleRate_;
                break;
			case kAmplitude1:
				amplitude_[0] = param;
				break;
			case kAmplitude2:
				amplitude_[1] = param;
                break;
            default:
                // eeek!
                break;
        }
    }
    
    int getDecimation(void) { return decimationCount_; }
    
    void reset(void)
    {
        lfo_[0] = 0.;
        lfo_[1] = 1.;
        attackCount_ = 0;
        wavetableIndex_ = 0.;
        decimationClock_ = decimationCount_ - 1;
    }
    
    bool run(float ( &lfo_out )[2], int scale = kBipolar)
    {
        int t;
        bool updated = false;
		
#ifdef INTERPOLATE
        double a0, ap1, am1, dt, y;
#endif
        if (++decimationClock_ == decimationCount_) {
            decimationClock_ = 0;
            if (enabled_)
            {
                float rn = ((float)rand()) * kRandMaxInv - .5f;  // zero mean uniform -.5 to +.5
                rn = noiseFilter_.run(rn);
    
                double rate_variation = 1.f + rn * variation_;
                if (rate_variation < .1f) rate_variation = .1f; // never allow <= 0
                
                wavetableIndex_ += static_cast<float>(rate_variation * wavetablePhInc_);
                if (wavetableIndex_ >= kWavetableSize) {
                    wavetableIndex_ -= kWavetableSize;
                }
                
                //	env is the lfo envelope

                float env = 1.;
                if (attackCount_ <= onsetDelay_) {  // == forces at least one hit
                    env = 0.;
                    wavetableIndex_ = 0.;  // so vibrato starts at zero after onset
                }
                else
                if (attackCount_ < onsetDelay_ + transition_) {
                    env = (attackCount_ - onsetDelay_) / transition_;
                }
                
				updated = true;
				attackCount_++;
				
#ifdef INTERPOLATE
                // do quadratic interpolation
                
                t = (int)lround(wavetableIndex_);
                dt = wavetableIndex_ - t;
                
                t = (t - 1) & kWavetableMask;
                am1 = wavetableArray_[t];
                
                t = (t + 1) & kWavetableMask;
                a0 = wavetableArray_[t];
                
                t = (t + 1) & kWavetableMask;
                ap1 = wavetableArray_[t];
                
                y = dt * (dt * (.5 * (am1 + ap1) - a0) + .5 * (ap1 - am1)) + a0;
                lfo_[0] = float(env * y);
#else
                t = (int)wavetableIndex_;
                lfo_[0] = float(env * wavetableArray_[t]);
#endif
                //	n starts at 1/4 cycle forward of sin(x) giving cos(x)
                float n = wavetableIndex_ + (kWavetableSize >> 2);
                if (n >= kWavetableSize) n -= kWavetableSize;
                
#ifdef INTERPOLATE
                // do quadratic interpolation
                
                t = (int)lround(n);
                dt = n - t;
                
                t = (t - 1) & kWavetableMask;
                am1 = wavetableArray_[t];
                
                t = (t + 1) & kWavetableMask;
                a0 = wavetableArray_[t];
                
                t = (t + 1) & kWavetableMask;
                ap1 = wavetableArray_[t];
                
                y = dt * (dt * (.5 * (am1 + ap1) - a0) + .5 * (ap1 - am1)) + a0;
                lfo_[1] = float(1.f + env * (y - 1.f));
#else
                t = (int)n;
                lfo_[1] = float(1. + env * (wavetableArray_[t] - 1.));
#endif
            } // if (enabled_)
        } // if (++decimationClock == decimationCount_)
		
		//  update the lfo outputs
		switch (scale) {
			default:
			case kBipolar:
				// return LFO outputs scaled from -1 to 1
				lfo_out[0] = lfo_[0]; // * amplitude_[0];
				lfo_out[1] = lfo_[1]; // * amplitude_[1];
				break;
			case kUnipolar:
				// return LFO outputs scaled from 0 to 1, normal amplitude control
				lfo_out[0] = 0.5f * (lfo_[0] + 1.f) * amplitude_[0];
				lfo_out[1] = 0.5f * (lfo_[1] + 1.f) * amplitude_[1];
				break;
			case kInverted:
				// return LFO outputs scaled from 0 to 1, inverted amplitude control
				lfo_out[0] = 1.f + lfo_[0] * amplitude_[0];
				lfo_out[1] = 1.f + lfo_[1] * amplitude_[1];
				break;
		}
		
		return updated;
    }
    
private:
    static constexpr float kTwoPi = 2 * 3.141592653589793f;
    static constexpr float kRandMaxInv = static_cast<float>(1./ ((float) RAND_MAX));
    static constexpr float kDecimatedRateHz = 200; // default
    static constexpr int kWavetableSize = 0x400; // 1024 (power of 2)
    static constexpr int kWavetableMask = 0x3FF; // mask for modulo wrap

    float sampleRate_ = kDecimatedRateHz;
	
    // user controls
	int waveform_ = kNone;
	
	float rateInHz_ = 1.;	// rate in Hz
    float variation_ = 0.;	// 0.1 to 0.2
	float onsetDelay_ = 0.;	// seconds >= 0
    float transition_ = 0.;	// seconds >= 0
	
	float amplitude_[2] = {0., 0.}; // 0 to 100%
	
    // vibrato state
	
	std::atomic<bool> enabled_ { false };
	
	float lfo_[2] = {0., 1.}; // quadrature lfo state
    
    // wavetable data
    float wavetableArray_[kWavetableSize];
	float wavetableIndex_ = 0.f; // current location in wavetable
	float wavetablePhInc_ = 0.f; // sample increment through wavetable

    // internal state
    int decimationCount_ = (int)lround(44100.f / kDecimatedRateHz);
	int decimationClock_ = decimationCount_ - 1;

    int attackCount_ = 0;
    
    BiquadFilter<float> noiseFilter_;
    
    // rate range 0.1Hz to 10Hz
    void setRateHz(float rate_hz)
    {
        float period = sampleRate_ / rate_hz;
        wavetablePhInc_ = kWavetableSize / period;
        //std::cout << "lfo rate = " << rate_control_ << "\n";
    }

    
    // create waveform table.. waveform range is {-1, +1}
    void setWaveform(int waveformType)
    {
        //std::cout << std::fixed << std::setprecision(3);
        
        if (waveformType != kNone)
        {
            enabled_.store(false);
            
            float fcn;	// range -1 to +1
            int dx = 0;
            
            for (int i = 0; i < kWavetableSize; ++i)
            {
                fcn = 0.;
                switch (waveformType)
                {
                    default:
                    case kSine:
                        fcn = sin( kTwoPi * i / kWavetableSize);
                        break;
                    case kSquare:
                        fcn = 1.;
                        if (i >= kWavetableSize / 2)
                            fcn = -1.;
                        break;
                    case kRampUp:
                        fcn = (2.f / kWavetableSize) * i - 1.f;
                        break;
                    case kRampDown:
                        fcn = (2.f / kWavetableSize) * (-i) + 1.f;
                        break;
                    case kTriangle:
                        fcn = (4.f / kWavetableSize) * dx - 1.f;
                        if (i <= kWavetableSize / 2)
                            dx++;
                        else
                            dx--;
                        break;
                }
                wavetableArray_[i] = fcn;
                //std::cout << i << "\t\t" << fcn << "\n";
            }
            this->reset();
        }
		
		enabled_.store(waveformType != kNone);
    }
};
