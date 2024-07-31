/*
  ==============================================================================

    SpectralTiltEQ.h
    Created: 16 Dec 2021 4:33:21pm
    Author:  Andrew

  ==============================================================================
*/

#pragma once

#include <cmath>
#include <array>
#include <memory>
#include <vector>

#define TDF_II 1 // Transposed Direct Form II

/**
 * H(z) = (b0d + b1d * z-1) / (1 - a1d * z-1)
 */
class SpectralTiltEQ_TF1S {
public:

    SpectralTiltEQ_TF1S() { setSampleRate(SR, 0.); }
    
	void setSampleRate(double sr, double tc)
    {
		SR = sr;
        TC = tc;
	}

	void design(double b1, double b0, double a0, double w1)
    {
        // discretize via bilinear transform
		double c = 1 / tan(w1 * 0.5 / SR);
		double d = a0 + c;
        
		b1d = (b0 - b1 * c) / d;
		b0d = (b0 + b1 * c) / d;
		a1d = (a0 - c) / d;
		g0d = a0 / b0;
	}

    // mono / stereo
    inline void run(double ( &u )[2], bool stereo = false)
    {
        // smooth coefficients
        b0f = TC * b0f + (1. - TC) * b0d;
        b1f = TC * b1f + (1. - TC) * b1d;
        a1f = TC * a1f + (1. - TC) * a1d;
        g0f = TC * g0f + (1. - TC) * g0d;
        
        // run the filter
        double yout = 0;
        int N = stereo ? 2 : 1;
        for (int n = 0; n < N; n++)
        {
#ifdef TDF_II
            yout = b0f * u[n] + x[n];
            x[n] = b1f * u[n] - a1f * yout;
            u[n] = yout * g0f; // output sample
#else
            yout = b0f * u[n] + b1f * x[n] - a1f * y[n];
            x[n] = u[n];
            y[n] = yout;
            u[n] = yout * g0f; // output sample
#endif
        }
    }
    
    // mono only
    inline double run(double u)
    {
        // smooth coefficients
        b0f = TC * b0f + (1. - TC) * b0d;
        b1f = TC * b1f + (1. - TC) * b1d;
        a1f = TC * a1f + (1. - TC) * a1d;
        g0f = TC * g0f + (1. - TC) * g0d;

        // run the filter
        double yout = 0;
#ifdef TDF_II
        yout = b0f * u + x[0];
        x[0] = b1f * u - a1f * yout;
#else
        yout = b0f * u + b1f * x[0] - a1f * y[0];
        x[0] = u;
        y[0] = yout;
#endif
        return yout * g0f; // output sample
    }

    //
    // calculate filter magnitude response
    // from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
    //
    float power(float cos1, float sin1) const
    {
        float realZeros = static_cast<float>((b0d + b1d * cos1) * g0d);
        float imagZeros = static_cast<float>(b1d * sin1 * g0d);

        float realPoles = static_cast<float>(1.f + a1d * cos1);
        float imagPoles = static_cast<float>(a1d * sin1);

        float divider = realPoles * realPoles + imagPoles * imagPoles;

        float realHw = (realZeros * realPoles + imagZeros * imagPoles) / divider;
        float imagHw = (imagZeros * realPoles - realZeros * imagPoles) / divider;

        float power = realHw * realHw + imagHw * imagHw;
        return power;
    }
    
private:
    static constexpr double kPi = 3.141592653589793238;
	double SR = 44100.0;
    double TC = 0; // coefficient smoothing

    // design coeffs
    double b0d = 1.0; // bypass
	double b1d = 0.0;
	double a1d = 0.0;
	double g0d = 1.0;
    
    // smoothed coeffs
    double b0f = 1.0; // bypass
    double b1f = 0.0;
    double a1f = 0.0;
    double g0f = 1.0;
    
    // state
    double x[2] = {0., 0.};
#ifndef TDF_II
    double y[2] = {0., 0.};
#endif
};

/**
 
 */
class SpectralTiltEQ {

public:
    static constexpr float kSlopeDbPerOctMin = -3.f;
    static constexpr float kSlopeDbPerOctMax = 3.f;
    static constexpr float kPivotFcDef = 750.f;

    enum Param {
        kSolo = 0,
        kFreq,
        kSlope,
        kPivot,
        kWidth,
        kStages,
        kEnable
    };
    
    SpectralTiltEQ()
    {
        setNumFilters(filterN);
        setSampleRate(SR);
    }
    
    void setParam(int paramId, float value)
    {
        switch (paramId) {
            case kFreq:
                f0 = value;
                design();
                break;
            case kWidth:
                bw = value;
                design();
               break;
            case kSlope:
                // convert from {-3, 3} dB / oct to {-.5, .5} alpha
                slope = value;
                alpha = slope / (kSlopeDbPerOctMax - kSlopeDbPerOctMin);
                design();
                break;
            case kPivot:
                fc = value;
                design();
                break;
            case kStages:
                setNumFilters((size_t)value);
                break;
            case kEnable:
                enable = (bool)value;
#if 0
                if (enable)
                    g[kTarget] = dBToGain(g_db);
                else
                    g[kTarget] = 0.; // will fade to 0
#endif
                break;
            case kSolo:
                solo = (bool)value;
                slope = -slope;
                alpha = -alpha;
                design();
                break;
            default:
                // uh oh
                break;
        }
    }
    
    inline bool isOn(void) const { return enable; }
    inline bool inSolo(void) const { return solo; }

    void setSampleRate(double sr)
    {
        SR = sr;
        T = 1 / SR;
        
        rampTc = onePoleCoeff(kSmoothMsec); // set coefficient smooth time
        
        for (size_t i = 0; i < filterArray.size(); i++)
            filterArray[i].setSampleRate(SR, rampTc);
        
        design(); // re-design the filter array w/ current params
    }

