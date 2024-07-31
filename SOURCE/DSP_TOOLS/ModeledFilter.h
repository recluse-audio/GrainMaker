/*
  ==============================================================================

    ModeledFilter.h
    Created: 26 Nov 2018 10:49:02am
    Author:  Andrew

  ==============================================================================
*/

#pragma once
//#define FILTER_DEBUG 1

#ifdef FILTER_DEBUG
#include <iostream>
#include <iomanip>
#endif
#include <cassert>
#include <math.h>


template <typename dspfloat_t>
class ModeledFilter
{
public:
    enum Type {
        kLowpass=0,
        kHighpass,
        kBandpass,
        kParallel
    };
    
    enum Topology {
        kPassive=0,
        kSallenKey,
        kMultipleFeedback
    };
    
    struct BOM {
        float R1;
        float R2;
        float R3;
        //float R4;
        float C1;
        float C2;
        float C3;
        //float C4;
    };
    
    ModeledFilter()
	{
		for (int n=0; n < kMaxBiquads; n++)
		{
			// set filters to bypass
			bqf[n].b0_ = 1;
			bqf[n].b1_ = 0;
			bqf[n].b2_ = 0;
			bqf[n].a0_ = 1;
			bqf[n].a1_ = 0;
			bqf[n].a2_ = 0;
		}
		
		reset(); // clear filter state
	};
	
    ~ModeledFilter() { /* nothing to see here */ };
    
