//
//  Vocoder.hpp
//  ATLiveDemo
//
//  Created by Antares on 6/10/18.
//  Copyright © 2018 Antares Audio Technologies. All rights reserved.
//

#pragma once

#include "Follower.h"
#include "TubeTone.h"
#include "TubeProc.h"
#include "Ensemble.h"
#include "LevelMeter.h"
#include "BiquadFilter.h"
#include "MultiStageIIR.h"
#include "UnvoicedDetector.h"

#include <cmath>

//#define VOCODER_DEBUG 1

#ifdef VOCODER_DEBUG
#include <iostream>
#include <iomanip>
#include <stdio.h>
#endif

#include "LogMath.h"
#include "TimeMath.h"

// vocoder UI parameters
enum VocoderParam {
    // filterbank design
    kVocFbEmulation=0,
    kVocFbNumBands,
    kVocFbNumPoles,
    kVocFbFilterQ,
    kVocFbFreqMin,
    kVocFbFreqMax,
    kVocFbSetLPF,
    kVocFbSetHPF,
    kVocFbStretch,
    kVocFbSlide,
    // envelope followers
    kVocEnvBoost, // hidden
    kVocEnvFlavor,
    kVocEnvFreeze,
    kVocEnvAttack,
    kVocEnvRelease,
    kVocEnvRelRatio,
    kVocEnvBandGain,
    kVocEnvBandShift,
	kVocEnvBandGlide,
    // compressor
    kVocCompThresh,
    kVocCompAttack, // hidden
    kVocCompRelease, // hidden
    // pre-filtering
    kVocLowCutEnable,
    kVocTubeEqTreble,
    // global
    kVocSynthComp,
	kVocSynthGain,
	kVocDriveGain,
	kVocChorusType,
    kVocHiConHpfFc,
    kVocHiConLevel, // hidden
    kVocMakeupGain,
    kVocAmplitudeMod,
    kVocStereoSpread,
	kVocUvMixBalance,
	kVocUvSensitivity,
    kVocVoiceToOutMix,
	kVocVoiceToChorus,
    // param count
    kVocNumParams
};

// vintage vocoder emulations
// (DO NOT change the order!)
enum VocoderModel {
	kVocAntaresVOC1 = 0,	// 24 band (default)
	kVocBarkhausen,	    	// 24 band, not importable
	kVocRolandVP330,	 	// 10 band
	kVocRolandSVC350, 		// 10 band
	kVocEMS2000,        	// 16 band, not importable
	kVocEMS5000,			// 22 band
	kVocSyntovox221,		// 20 band
	kVocSennhVSM201,    	// 20 band, not importable
	kVocMoog907,			// 10 band
	kVocMoog914,			// 14 band
	kVocBode7702,			// 16 band
	kVocKorgVC10,			// 20 band
	kVocElecHarm0300, 		// 14 band
	kVocDoepferA128,		// 15 band
	kVocDoepferA129,    	// 15 band, not importable
	kVocHoeroldVoIS,		// 18 band
	kVocGRPV22,				// 22 band
	kVocMAMVF11,        	// 11 band, not importable
	kVocMFOS12,				// 12 band
	kVocDudley,				// 10 band
	kVocNumModels       	// 18
};

enum VocoderFeedback {
    kVoiceLevel=0,
    kSynthLevel,
	kVocodeLevel,
    kOutputLevel,
    kVoiceEnvelope,
    kSynthEnvelope,
    kVoiceCompGain,
	kSynthCompGain,
    kNumLevelParams
};

enum VocoderFilterPoles {
	kVocFilter2Pole=0,
	kVocFilter4Pole,
	kVocFilter6Pole,
	kVocFilter8Pole
};

enum VocoderGuiUpdateType {
	kNoGuiUpdate = 0,
	kUpdateFilterGraph,
	kUpdateFilterGraphAndMultiSlider
};

template <typename dspfloat_t>
class VocodeCore
{
public:
	static constexpr int kMaxBands = 24;
    static constexpr int kGraphPoints = 512;
    
	inline int numBands(void) { return bands_; }
	
    // custom vocoder filter bank design data
    struct ImportData {
        int numBands;
        float filterQ;
        float fMin;
        float fMax;
        bool lpfOn;
        bool hpfOn;
        int numPoles;
    };

