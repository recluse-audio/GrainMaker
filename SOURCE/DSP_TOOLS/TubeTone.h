/*
  ==============================================================================

    TubeTone.h
    Created: 27 Aug 2018 9:00:55am
    Author:  Antares

  ==============================================================================
*/

#pragma once

#include <assert.h>
#ifdef FILTER_DEBUG
#include <iostream>
#include <iomanip>
#endif

#include <cmath>

//#define FILTER_DEBUG

// Digital model of '59 Fender Bassman tone stack
// from https://ccrma.stanford.edu/~dtyeh/papers/yeh06_dafx.pdf
//
template <typename dspfloat_t>
class TubeTone
{
public:
    enum {
        kLow=0,
        kMid,
        kTop,
        kGain,
		kBypass
    };
    
    struct Coeffs {
        dspfloat_t B0;
        dspfloat_t B1;
        dspfloat_t B2;
        dspfloat_t B3;
        dspfloat_t A1;
        dspfloat_t A2;
        dspfloat_t A3;
    };
    
    TubeTone()
    {
        // reset control params
        t = 0; m = 0; l = 0;
        
        g = dbToGain(10.); // +10 dB boost
        
        // create an intial tone stack
        setSampleRate(44100.);
    }
    
    ~TubeTone(){}
    
    void setSampleRate(float fs)
    {
        fs_ = fs;
        design();
        reset();
    }
	
	void setBypass(void)
	{
		// set bypass
		A0 = 1.; A1 = 0.; A2 = 0.; A3 = 0.;
		B0 = 1.; B1 = 0.; B2 = 0.; B3 = 0.;
	}
	
    void setControl(int paramId, float param)
    {
        switch(paramId) {
            case kLow:
                l = param;
                design();
                break;
            case kMid:
                m = param;
                design();
                break;
            case kTop:
                t = param;
                design();
                break;
            case kGain:
				g_db = param;
                g = dbToGain(g_db);
                break;
			default:
			case kBypass:
				if ((bool)param) {
					setBypass();
					g = 1.; // unity gain
				} else {
					design();
					g = dbToGain(g_db);
				}
				break;
        };
    }
    
    void reset(void)
    {
        x1 = x2 = x3 = 0.;
        y1 = y2 = y3 = 0.;
    }
    
