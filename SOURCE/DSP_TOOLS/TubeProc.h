//
//	Copyright (c) 2018 Antares Audio Technologies. All rights reserved.

#pragma once
#ifndef AKDSPLIB_MODULE
#include "TubeTone.h"
#endif
#include <cmath>
#include <atomic>

//#define USE_DB_LOOKUP_TABLES 1

template <typename dspfloat_t>
class TubeProc
{
public:
    TubeProc();
	~TubeProc() { setSampleRate(sampleRate); }
    
    void setControl(int paramId, float paramVal);
    
    void setSampleRate(float sampleRate);
    
    dspfloat_t run(dspfloat_t xin);
    
    void getLevels(float& inputLevel, float& outputLevel);
    void getCompLevels(float& compLevel, float& compGain);
    
    float getToneFilterResponse(float freq_hz)
    {
        return (float)tubeTone.getMagnitudeResponse(freq_hz);
    }
    
    // configuration params
    enum Param
    {
        kBypass = 0,
        kSetEqPost,
        kInputGain,
        kDriveGain,
        kOutputGain,
        kCompEnable,
        kCompThresh,
        kCompAttack,
        kCompRelease,
        kEnableTone,
        kSetToneLow,
        kSetToneMid,
        kSetToneHigh,
        kSetSeverity
    };
private:
    float sampleRate = 44100.;
    
	// control parameters

    bool bypass = false;        // UI param, bypass
    bool eqpost  = false;       // UI param, TubeTone post-distortion
	bool severity = false;      // UI param, velvet /  crunch setting
    bool tubetone = false;      // UI param, TubeTone EQ
	bool compressor = false;    // UI param, OmniTube

    float input_gain_coef = 1.;     // UI param, input gain control
    float output_gain_coef = 1.;    // UI param, output gain control
    float drive_gain_control = 0.;  // UI param, drive gain control
    
    // filters
    
    dspfloat_t smooth_alpha;
	
    dspfloat_t dcblock_alpha;
    dspfloat_t dcblock_state_x = 0.f;
    dspfloat_t dcblock_state_y = 0.f;

    // compressor
    
    dspfloat_t comp_gain = 1.f;
    dspfloat_t comp_attack;
	dspfloat_t comp_release;
    dspfloat_t comp_envelope = 0.f;
    dspfloat_t comp_threshold = 1.f;
    dspfloat_t comp_threshold_f = 1.f;
	dspfloat_t comp_threshold_inv = 1.f;
    dspfloat_t comp_threshold_inv_f = 1.f;

	// distortion

	dspfloat_t drive_gain_coef = 1.f;
    dspfloat_t drive_gain_coef_f = 1.f;
	dspfloat_t drive_gain_recovery = 1.f;
    dspfloat_t drive_gain_recovery_f = 1.f;

    // metering
    
    std::atomic<bool> clear_levels = true;
    
	dspfloat_t input_level = 0.;
	dspfloat_t output_level = 0.;

    // Tube EQ
    
    TubeTone<dspfloat_t> tubeTone;

	// dBToGain: convert decibels to gain.
	inline dspfloat_t dBToGain(dspfloat_t db)
	{
		return pow(10.0f, db / 20.0f);
	}
    
#ifdef USE_DB_LOOKUP_TABLES
    // lookup tables
    dspfloat_t dBToGainDrive[25]; // 0 to +24dB, 0.5dB steps
    dspfloat_t dBToGainRecov[25]; // 0 to -3.6dB, 0.15dB steps
    dspfloat_t dBToGainCompr[37]; // 0 to -36dB, 1.0dB steps
#endif

	void updateGains();

    // processing constants
    static constexpr dspfloat_t kAttackHalfTime = .00001f; // sec
    static constexpr dspfloat_t kReleaseHalfTime = .100f;  // sec
    static constexpr dspfloat_t kDcBlockerFreqHz = 12; // Hz
    static constexpr dspfloat_t kMinCompThreshDb = -36;
    static constexpr dspfloat_t k0dBFS = 0.9999f;
    static constexpr dspfloat_t kPi = 3.14159265358979323846f;
};