    // set number of one pole filters
    void setNumFilters(size_t N)
    {
        filterN = N; // N must be 2 or more
		filterArray.clear();
        
        // add filter objects to array
		for (size_t i = 0; i < filterN; i++)
            filterArray.push_back(SpectralTiltEQ_TF1S());
        
        // (re)initialize filters for current sample rate
        for (size_t i = 0; i < filterN; i++)
            filterArray[i].setSampleRate(SR, rampTc);
        
        design(); // re-design the filter array w/ current params
	}

    // process mono | stereo samples
    inline void run(double ( &xi )[2], double ( &xo )[2], bool stereo = false)
    {
        g[kActive] = rampTc * g[kActive] + (1. - rampTc) * g[kTarget];
        
        double u[2];
        u[0] = xi[0];
        u[1] = xi[1];
        
        bool on = (alpha != 0) && enable && !updating;
        
        if (on) {
            for (size_t i = 0; i < filterN; i++)
                filterArray[i].run(u, stereo); // in place filtering
            
            u[0] *= g[kActive];
            if (stereo)
                u[1] *= g[kActive]; // if !stereo, just pass through R channel
        }
        // copy result to output
        xo[0] = u[0];
        xo[1] = u[1];
	}
    
    // process mono sample only
    inline double run(double xi)
    {
        g[kActive] = rampTc * g[kActive] + (1. - rampTc) * g[kTarget];
        
        bool on = (alpha != 0) && enable && !updating;

        double u = xi;
        if (on) {
            for (size_t i = 0; i < filterN; i++)
                u = filterArray[i].run(u); // in place filtering
            
            u *= g[kActive];
        }
        return u;
    }

    // calculate filter magnitude response at a single frequency point
    float magnitude(float freq_hz)
    {
        this->setFreqPoint(freq_hz);
        
        double h = 1.f;
        for (size_t i = 0; i < filterArray.size(); i++)
            h *= filterArray[i].power(cos1, sin1);
        
        float m = sqrtf(fmax(float(h), 0.f)) * float(g[kTarget]);
        return (m <= 1e-05f) ? -100.f : 20.f * log10f(m);
    }

    // calculate filter magnitude response over vector of frequency points
    void magnitude(float freq_hz[], float mag_db[], int numPoints)
    {
        for (int i = 0; i < numPoints; i++)
            mag_db[i] = magnitude(freq_hz[i]);
    }
    
    // Define EQ band state
    struct EqState
    {
        bool eqOn;
        bool solo;
        float freq;
        float gain;
    };

    // Get current EQ state
    void getEqState(EqState& state)
    {
        state.eqOn = enable;
        state.solo = solo;
        state.freq = float(fc);
        state.gain = float(slope);
    }

private:
    // filter design helpers
    
    void design(void)
    {
        updating.store(true);
        
        w0 = 2. * kPi * f0;
        f1 = fmin(f0 + bw, SR / 2); // limit to Nyquist
        r = pow((f1 / f0), (1.0 / static_cast<double>(filterN))); // N must be at least 2

        for (size_t i = 0; i < filterN; i++)
            filterArray[i].design(1.0, mzh((int)i), mph((int)i), 1.0);
        
        // determine gain to center tilt at the pivot frequency
        
        g[kTarget] = 1.0; // reset gain so it's not applied to magnitude
        g_db = -magnitude((float)fc);
        g[kTarget] = dBToGain(g_db);
        
        updating.store(false);
    }
    
	inline double prewarp(double w, double t, double wp) const {
        // warped w
		return wp * tan(w * t / 2) / tan(wp * t / 2);
	}
	
	inline double mz(int i) const {
        // analog zero
		return w0 * pow(r, (-alpha + i));
	}
	
	inline double mp(int i) const {
        // analog pole
		return w0 * pow(r, i);
	}
	
	inline double mzh(int i) const {
        // prewarped zero
		return prewarp(mz(i), T, w0);
	}
	
	inline double mph(int i) const {
        // prewarped pole
		return prewarp(mp(i), T, w0);
	}
	
private:
    std::vector<SpectralTiltEQ_TF1S> filterArray;
    std::atomic<bool> updating{false};
    
    static constexpr float kSmoothMsec = 50.f;
    static constexpr double kPi = 3.141592653589793238;

	size_t filterN = 24; // # of filter stages
    bool enable = true;
    bool solo = false;

	double SR = 44100.0;
	double T = 1 / 44100.0;
    double r = 40.0;

    double fc = kPivotFcDef; // default pivot frequency
    double f0 = 16.0;
    double f1 = SR / 2.;
    double bw = f1 - f0;
    double w0 = 2. * kPi * f0;

	double alpha = 0.0; // tilt coefficient
    double slope = 0.0; // tilt parameter dB/oct
    
    // pre-compute cos / sin values at a frequency point,
    // in order to optimize magnitude response calculation
    void setFreqPoint(float freq_hz)
    {
        float w = float(2 * kPi * freq_hz / SR);
        cos1 = cos(-1 * w);
        sin1 = sin(-1 * w);
    }

    // for magnitude response
    float sin1;
    float cos1;

    inline double dBToGain(double db)
    {
        return pow(10., db / 20.);
    }
    
    enum {
        kActive = 0,
        kTarget
    };
    
    double g[2] = {1., 1.};
    double g_db = 0;
    double rampTc = 0;
    
    inline double onePoleCoeff(double tau)
    {
        if(tau > 0.0) {
            return exp(-1.0 / (tau * .001 * SR));
        }
        return 1.0;
    }
};