    void design(void)
    {
        // calculate analog poles and zeros

        // analog R, C values
        constexpr dspfloat_t C1 = 0.25e-09f; // 0.25 nF
        constexpr dspfloat_t C2 = 20e-09f;   // 20 nF
        constexpr dspfloat_t C3 = 20e-09f;   // 20 nF
        constexpr dspfloat_t R1 = 250e3;    // 250K
        constexpr dspfloat_t R2 = 1e06;     // 1M ohm
        constexpr dspfloat_t R3 = 25e03;    // 25K
        constexpr dspfloat_t R4 = 56e03;    // 56K
        
        // R combinations
        constexpr dspfloat_t R1R2 = R1 * R2;
        constexpr dspfloat_t R1R3 = R1 * R3;
        constexpr dspfloat_t R1R4 = R1 * R4;
        constexpr dspfloat_t R2R3 = R2 * R3;
        constexpr dspfloat_t R2R4 = R2 * R4;
        constexpr dspfloat_t R3R3 = R3 * R3;
        constexpr dspfloat_t R3R4 = R3 * R4;
        constexpr dspfloat_t R1R2R3 = R1 * R2R3;
        constexpr dspfloat_t R1R3R4 = R1 * R3R4;
        constexpr dspfloat_t R2R3R4 = R2 * R3R4;
        constexpr dspfloat_t R1R2R4 = R1 * R2R4;
        constexpr dspfloat_t R1R3R3 = R1 * R3R3;
        constexpr dspfloat_t R3R3R4 = R3R3 * R4;
        
        // C combinations
        constexpr dspfloat_t C1C2 = C1 * C2;
        constexpr dspfloat_t C1C3 = C1 * C3;
        constexpr dspfloat_t C2C3 = C2 * C3;
        constexpr dspfloat_t C1C2C3 = C1 * C2C3;
        
        // R and C combinations
        constexpr dspfloat_t C1R1 = C1 * R1;
        constexpr dspfloat_t C1R2 = C1 * R2;
        constexpr dspfloat_t C1R3 = C1 * R3;
        constexpr dspfloat_t C2R2 = C2 * R2;
        constexpr dspfloat_t C2R3 = C2 * R3;
        constexpr dspfloat_t C2R4 = C2 * R4;
        constexpr dspfloat_t C3R3 = C3 * R3;
        constexpr dspfloat_t C3R4 = C3 * R4;

        constexpr dspfloat_t C1C2R1R2 = C1C2 * R1R2;
        constexpr dspfloat_t C1C2R1R3 = C1C2 * R1R3;
        constexpr dspfloat_t C1C2R1R4 = C1C2 * R1R4;
        constexpr dspfloat_t C1C2R2R4 = C1C2 * R2R4;
        constexpr dspfloat_t C1C2R3R4 = C1C2 * R3R4;
        constexpr dspfloat_t C1C3R1R3 = C1C3 * R1R3;
        constexpr dspfloat_t C1C3R2R3 = C1C3 * R2R3;
        constexpr dspfloat_t C1C3R2R4 = C1C3 * R2R4;
        constexpr dspfloat_t C1C3R3R3 = C1C3 * R3R3;
        constexpr dspfloat_t C1C3R3R4 = C1C3 * R3R4;
        constexpr dspfloat_t C1C3R1R4 = C1C3 * R1R4;
        constexpr dspfloat_t C2C3R2R3 = C2C3 * R2R3;
        constexpr dspfloat_t C2C3R2R4 = C2C3 * R2R4;
        constexpr dspfloat_t C2C3R3R3 = C2C3 * R3R3;
        constexpr dspfloat_t C2C3R3R4 = C2C3 * R3R4;
        
        constexpr dspfloat_t C1C2C3R1R2R3 = C1C2C3 * R1R2R3;
        constexpr dspfloat_t C1C2C3R1R2R4 = C1C2C3 * R1R2R4;
        constexpr dspfloat_t C1C2C3R1R3R3 = C1C2C3 * R1R3R3;
        constexpr dspfloat_t C1C2C3R1R3R4 = C1C2C3 * R1R3R4;
        constexpr dspfloat_t C1C2C3R3R3R4 = C1C2C3 * R3R3R4;
        constexpr dspfloat_t C1C2C3R2R3R4 = C1C2C3 * R2R3R4;

        dspfloat_t mm = m * m;
        dspfloat_t lm = l * m;
        
        dspfloat_t b1, b2, b3;
        dspfloat_t a0, a1, a2, a3;
        
        b1 = t*C1R1 + m*C3R3 + l*(C1R2 + C2R2) + (C1R3 + C2R3);
        
        b2 = t*(C1C2R1R4 + C1C3R1R4) - mm*(C1C3R3R3 + C2C3R3R3)
        + m*(C1C3R1R3 + C1C3R3R3 + C2C3R3R3)
        + l*(C1C2R1R2 + C1C2R2R4 + C1C3R2R4)
        + lm*(C1C3R2R3 + C2C3R2R3)
        + (C1C2R1R3 + C1C2R3R4 + C1C3R3R4);
        
        b3 = lm*(C1C2C3R1R2R3 + C1C2C3R2R3R4)
        - mm*(C1C2C3R1R3R3 + C1C2C3R3R3R4)
        + m*(C1C2C3R1R3R3 + C1C2C3R3R3R4)
        + t*C1C2C3R1R3R4 - t*m*C1C2C3R1R3R4
        + t*l*C1C2C3R1R2R4;
        
        a0 = 1.;
        
        a1 = (C1R1 + C1R3 + C2R3 + C2R4 + C3R4)
        + m*C3R3 + l*(C1R2 + C2R2);
        
        a2 = m*(C1C3R1R3 - C2C3R3R4 + C1C3R3R3
        + C2C3R3R3) + lm*(C1C3R2R3 + C2C3R2R3)
        - mm*(C1C3R3R3 + C2C3R3R3) + l*(C1C2R2R4
        + C1C2R1R2 + C1C3R2R4 + C2C3R2R4)
        + (C1C2R1R4 + C1C3R1R4 + C1C2R3R4
        + C1C2R1R3 + C1C3R3R4 + C2C3R3R4);
        
        a3 = lm*(C1C2C3R1R2R3 + C1C2C3R2R3R4)
        - mm*(C1C2C3R1R3R3 + C1C2C3R3R3R4)
        + m*(C1C2C3R3R3R4 + C1C2C3R1R3R3
        - C1C2C3R1R3R4) + l*C1C2C3R1R2R4
        + C1C2C3R1R3R4;

        // calculate IIR filter coefficients
        
        dspfloat_t z1, z2, z3;
        
        assert(fs_ != 0);
        
        dspfloat_t c1 = 2 * fs_;
        dspfloat_t c2 = c1 * c1;
        dspfloat_t c3 = c1 * c2;
        
        z1 = b1 * c1;
        z2 = b2 * c2;
        z3 = b3 * c3;
        
        // numerator coeffs
        B0 = -z1 - z2 - z3;
        B1 = -z1 + z2 + 3.f*z3;
        B2 =  z1 + z2 - 3.f*z3;
        B3 =  z1 - z2 + z3;
        
        z1 = a1 * c1;
        z2 = a2 * c2;
        z3 = a3 * c3;
        
        // denominator coeffs
        A0 =    a0 - z1 - z2 - z3;
        A1 = -3.f*a0 - z1 + z2 + 3.f*z3;
        A2 = -3.f*a0 + z1 + z2 - 3.f*z3;
        A3 =   -a0 + z1 - z2 + z3;
        
        dspfloat_t norm = 1.f / A0;
        
        // normalize numerator
        B0 *= norm;
        B1 *= norm;
        B2 *= norm;
        B3 *= norm;
        
        // normalize denominator
        A1 *= norm;
        A2 *= norm;
        A3 *= norm;
        
        A0 = 1.;
        
        // get free coeff struct
        bool free = 1 - swap;
        
        // load new coefficients
        H[free].B0 = B0;
        H[free].B1 = B1;
        H[free].B2 = B2;
        H[free].B3 = B3;
        H[free].A1 = A1;
        H[free].A2 = A2;
        H[free].A3 = A3;
        
        // point filter to new coefficients
        swap =  1 - swap;
        
#ifdef FILTER_DEBUG // DEBUG
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "a1 = " << a1 << "\n";
        std::cout << "a2 = " << a2 << "\n";
        std::cout << "a3 = " << a3 << "\n";
        std::cout << "b1 = " << b1 << "\n";
        std::cout << "b2 = " << b2 << "\n";
        std::cout << "b3 = " << b3 << "\n";
        std::cout << "\n";
        std::cout << "A0 = " << A0 << "\n";
        std::cout << "A1 = " << A1 << "\n";
        std::cout << "A2 = " << A2 << "\n";
        std::cout << "A3 = " << A3 << "\n";
        std::cout << "B0 = " << B0 << "\n";
        std::cout << "B1 = " << B1 << "\n";
        std::cout << "B2 = " << B2 << "\n";
        std::cout << "B3 = " << b3 << "\n";
        std::cout << "\n";
#endif
    }
    
