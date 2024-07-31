/*
  ==============================================================================

    UnvoicedDetector.h
    Created: 2 Jul 2019 5:21:12pm
    Author:  Andrew

  ==============================================================================
*/

#pragma once

#include <math.h>
#include "Zerox.h"
#include "Follower.h"
#include "Crossover.h"
#include "CrestFinder.h"
#include "BiquadFilter.h"
#include "MultiStageIIR.h"

template <typename dspfloat_t>
class UnvoicedDetector
{
public:
    enum ParamId {
        kMode=0,
        kAlpha,
        kHpfFc,
        kLpfFc,
        kAttMs,
        kRelMs,
		kSplit,
		kSlope,
		kFtype,
		kZcrTs,
		kZcrTw,
		kUseLc
    };
	
	enum DetectMode {
		kDetectOff=0,
		kDetectAuto,
		kDetectBridge
	};
	
	enum FilterSlope {
		k12DbPerOct = 1,
		k24DbPerOct = 2
	};
	
	enum Levels {
		kUnvDet,
		kLpfEnv,
		kHpfEnv,
		kZcrEnv,
		kNumLevels
	};
    
    UnvoicedDetector()
    {
        setSampleRate(44100.);
    }
    
    ~UnvoicedDetector()
    {
		// nothing to see here
    }
    
    void setSampleRate(float fs)
    {
        fs_ = fs;
		dcAlpha_ = exp(-2. * kPi * 10. / fs_); // ~0.998 @44.1KHz

        zerox.init(fs_, zcrWindowMs_, zcrSmoothMs_);
		xover.design(fs_, hpfFc_, filterSlope_);
		lowcut.design(fs_, lcfFc_, 0.7071f, 0, BiquadFilter<dspfloat_t>::kHighpass);
		
		crest.setSampleRate(fs_);

        hpf_.design(fs_, hpfFc_, 0.7071f, MultiStageIIR<dspfloat_t>::kTypeHighpass, filterSlope_);
        
        lpf_.design(fs_, lpfFc_, 0.7071f, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterSlope_);
		
		hpfFollow_.setSampleRate(fs_);
        hpfFollow_.setAttackMs(envAttMs_);
        hpfFollow_.setReleaseMs(envRelMs_);
        hpfEnv_ = 0.;

		lpfFollow_.setSampleRate(fs_);
        lpfFollow_.setAttackMs(envAttMs_);
        lpfFollow_.setReleaseMs(envRelMs_);
        lpfEnv_ = 0.;
 
		zcrFollow_.setSampleRate(fs_);
		zcrFollow_.setAttackMs(envAttMs_);
        zcrFollow_.setReleaseMs(envRelMs_);
        zcrEnv_ = 0.;       
    }
    
    void setParam(int pid, float val)
    {
		double coeff;
        switch (pid) {
            case kMode:
                uvDetectMode_ = (int)val;
                break;
            case kAlpha:
                uvdetAlpha_ = val * .01f; // 0 to 1.
				uvdetThresh_ = (val > 96) ? -1e6 : -val; // 0 to -100
                break;
            case kHpfFc:
                hpfFc_ = val;
				if (splitBand_) {
					xover.design(fs_, hpfFc_, filterSlope_);
				} else {
                	hpf_.design(fs_, hpfFc_, 0.7f, MultiStageIIR<dspfloat_t>::kTypeHighpass, filterSlope_);
				}
                zcrThresh = hpfFc_ / fs_;
                break;
            case kLpfFc:
                lpfFc_ = val;
                lpf_.design(fs_, lpfFc_, 0.7f, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterSlope_);
                break;
            case kAttMs:
                envAttMs_ = val;
				coeff = exp(-1. / (fs_ * envAttMs_ * .001));
                hpfFollow_.setAttack(coeff);
                lpfFollow_.setAttack(coeff);
				zcrFollow_.setAttack(coeff);
                break;
            case kRelMs:
                envRelMs_ = val;
				coeff = exp(-1. / (fs_ * envRelMs_ * .001));
                hpfFollow_.setRelease(coeff);
                lpfFollow_.setRelease(coeff);
				zcrFollow_.setRelease(coeff);
                break;
			case kSplit:
				splitBand_ = (bool)val;
				break;
			case kSlope:
				filterSlope_ = (int)val;
				if (splitBand_) {
					xover.design(fs_, hpfFc_, filterSlope_);
				} else {
					hpf_.design(fs_, hpfFc_, 0.7f, MultiStageIIR<dspfloat_t>::kTypeHighpass, filterSlope_);
					lpf_.design(fs_, lpfFc_, 0.7f, MultiStageIIR<dspfloat_t>::kTypeLowpass, filterSlope_);
				}
				break;
			case kFtype:
				followerType_ = (int)val;
				break;
			case kZcrTw:
				zcrWindowMs_ = val;
				zerox.setWindowTcMs(zcrWindowMs_);
				break;
			case kZcrTs:
				zcrSmoothMs_ = val;
				zerox.setSmoothTcMs(zcrSmoothMs_);
				break;
			case kUseLc:
				useLowcut_ = (bool)val;
				break;
        }
    }
    
