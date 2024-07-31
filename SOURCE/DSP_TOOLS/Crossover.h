/*
  ==============================================================================

    Crossover.h
    Created: 27 Jun 2020 3:55:06pm
    Author:  Antares

  ==============================================================================
*/

#pragma once
#include <math.h>
#include "BiquadFilter.h"


//#define PIRKLE 1

// Linkwitz-Riley crossover filter design

template <typename dspfloat_t>
class Crossover
{
public:
	enum Type {
		kBypass=0,
		k2pole=1,
		k4pole=2
	};
	
	Crossover() { }
	~Crossover() { }
	
	void design(float fs, float fc, int type = k4pole)
	{
#ifndef PIRKLE
        // BiquadFilter design - 2nd or 4th order
        
        const float kQ2pole = 0.5f;
		const float kQ4pole = 0.7071067812f;
		
		if (type == k4pole) {
			hpf[0].design(fs, fc, kQ4pole, 0, BiquadFilter<dspfloat_t>::kHighpass);
			hpf[1].design(fs, fc, kQ4pole, 0, BiquadFilter<dspfloat_t>::kHighpass);
			lpf[0].design(fs, fc, kQ4pole, 0, BiquadFilter<dspfloat_t>::kLowpass);
			lpf[1].design(fs, fc, kQ4pole, 0, BiquadFilter<dspfloat_t>::kLowpass);
		} else
		if (type == k2pole ){
			hpf[0].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kHighpass);
			hpf[1].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kBypass);
			lpf[0].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kLowpass);
			lpf[1].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kBypass);
		} else
		if (type == kBypass) {
			hpf[0].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kBypass);
			hpf[1].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kBypass);
			lpf[0].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kBypass);
			lpf[1].design(fs, fc, kQ2pole, 0, BiquadFilter<dspfloat_t>::kBypass);
		}
#else
		// Pirkle design - 2nd order only
        
		double theta = kPi * fc / fs;
		double omega = kPi * fc;
		double kappa = omega / tan(theta);
		double delta = kappa * kappa + omega * omega + 2 * kappa * omega;
		
		BiquadStruct b;
		
		// LPF & HPF B(z)
		
		b.B[0] = 1.0;
		b.B[1] = (-2. * kappa * kappa + 2. * omega * omega) / delta;
		b.B[2] = (-2. * kappa * omega + kappa * kappa + omega * omega) / delta;

		// LPF A(z)

		b.A[0] = b.A[2] = omega * omega / delta;
		b.A[1] = 2. * b.A[0];
		
		lpf[0].design(b);
		
		// HPF A(z)
		
		b.A[0] = b.A[2] = kappa * kappa / delta;
		b.A[1] = - 2. * b.A[0];
		
		hpf[0].design(b);
#endif
	}
	
	// run the crossover
	void run(dspfloat_t x, dspfloat_t ( &y )[2])
	{
		y[0] = hpf[0].runInterp(x);
		y[0] = hpf[1].runInterp(y[0]);
		y[1] = lpf[0].runInterp(x);
		y[1] = lpf[1].runInterp(y[1]);
	}
	
    void getHpfResponse(float freq_hz[], float mag_db[], int numPoints)
    {
        for (int n = 0; n < numPoints; n++) {
            mag_db[n] = hpf[0].getMagnitudeResponse(freq_hz[n]);
            mag_db[n] += hpf[1].getMagnitudeResponse(freq_hz[n]);
        }
    }

    void getLpfResponse(float freq_hz[], float mag_db[], int numPoints)
    {
        for (int n = 0; n < numPoints; n++) {
            mag_db[n] = lpf[0].getMagnitudeResponse(freq_hz[n]);
            mag_db[n] += lpf[1].getMagnitudeResponse(freq_hz[n]);
        }
    }
    
private:
	static constexpr double kPi = 3.14159265358979323846;

	BiquadFilter<dspfloat_t> hpf[2];
	BiquadFilter<dspfloat_t> lpf[2];
};