    // vintage vocoder filter bank design data
    struct VintageModel {
        float bandFreq[kMaxBands];
        float filterQ;
        int numBands;
        int numPoles;
        bool lpfOn;
        bool hpfOn;
        bool canModify;
        ImportData importData;
    };

	// constructor
    VocodeCore();

    // destructor
    ~VocodeCore() { }

    void setSampleRate(float fs);
    
    void designCustom(void);
	
    void designVintage(int model);

    void run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], dspfloat_t noise, dspfloat_t voice, bool synthIsExternal, int audition = 0);

    void setControl(int paramId, float param, float param2, int& guiUpdateFlag);
    
	void setMorphGains( float ( &pBandGains )[kMaxBands] )
	{
		for (int band = 0; band < kMaxBands; band++) {
			bm_[band] = pBandGains[band];
		}
	}
	
	void setMorphEnable(bool morphEnable)
	{
		morphEnable_ = morphEnable;
	}
	
    inline float getSampleRate(void) const { return fs_; }

    int getGraphData(float h[][kGraphPoints], float f[], float fc[], float fx[], int bins);
	
    void getLevels(float ( &pLevel )[kNumLevelParams]);
    
    inline float getSynthFilterBandFreq(int band) const
    {
        return fx_[band];
    }

    inline float getVoiceFilterBandFreq(int band) const
    {
        return fc_[band];
	}
	
	void getVoiceFilterEnvelope( float ( &pEnvelope )[kMaxBands], float ( &pBandGains )[kMaxBands] )
	{
		for (int band = 0; band < kMaxBands; band++) {
			pEnvelope[band] = voiceEnvDet_[band].getEnvelope() * bg_[band];
			pBandGains[band] = morphEnable_ ? bm_[band] : bg_[band];
		}
	}
	
	void getUnvoicedState( float ( &pState)[UnvoicedDetector<dspfloat_t>::kNumLevels])
	{
		float state[UnvoicedDetector<dspfloat_t>::kNumLevels];
		uvDetector_.getState(state);
		for (int i=0; i < UnvoicedDetector<dspfloat_t>::kNumLevels; i++) {
			pState[i] = state[i];
		}
	}

    int getVocoderModelParams(int& poles, bool& lpfOn, bool& hpfOn, float& fMin, float& fMax, float& fBw);
    void getVocoderModelDefaultParams(int& poles, bool& lpfOn, bool& hpfOn, float& fMin, float& fMax, float& fBw, int& bands);
    void getVocoderModelDefaultParams(VintageModel& model);
    bool canModifyFilters(void) const;
    bool canModifyFilters(int model) const;
    