    inline dspfloat_t run(dspfloat_t x)
    {
        double y = B0 * x + B1 * x1 + B2 * x2 + B3 * x3 - A1 * y1 - A2 * y2 - A3 * y3;
        x3 = x2; x2 = x1; x1 = x; y3 = y2; y2 = y1; y1 = y;
        return static_cast<dspfloat_t>(y * g);
    }
    
    inline dspfloat_t runBuffered(dspfloat_t x)
    {
        Coeffs* h = &H[swap]; // get pointer to active coefficient struct
        
        // run filter
        dspfloat_t y = h->B0 * x + h->B1 * x1 + h->B2 * x2 + h->B3 * x3 - h->A1 * y1 - h->A2 * y2 - h->A3 * y3;
        x3 = x2; x2 = x1; x1 = x; y3 = y2; y2 = y1; y1 = y;
        return static_cast<dspfloat_t>(y * g);
    }

    // Calculates magnitude reponse |H(z)| for 3 pole filter at freq_hz
    // from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
    //
    dspfloat_t getMagnitudeResponse(float freq_hz)
    {
        const dspfloat_t kPi = 3.14159265358979323846f;
        dspfloat_t w = 2 * kPi * freq_hz / fs_;
        
        dspfloat_t cos1 = cos(-1 * w);
        dspfloat_t cos2 = cos(-2 * w);
        dspfloat_t cos3 = cos(-3 * w);
        
        dspfloat_t sin1 = sin(-1 * w);
        dspfloat_t sin2 = sin(-2 * w);
        dspfloat_t sin3 = sin(-3 * w);
        
        dspfloat_t realZeros = B0 + B1 * cos1 + B2 * cos2 + B3 * cos3;
        dspfloat_t imagZeros = B1 * sin1 + B2 * sin2 + B3 * sin3;
        
        dspfloat_t realPoles = 1 + A1 * cos1 + A2 * cos2 + A3 * cos3;
        dspfloat_t imagPoles = A1 * sin1 + A2 * sin2 + A3 * sin3;
        
        dspfloat_t divider = realPoles * realPoles + imagPoles * imagPoles;
        
        dspfloat_t realHw = (realZeros * realPoles + imagZeros * imagPoles) / divider;
        dspfloat_t imagHw = (imagZeros * realPoles - realZeros * imagPoles) / divider;
        
        dspfloat_t magnitude = sqrt(realHw * realHw + imagHw * imagHw);
        
        return 20 * log10(magnitude * g);   // gain in dB
    }
    
private:
    dspfloat_t fs_;

    dspfloat_t B0, B1, B2, B3;
    dspfloat_t A0, A1, A2, A3;
    
    dspfloat_t x1, x2, x3;
    dspfloat_t y1, y2, y3;
    
    dspfloat_t t, m, l, g, g_db;
    
    Coeffs H[2] = {{1, 0, 0, 0, 0}, {1, 0, 0, 0, 0}}; // bypass
    
    bool swap = false;
    
    inline dspfloat_t dbToGain(dspfloat_t db)
    {
        return static_cast<dspfloat_t>(pow(10.0, db / 20.0));
    }
};
