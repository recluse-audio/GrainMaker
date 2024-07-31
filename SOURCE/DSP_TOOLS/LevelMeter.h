/*
  ==============================================================================

    LevelMeter.h
    Created: 2 Feb 2020 10:35:16am
    Author:  Andrew

  ==============================================================================
*/

#pragma once

#include <cmath>
#include <atomic>
#include <cassert>

template <typename dspfloat_t>
class LevelMeter
{
public:
    LevelMeter()
    {
        decayCoeff = exp(-1.f / (44100.f * kDecayTimeDefault));
        clear();
    }
    
    ~LevelMeter() { /* nothing to see here */}
	
	enum Type {
		kPeak=0,
		kRMS=1
	};
    
    enum Chan {
        kChanL=0,
        kChanR=1
    };
    
    enum Mode {
        kClearAfterRead = 1,
        kNoClearAfterRead = 0
    };
    
    /// Set ballistics decay time in milliseconds
    inline void setDecayMs(float decay_ms, float fs)
    {
        decayCoeff = (decay_ms == 0) ? 1.f : expf(-1.f / (fs * decay_ms * 0.001f));
    }
    
    /// Run mono-to-mono level meter
    inline dspfloat_t run(dspfloat_t x, int type = kPeak)
    {
        if (reset.exchange(false)) {
            level[0] = 0;
        }

		if (type == kRMS) {
			state[0] = decayCoeff * state[0] + (1.f - decayCoeff) * (x * x);
			level[0] = sqrt(fmax(state[0], 1e-06f));
		} else {
        	level[0] = fmax(level[0], fabs(x)) * decayCoeff;
		}
        
        return level[0];
    }
    
    /// Run stereo-to-mono level meter - peak is max(L, R)
    inline dspfloat_t run(dspfloat_t ( &x )[2], int type = kPeak, bool stereo = true)
    {
        if (reset.exchange(false)) {
            level[0] = 0;
        }
        
        dspfloat_t u = 0.;
        switch (type) {
            case kRMS:
                u = x[0] * x[0];
                if (stereo)
                    u = fmax(u, x[1] * x[1]);
                state[0] = decayCoeff * state[0] + (1.f - decayCoeff) * float(u);
                level[0] = sqrt(fmax(state[0 ], 1e-06f));
                break;
            case kPeak:
                u = fabs(x[0]);
                if (stereo)
                    u = fmax(u, fabs(x[1]));
                level[0] = fmax(level[0], u) * decayCoeff;
                break;
            default:
                break;
        }
        
        return level[0];
    }

    /// Run stereo-to-stereo level meter - independent L/R channels
    void run(dspfloat_t ( &x )[2], dspfloat_t ( &m )[2], int type = kPeak)
    {
        if (reset.exchange(false)) {
            level[0] = 0;
            level[1] = 0;
        }

        if (type == kRMS) {
            double l = x[0] * x[0]; // L channel
            state[0] = decayCoeff * state[0] + (1.f - decayCoeff) * dspfloat_t(l);
            level[0] = sqrt(fmax(state[0], 1e-06f));
            double r = x[1] * x[1]; // R channel
            state[1] = decayCoeff * state[0] + (1.f - decayCoeff) * dspfloat_t(r);
            level[1] = sqrt(fmax(state[1], 1e-06f));
        } else {
            level[0] = fmax(fabs(x[0]), level[0]) * decayCoeff;
            level[1] = fmax(fabs(x[1]), level[1]) * decayCoeff;
        }
        
        // update pass-by-reference values
        m[0] = level[0];
        m[1] = level[1];
    }
    
    /// Clear the meter levels
	inline void clear(void)
    {
        reset.store(true);
		//level[0] = 0;
        //level[1] = 0;
	}
    
    inline dspfloat_t get(bool clearAfterRead = kNoClearAfterRead)
    {
        dspfloat_t v = static_cast<dspfloat_t>(level[0]);
        reset.store(clearAfterRead);
        return v;
    }
    
    /// Get mono channel level (L or R)
    inline dspfloat_t get(int chan, bool clearAfterRead = kNoClearAfterRead)
    {
        dspfloat_t v = (chan == kChanL) ? level[0] : level[1];
        reset.store(clearAfterRead);
        return static_cast<dspfloat_t>(v);
    }
    
    /// Get mono channel level in dB (L or R)
    inline dspfloat_t getDb(int chan = kChanL, float minDb = -70.f, bool clearAfterRead = kNoClearAfterRead)
    {
        dspfloat_t v = linToDb((chan == kChanL) ? level[0] : level[1], minDb);
        reset.store(clearAfterRead);
        return v;
    }

    /// Get stereo meter levels
    inline void get(dspfloat_t ( &v )[2], bool clearAfterRead = kNoClearAfterRead)
    {
        v[0] = level[0];
        v[1] = level[1];
        reset.store(clearAfterRead);
    }
    
    /// Get stereo meter levels in dB
    inline void getDb(dspfloat_t ( &v )[2], float minDb = -70.f, bool clearAfterRead = kNoClearAfterRead)
    {
        v[0] = linToDb(level[0], minDb);
        v[1] = linToDb(level[0], minDb);
        reset.store(clearAfterRead);
    }

private:
    static constexpr float kDecayTimeDefault = 0.2f; // 200 milliseconds

    // meter state
    dspfloat_t state[2] = {0, 0};
    dspfloat_t decayCoeff = 0;

    dspfloat_t level[2] = {0, 0};
    std::atomic<bool> reset = true;

    // Convert from linear (scalar) value to decibels
    inline dspfloat_t linToDb(dspfloat_t x, dspfloat_t minDb = -70.f)
    {
        return (x == 0) ? minDb : 20.f * log10f(x);
    }
};

#if 0
// True-Peak metering w/ 4x oversampling

// FIR interpolator coefficients @48KHz
static double fir[12][4] = {
	{0.0017089843750, −0.0291748046875, −0.0189208984375, −0.0083007812500},
	{0.0109863281250, 0.0292968750000, 0.0330810546875, 0.0148925781250},
	{−0.0196533203125, −0.0517578125000, −0.0582275390625, −0.0266113281250},
	{0.0332031250000, 0.0891113281250, 0.1015625000000, 0.0476074218750},
	{−0.0594482421875, −0.1665039062500, −0.2003173828125, −0.1022949218750},
	{0.1373291015625, 0.4650878906250, 0.7797851562500, 0.9721679687500},
	{0.9721679687500, 0.7797851562500, 0.4650878906250, 0.1373291015625},
	{−0.1022949218750, −0.2003173828125, −0.1665039062500, −0.0594482421875},
	{0.0476074218750, 0.1015625000000, 0.0891113281250, 0.0332031250000},
	{−0.0266113281250, −0.0582275390625, −0.0517578125000, −0.0196533203125},
	{0.0148925781250, 0.0330810546875, 0.0292968750000, 0.0109863281250},
	{−0.0083007812500, −0.0189208984375, −0.0291748046875, 0.0017089843750}
};

// Run true peak meter
dspfloat_t truePeakMeter(dspfloat_t xin)
{
	double xos[4] = {0, 0, 0, 0};
	for (int j=0; j < 4; j++) {
		for (int i=0; i < 12; i++) {
			xos[j] += fir[i][j] * xin;
		}
		level = fmax(fabs(xos[j]), level);
	}
	level *= decay;
	return static_cast<dspfloat_t>(level);
}
#endif