private:
    // constants and defaults
	
	enum {
		kActive = 0,
		kTarget
	};
	
	static constexpr int kHiConHpfStages = 2;
	static constexpr int kSlowClockPeriod = 5;
	static constexpr float k0dBFS = 0.99999999f;
	static constexpr float kLowcutFilterFc = 80; // Hz
	static constexpr float kHighConFilterFc = 30; // Hz
	static constexpr float kSynthEnvGateThr = 0.001f; // -60dB
	static constexpr float kBoostGainDbInit = 15; // dB
	static constexpr float kMakeupGainDbInit = 0;
	static constexpr float kCompThreshDbInit = -10;
    static constexpr float kTubeEqLowDefault = 0.007f;
    static constexpr float kTubeEqMidDefault = 0.780f;
    static constexpr float kTubeEqTopDefault = 0.200f;

    // global state
	
    bool lowcut_ = true;
    bool freeze_ = false;
	bool rmsdet_ = false;
	bool chorusOn_ = false;
	bool unvoiced_ = false;
	bool vocToChorus_ = false;
	bool morphEnable_ = false;

	int slowClock_ = 0;
	int slowCount_ = 1;
	
	// filterbank state
	
    int model_ = kVocAntaresVOC1;
    int bands_ = kMaxBands;
	int stages_ = MultiStageIIR<dspfloat_t>::kSlope48dB;

    bool useHpf_ = true;
    bool useLpf_ = true;

    float Qv_ = 5.;
    float Qs_ = 5.;
    float fs_ = 44100.;
    float fmin_ = 100.;
    float fmax_ = 10000.;
	
	int shift_ = 0;  // follower output routing
    float slide_ = 0.; // -1.0 -> 0. -> +1.0
    float stretch_ = 1.; // 0.5 -> 1.0 -> 2.0

    float boost_ = (float)LogMath::dbToLin(kBoostGainDbInit);
	
    // envelope follower state

    float inpAttTimeMs_ = .5; // was 0.5, 0.00001 sec in TubeProc
	float inpRelTimeMs_ = 100.;
	
    float extAttTimeMs_ = 2.;
    float extRelTimeMs_ = 100.;
	
    float envAttTimeMs_ = .5;
    float envRelTimeMs_ = 20.;
    float envRelTimeDt_ = 1.; // smear

    float inputEnv_ = 0.;
	float voiceEnv_ = 0.;
    float synthEnv_ = 0.;
	
	// level metering state
	
	LevelMeter<dspfloat_t> levelMeter[4];
	
    // voice compressor + emphasis

    float synthCompThresh_ = (float)LogMath::dbToLin(kCompThreshDbInit);
	float synthCompGain_ = 1.; // 0dB
	float synthExtraGain_ = 1.; // 0dB
	
    float voiceCompThresh_ = (float)LogMath::dbToLin(kCompThreshDbInit);
    float voiceCompGain_ = 1.; // 0dB
	
	float warmth_ = 0.;
	float emphasis_ = kTubeEqTopDefault; // emphasis
	
    // high consonant filter

    float hiconHpfFc_ = kHighConFilterFc;
    float hiconLevel_ = 0.;

	// output control
	
	dspfloat_t spreadAdj_[2] = {0.5, 0.5}; // no spread
	dspfloat_t dryVocMix_[2] = {0., 0.};
	dspfloat_t makeupGain_[2] = {1., 1.}; // +0 dB
	dspfloat_t unvoicedMix_[2] = {0.5, 0.5}; // default
	
	float morphAlpha_ = 0;
	float smoothAlpha_ = 0; // smoothing coefficient
	
	// voice level sensitivity
	static constexpr float kVlsGainRefDb = -15; // dB
    float vlsGain_ = (float)LogMath::dbToLin(kVlsGainRefDb);
	float vlsCoeff_[2] = {vlsGain_, vlsGain_};
	float vlsAmount_[2] = {0., 0.};  // default = 0 %

    // vocoder vectors

    dspfloat_t vf_[kMaxBands]; // analysis filterbank outputs
    dspfloat_t sf_[kMaxBands]; // synthesis filterbank outputs
    
    float fc_[kMaxBands]; // analysis filter frequencies
    float fx_[kMaxBands]; // synthesis filter frequencies
    
	float bg_[kMaxBands]; // follower band gains
	float bm_[kMaxBands]; // follower band gains (morphed)
	float bf_[kMaxBands]; // follower band gains (smoothed)
    
	int   io_[kMaxBands]; // band re-mapping indices

    // DSP objects
	
	MultiStageIIR<dspfloat_t> voiceFilter_[kMaxBands];
	MultiStageIIR<dspfloat_t> synthFilter_[kMaxBands];
    
	Follower<dspfloat_t> voiceEnvDet_[kMaxBands];
    Follower<dspfloat_t> inputFollow_;
    Follower<dspfloat_t> voiceFollow_;
    Follower<dspfloat_t> synthFollow_;
    
    BiquadFilter<dspfloat_t> locutHpf_;
    MultiStageIIR<dspfloat_t> hiconHpf_;
	UnvoicedDetector<dspfloat_t> uvDetector_;
    
	TubeProc<dspfloat_t> tubeProc_[2];
	TubeTone<dspfloat_t> tubeToneEq_;
	Ensemble<dspfloat_t> chorusFx_;
	
    // private methods

    void setLPF(bool useLpf, float Qv, float Qs, bool updatedFromGui = false);

    void setHPF(bool useHpf, float Qv, float Qs, bool updatedFromGui = false);

    void setEnvReleaseTimes(void);
    
    void setBandShift(int shift);
	
    // static arrays
    
    static const VintageModel vintage_[kVocNumModels];
	
	// fixed (non-editable) vintage models
    static const float vsm201Q_[20];
    static const float mamVF11Q_[11];
    static const float barkScaleQ_[24];
};
