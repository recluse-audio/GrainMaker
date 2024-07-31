/*
  ==============================================================================

    Ensemble.h
    Created: 17 Oct 2018 10:07:47am
    Author:  Antares

  ==============================================================================
*/

#pragma once
//#include "Platform.h"

#ifndef AKDSPLIB_MODULE
#include "QuadLFO.h"
#include "FracDelay.h"
#include "BiquadFilter.h"
#endif

template <typename dspfloat_t>
class Ensemble
{
public:
    enum EnsembleParam {
        kRate=0,
        kDepth,
        kDelay,
        kHpfFc,
        kHpfMix,
        kWetMix,
        kDryMix
    };
    
    Ensemble()
    {
        setSampleRate(fs_);
    }
    
    ~Ensemble()
    {
        delete fracDelayL_;
        delete fracDelayR_;
    }
    
    void setSampleRate(float fs)
    {
        fs_ = fs;
        
        delete fracDelayL_;
        delete fracDelayR_;

        fracDelayL_ = new FracDelay<dspfloat_t>(fs_, 2 * (kMaxDelayTimeMs + kMinDelayTimeMs));
        fracDelayR_ = new FracDelay<dspfloat_t>(fs_, 2 * (kMaxDelayTimeMs + kMinDelayTimeMs));
        
        quadLFO_.setSampleRate(fs_, 2000); // decimated rate = 2000Hz
        quadLFO_.setControl(QuadLFO::kFreq, rate_);
        quadLFO_.setControl(QuadLFO::kWave, QuadLFO::kTriangle);
        
        hpfL_.design(fs_, hpfFc_, 0.7f, 0, BiquadFilter<dspfloat_t>::kOnePoleHP);
        hpfR_.design(fs_, hpfFc_, 0.7f, 0, BiquadFilter<dspfloat_t>::kOnePoleHP);
    }
    
    void setControl(int paramId, float param)
    {
        switch(paramId) {
            case kRate:
                rate_ = param;
                quadLFO_.setControl(QuadLFO::kFreq, rate_);
                break;
            case kDepth:
                depth_ = param;
                break;
            case kDelay:
                delay_ = param;
                break;
            case kHpfFc:
                hpfFc_ = param;
                hpfL_.design(fs_, hpfFc_, 0.7f, 0, BiquadFilter<dspfloat_t>::kOnePoleHP);
                hpfR_.design(fs_, hpfFc_, 0.7f, 0, BiquadFilter<dspfloat_t>::kOnePoleHP);
                break;
            case kHpfMix:
                hpfMix_ = param;
                break;
            case kWetMix:
                wetMix_ = param;
                break;
            case kDryMix:
                dryMix_ = param;
                break;
            default:
                // eeek!
                break;
        }
    }
    
    //
    // run ensemble (sample processing)
    //
    inline void run( dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2])
    {
        float lfo[2];
        
		if (quadLFO_.run(lfo, QuadLFO::kBipolar)) {
            // update delay times only if new LFO value ready
            float d1 = depth_ * lfo[0] * (kMaxDelayTimeMs - kMinDelayTimeMs) + delay_;
            float d2 = depth_ * (1.f - lfo[0]) * (kMaxDelayTimeMs - kMinDelayTimeMs) + delay_;
            fracDelayL_->setDelayMs(d1);
            fracDelayR_->setDelayMs(d2);
        }
        
        dspfloat_t x[2] = {0, 0};
        
        x[0] = xi[0];
        if (stereoIn_) {
            x[1] = xi[1];
        } else {
            x[1] = x[0];
        }
        
        x[0] = fracDelayL_->run((float)x[0]);
        x[1] = fracDelayR_->run((float)x[1]);
        
        dspfloat_t xL = hpfL_.run(-x[0]) * 1.414f; // +3dB makeup gain
        dspfloat_t xR = hpfR_.run(-x[1]) * 1.414f; // +3dB makeup gain

        x[0] = x[0] * wetMix_ + xi[0] * dryMix_ + xR * hpfMix_;
        x[1] = x[1] * wetMix_ + xi[1] * dryMix_ + xL * hpfMix_;
        
        if (stereoOut_) {
            xo[0] = x[0];
            xo[1] = x[1];
        } else {
            xo[0] = 0.5f * (x[0] + x[1]);
        }
    }
    
    // clear delay lines
    void reset(void)
    {
        fracDelayL_->reset();
        fracDelayR_->reset();
    }
    
    //
    // run ensemble (block processing)
    //
    inline void run(dspfloat_t** xi, dspfloat_t** xo, int frames)
    {
        double x[2];
        double y[2];
        
        for (int n = 0; n < frames; n++)
        {
            x[0]  = xi[0][n];
            if (stereoIn_) {
                x[1]  = xi[1][n];
            } else {
                x[1] = x[0];
            }
            
            this->run(x, y);
            
            xo[0][n] = y[0];
            if (stereoOut_) {
                xo[1][n] = y[1];
            }
        }
    }
    
    void loadPreset(int preset)
    {
        switch (preset) {
            default:
            case 0:
                // bypass
                break;
			case 1:
				setControl(kRate, 0.16f);
				delay_ = 15;
				depth_ = 0.2f;
				break;
            case 2:
                setControl(kRate, 0.25f);
                delay_ = 12;
                depth_ = 0.3f;
                break;
            case 3:
                setControl(kRate, 0.33f);
                delay_ = 10;
                depth_ = 0.4f;
                break;
            case 4:
                setControl(kRate, 0.50f);
                delay_ = 8;
                depth_ = 0.5f;
                break;
        }
		
		fracDelayL_->reset();
		fracDelayR_->reset();
    }
    
    void setStereoIn(bool stereo) { stereoIn_ = stereo; }
    void setStereoOut(bool stereo) { stereoOut_ = stereo; }
    
private:
    static const int kMinDelayTimeMs = 5;
    static const int kMaxDelayTimeMs = 30;
    
    FracDelay<dspfloat_t>* fracDelayL_ = nullptr;
    FracDelay<dspfloat_t>* fracDelayR_ = nullptr;
    
    BiquadFilter<dspfloat_t> hpfL_;
    BiquadFilter<dspfloat_t> hpfR_;
    
    QuadLFO quadLFO_;
    
    bool stereoIn_ = true;
    bool stereoOut_ = true;

    float fs_ = 44100.;
    float rate_ = .25;
    float depth_ = .5;
    float delay_ = 25.;
    float hpfFc_ = 1000;
    float hpfMix_ = .5;
    float wetMix_ = .5;
    float dryMix_ = .5;
};