    void design(float fs, int stage, int topology, int type, BOM& bom)
    {
        assert (fs != 0);
        Fs_ = fs;
        
        dspfloat_t f0 = 0;
        
        // calculate Laplace coefficients H(s) = B(s) / A(s)
        // H(s) = b0 * s^2 + b1 * s + b2 / a0 * s^2 + a1 * s + a2
        
        dspfloat_t a[3];
        dspfloat_t b[3];
        
        switch (topology) {
            case kPassive:
                if (type == kLowpass) {
                    f0 = 1. / (2 * kPi * bom.R1 * bom.C1);
                    b[0] = 0.;
                    b[1] = 0.;
                    b[2] = 1. / (bom.R1 * bom.C1);
                    a[0] = 0.;
                    a[1] = 1.;
                    a[2] = 1. / (bom.R1 * bom.C1);
                }
                else
                if (type == kHighpass) {
                    f0 = 1. / (2 * kPi * bom.R1 * bom.C1);
                    b[0] = 0.;
                    b[1] = 1.;
                    b[2] = 0.;
                    a[0] = 0.;
                    a[1] = 1.;
                    a[2] = 1. / (bom.R1 * bom.C1);
                }
                else
                if (type == kParallel) {
                    f0 = 1. / (bom.R1 * bom.C1);
                    b[0] = 0.;
                    b[1] = 0.;
                    b[2] = bom.R1;
                    a[0] = 0.;
                    a[1] = bom.R1 * bom.C1;
                    a[2] = 1.;
                }
                break;
            case kSallenKey:
                if (type == kLowpass) {
                    f0 = 1. / (2 * kPi * sqrtf(bom.R1 * bom.C1 * bom.R2 * bom.C2));
                    b[0] = 0.;
                    b[1] = 0.;
                    b[2] = 1. / (bom.R1 * bom.C1 * bom.R2 * bom.C2);
                    a[0] = 1.;
                    a[1] = 1. / (bom.R2 * bom.C1) + 1. / (bom.R1 * bom.C1);
                    a[2] = 1. / (bom.R1 * bom.C1 * bom.R2 * bom.C2);
                }
                else
                if (type == kHighpass) {
                    f0 = 1. / (2 * kPi * sqrtf(bom.R1 * bom.C1 * bom.R2 * bom.C2));
                    b[0] = 1.;
                    b[1] = 0.;
                    b[2] = 0.;
                    a[0] = 1.;
                    a[1] = 1. / (bom.R2 * bom.C1) + 1. / (bom.R2 * bom.C2);
                    a[2] = 1. / (bom.R1 * bom.C1 * bom.R2 * bom.C2);
                }
                break;
            case kMultipleFeedback:
                if (type == kLowpass) {
                    f0 = 1. / (2 * kPi * sqrtf(bom.R2 * bom.R3 * bom.C1 * bom.C2));
                    b[0] = 0.;
                    b[1] = 0.;
                    b[2] = 1. / (bom.C1 * bom.C2 * bom.R1 * bom.R2);
                    a[0] = 1.;
                    a[1] = (1. / bom.C1) * (1. / bom.R1 + 1. / bom.R2 + 1. / bom.R3);
                    a[2] = 1. / (bom.C1 * bom.C2 * bom.R2 * bom.R3);
                }
                else
                if (type == kHighpass) {
                    f0 = 1. / (2 * kPi * sqrtf(bom.R1 * bom.R2 * bom.C2 * bom.C3));
                    b[0] = -1. / (bom.C1 / bom.C3);
                    b[1] = 0.;
                    b[2] = 0.;
                    a[0] = 1.;
                    a[1] = (bom.C1 + bom.C2 + bom.C3) / (1. / (bom.R1 * bom.R2 * bom.C2 * bom.C3));
                    a[2] = 1. / (bom.R1 * bom.R2 * bom.C2 * bom.C3);
                }
                else
                if (type == kBandpass) {
                    f0 = (1. / (2 * kPi)) * sqrtf((1. / (bom.R3 * bom.C1 * bom.C2)) * (1. / bom.R1 + 1. / bom.R2));
                    b[0] = 0.;
                    b[1] = -1. / (bom.R1 * bom.C1);
                    b[2] = 0.;
                    a[0] = 1.;
                    a[1] = 1. / (bom.R3 * bom.C2) + 1. / (bom.R3 * bom.C1);
                    a[2] = (1. / (bom.R3 * bom.C1 * bom.C2)) * (1. / bom.R1 + 1. / bom.R2);
                }
                break;
        };
#if 1
        //std::cout << std::fixed << std::setprecision(12);
        std::cout << "laplace " << stage + 1 << ":\n";
        std::cout << "f0 = " << f0 << "\n";
        std::cout << "b0 = " << b[0] << "\nb1 = " << b[1] << "\nb2 = " << b[2] << "\n";
        std::cout << "a0 = " << a[0] << "\na1 = " << a[1] << "\na2 = " << a[2] << "\n";
        std::cout << "\n";
#endif

        // bilinear transform - convention is H(z) = B(z) / A(z)
        
        bqf[stage].f0_ = f0;

        dspfloat_t c = 2 * Fs_;
        dspfloat_t c2 = c * c;
        
        // numerator B(z)
        bqf[stage].b0_ = b[0] * c2 + b[1] * c + b[2];
        bqf[stage].b1_ = -2 * b[0] * c2 + 2 * b[2];
        bqf[stage].b2_ = b[0] * c2 - b[1] * c + b[2];
        
        // denominator A(z)
        bqf[stage].a0_ = a[0] * c2 + a[1] * c + a[2];
        bqf[stage].a1_ = -2 * a[0] * c2 + 2 * a[2];
        bqf[stage].a2_ = a[0] * c2 - a[1] * c + a[2];
        
        // normalize coefficients
        
        dspfloat_t norm = 1. / bqf[stage].a0_;
        
        bqf[stage].b0_ *= norm;
        bqf[stage].b1_ *= norm;
        bqf[stage].b2_ *= norm;
        bqf[stage].a0_ *= norm;
        bqf[stage].a1_ *= norm;
        bqf[stage].a2_ *= norm;
        
        bqf[stage].g0_ = 1. / getBiquadResponse(bqf[stage], f0);
        
#ifdef FILTER_DEBUG
        //std::cout << std::fixed << std::setprecision(12);
        std::cout << "biquad " << stage + 1 << ":\n";
        std::cout << "c0 = " << norm << "\n";
        std::cout << "g0 = " << g0_ << "\n";
        std::cout << "b0 = " << bqf[stage].b0_ << "\nb1 = " << bqf[stage].b1_ << "\nb2 = " << bqf[stage].b2_ << "\n";
        std::cout << "a0 = " << bqf[stage].a0_ << "\na1 = " << bqf[stage].a1_ << "\na2 = " << bqf[stage].a2_ << "\n";
        std::cout << "\n";
#endif
    };
    
