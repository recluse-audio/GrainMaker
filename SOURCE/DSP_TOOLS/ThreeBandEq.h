/*
  ==============================================================================

    ThreeBandEq.h
    Created: 25 May 2021 5:08:23pm
    Author:  Antares

  ==============================================================================
*/

#pragma once
#include <math.h>
#include "BiquadFilter.h"

template <typename dspfloat_t>
class ThreeBandEq
{
public:
	ThreeBandEq()
	{
		init(fs);
	};
	
	~ThreeBandEq() { };
	
	void init(float fs_hz)
	{
		fs = fs_hz;
		
		leq.design(fs, lf, 1., lg, BiquadFilter<dspfloat_t>::kLowshelf);
		meq.design(fs, mf, 1., mg, BiquadFilter<dspfloat_t>::kPeaking);
		heq.design(fs, hf, 1., hg, BiquadFilter<dspfloat_t>::kHighshelf);
	};
	
	float run(float x)
	{
		if (bypass)
			return x;
		
		double y;
		
		y = leq.runInterp(x);
		y = meq.runInterp(y);
		y = heq.runInterp(y);
		
		return y * gain;
	};
	
	void setBypass(bool b)
	{
		bypass = b;
	};
	
	void setLfGain(float gain_db)
	{
		lg = gain_db;
		leq.design(fs, lf, 1., lg, BiquadFilter<dspfloat_t>::kLowshelf);
	};
	
	void setLfFreq(float freq_hz)
	{
		lf = freq_hz;
		leq.design(fs, lf, 1., lg, BiquadFilter<dspfloat_t>::kLowshelf);
	};

	void setMfGain(float gain_db)
	{
		mg = gain_db;
		meq.design(fs, mf, 1., mg, BiquadFilter<dspfloat_t>::kPeaking);
	};

	void setMfFreq(float freq_hz)
	{
		mf = freq_hz;
		meq.design(fs, mf, 1., mg, BiquadFilter<dspfloat_t>::kPeaking);
	};

	void setHfGain(float gain_db)
	{
		hg = gain_db;
		heq.design(fs, hf, 1., hg, BiquadFilter<dspfloat_t>::kHighshelf);
	};

	void setHfFreq(float freq_hz)
	{
		hf = freq_hz;
		heq.design(fs, hf, 1., hg, BiquadFilter<dspfloat_t>::kHighshelf);
	};

	void setGainTrim(float gain_db)
	{
		tg = gain_db;
		gain = dBToGain(tg);
	};
	
	float getMagnitudeResponse(float freq_hz)
	{
		float h = 0.;
		h += leq.getMagnitudeResponse(freq_hz);
		h += meq.getMagnitudeResponse(freq_hz);
		h += heq.getMagnitudeResponse(freq_hz);
		return h + tg;
	};
	
private:
	
	bool bypass = true;
	
	float fs = 44100.; // sample rate
	float gain = 1.0;
	
	// Freq controls
	
	float  lf = 250.;  // LF freq
	float  mf = 1500.; // MF freq
	float  hf = 5000.; // HF freq

	// Gain controls
	
	float  lg = 0.;  // LF gain dB
	float  mg = 0.;  // MF gain dB
	float  hg = 0.;  // HF gain dB
	float  tg = 0.;  // bulk gain dB

	// Filters
	
	BiquadFilter<dspfloat_t> leq;
	BiquadFilter<dspfloat_t> meq;
	BiquadFilter<dspfloat_t> heq;
	
	float dBToGain(float db)
	{
		return powf(10.f, db / 20.f);
	};
};