    inline bool run(dspfloat_t xin, bool freeze = false)
    {
		// remove any low rumble, pops, plosives, etc...
		if (useLowcut_) {
#if 1
			// apply two pole highpass filter, fc = 100Hz
			xin = lowcut.run(xin);
#else
			// apply one pole + one zero DC blocker filter
			dcy_ = xin - dcx_ + dcAlpha_ * dcy_;
			dcx_ = xin;
			xin = dcy_;
#endif
		}
		
		if (useCrest_) {
			crestLevel_ = sqrtf(crest.run(xin));
		}
		
		// run voiced / unvoiced filters + envelope followers
		
		if (splitBand_) {
			xover.run(xin, xf);
		} else {
			xf[0] = hpf_.run(xin);
			xf[1] = lpf_.run(xin);
		}

		hpfEnv_ = fmax(hpfFollow_.run(xf[0], followerType_, freeze), 0.f); // 1e-4);
		lpfEnv_ = fmax(lpfFollow_.run(xf[1], followerType_, freeze), 0.f); // 1e-4);
		
		unvoiced_ = false;
		if (useZerox_)
		{
			zcrRaw_ = zerox.run(xin);
			zcrEnv_ = zcrFollow_.run(zcrRaw_, followerType_, freeze);
			
            if (uvDetectMode_ == kDetectAuto) {
                if ((hpfEnv_ > lpfEnv_) && (zcrEnv_ > zcrThresh)) {
                    unvoiced_ = true;
                }
            } else {
                if (uvDetectMode_ == kDetectOff) {
                    hpfEnv_ = 0;
                    lpfEnv_ = 0;
                    zcrEnv_ = 0;
                    unvoiced_ = false;
                } else {
                    // bridge
                    hpfEnv_ = 1;
                    lpfEnv_ = 1;
                    zcrEnv_ = 1;
                    unvoiced_ = true;
                }
            }
        }
        else {
#if 0
            // simplified scheme w/ sensitivity control
            if ((uvdetAlpha_ * hpfEnv_) > ((1. - uvdetAlpha_) * lpfEnv_)) {
                unvoiced_ = true;
				uvdetFlag_ = true;
            }
#else
			// log domain scheme w/ sensitivity control
			float hpfDb = linToDb(hpfEnv_);
			float lpfDb = linToDb(lpfEnv_);
			
			if ((1. - uvdetAlpha_) * hpfDb > uvdetAlpha_ * lpfDb) {
			//if (hpfDb > uvdetThresh_) { // && lpfDb  < uvdetThresh_) {
				unvoiced_ = true;
				uvdetFlag_ = true;
			}
#endif
        }
        return unvoiced_;
    }
	
	void enableTestDSP (bool enable) {
		useLowcut_ = enable;
		useZerox_ = enable;
		useCrest_ = enable;
	}
    
    inline float getZeroxLevel(void) {
        return zcrEnv_;
    }

    inline float getUvHpfLevel(void) {
        return hpfEnv_;
    }

    inline float getUvLpfLevel(void) {
        return lpfEnv_;
    }
    
    inline bool getUnvoicedFlag(void) {
		uvdetFlag_ = false;
        return unvoiced_;
    }
	
	inline float getZeroxRate(void) {
		return zcrRaw_;
	}

	inline float getCrestFactor(void) {
		return crestLevel_;
	}

    inline void getState( float ( &pState )[4]) {
        pState[0] = uvdetFlag_; // unvoiced_
        pState[1] = lpfEnv_;
        pState[2] = hpfEnv_;
        pState[3] = zcrEnv_;
		// reset detect meter
		if (!unvoiced_)
			uvdetFlag_ = false;
    }
	
	inline void getHpfLpf( double ( &pSamp)[2]) {
		pSamp[0] = xf[0]; // HPF
		pSamp[1] = xf[1]; // LPF
	}

private:
    float fs_ = 44100.;
    
	dspfloat_t xf[2];
	
    MultiStageIIR<dspfloat_t> hpf_;
    MultiStageIIR<dspfloat_t> lpf_;
    
    Follower<dspfloat_t> hpfFollow_;
    Follower<dspfloat_t> lpfFollow_;
    Follower<dspfloat_t> zcrFollow_;
	
	Zerox zerox;
	Crossover<dspfloat_t> xover;
	CrestFinder<dspfloat_t> crest;
	BiquadFilter<dspfloat_t> lowcut;

    bool unvoiced_ = false;
    bool useZerox_ = false;
	bool useCrest_ = false;
	bool useLowcut_ = false;
	bool splitBand_ = false;
	bool uvdetFlag_ = false;

	float hpfFc_ = 3500; // Hz
	float lpfFc_ = 1000; // Hz
	float lcfFc_ = 100; // Hz
    float hpfEnv_ = 0;
    float lpfEnv_ = 0;
    float zcrEnv_ = 0;
	float zcrRaw_ = 0;
    float envAttMs_ = .2f;
    float envRelMs_ = 20.;
    
	float crestLevel_ = 0.;
	float uvdetAlpha_ = 0.5;
	float uvdetThresh_ = -50.;

	// DC blocker
	double dcx_ = 0;
	double dcy_ = 0;
	double dcAlpha_;

	float zcrThresh = hpfFc_ / fs_;
	float zcrWindowMs_ = 2.;
	float zcrSmoothMs_ = 20.;
	
	int filterSlope_ = k24DbPerOct;
	int uvDetectMode_ = kDetectAuto;
	int followerType_ = Follower<double>::kSmoothBranching;
	
	static constexpr double kPi = 3.14159265358979323846;
	
	float linToDb(float x) {
		return (x == 0) ? -120 : 20.f * log10f(x);
	}
};