    inline void reset(void)
    {
        for (int n=0; n < kMaxBiquads; n++) {
            // zero the filter state
            bqf[n].x1_ = bqf[n].x2_ = bqf[n].y1_ = bqf[n].y2_ = 0.;
        }
    };
    
    // run cascaded biquad filters
    inline dspfloat_t run(dspfloat_t x_in, int stages)
    {
        double x = x_in;
        double y = 0;
        
        for (int n=0; n < stages; n++) {
            // run the filter - calculate output sample
            y = bqf[n].b0_ * x + bqf[n].b1_ * bqf[n].x1_ + bqf[n].b2_ * bqf[n].x2_ - bqf[n].a1_ * bqf[n].y1_ - bqf[n].a2_ * bqf[n].y2_;
            // update filter state
            bqf[n].x2_ = bqf[n].x1_; bqf[n].x1_ = x;
            bqf[n].y2_ = bqf[n].y1_; bqf[n].y1_ = y;
            // next stage filter input is previous stage filter output
            x = y * bqf[n].g0_;
        }
        return y;
    };
    
    // get cascaded filter magnitude response
    dspfloat_t getMagnitudeResponse(float freq_hz, int stages = 1)
    {
        dspfloat_t magnitude = 1.;
        
        // loop over all biquad sections
        for (int n=0; n < stages; n++) {
            magnitude *= getBiquadResponse(bqf[n], freq_hz);
        }
        return 20 * log10(magnitude);
    };
    
private:
    dspfloat_t Fs_ = 44100; // sample rate
    //int N_ = 2; // number of stages
    
    static const int kMaxBiquads = 4;
    const dspfloat_t kPi = 3.14159265426;
    
    struct Biquad {
        dspfloat_t b0_;
        dspfloat_t b1_;
        dspfloat_t b2_;
        dspfloat_t a0_;
        dspfloat_t a1_;
        dspfloat_t a2_;
        dspfloat_t g0_;
        dspfloat_t x1_;
        dspfloat_t x2_;
        dspfloat_t y1_;
        dspfloat_t y2_;
        dspfloat_t f0_;
    } bqf[kMaxBiquads];

    // Calculates magnitude reponse |H(z)| for single biquad filter stage at freq_hz
    // from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
    //
    dspfloat_t getBiquadResponse(Biquad b, float freq_hz)
    {
        assert(Fs_ != 0);
        
        dspfloat_t w = 2 * kPi * freq_hz / Fs_;
        
        dspfloat_t cos1 = cos(-1 * w);
        dspfloat_t cos2 = cos(-2 * w);
        
        dspfloat_t sin1 = sin(-1 * w);
        dspfloat_t sin2 = sin(-2 * w);
        
        dspfloat_t realZeros = b.b0_ + b.b1_ * cos1 + b.b2_ * cos2;
        dspfloat_t imagZeros = b.b1_ * sin1 + b.b2_ * sin2;
        
        dspfloat_t realPoles = 1 + b.a1_ * cos1 + b.a2_ * cos2;
        dspfloat_t imagPoles = b.a1_ * sin1 + b.a2_ * sin2;
        
        dspfloat_t divider = realPoles * realPoles + imagPoles * imagPoles;
        
        dspfloat_t realHw = (realZeros * realPoles + imagZeros * imagPoles) / divider;
        dspfloat_t imagHw = (imagZeros * realPoles - realZeros * imagPoles) / divider;
        
        dspfloat_t magnitude = sqrt(realHw * realHw + imagHw * imagHw);
        
        return magnitude;
    };
};
