/*
  ==============================================================================

    FIRFilter.h
    Created: 3 Dec 2019 3:35:50pm
    Author:  Andrew

  ==============================================================================
*/

#pragma once
#include <cmath>

class FIRFilter {
public:
	static const int kMaxTaps = 128;
	
	enum FIRType {
		kBypass=0,
		kLowpass,
		kHighpass
	};
	
    FIRFilter()
    {
        this->reset();
    };
    
    ~FIRFilter()
    {
    };
	
    void design(float fs, float fc, int taps, int kind = kLowpass)
    {
        fs_ = fs;

		if (taps > kMaxTaps)
			return;
		
		M_ = taps; 		// M_ must be even
		N_ = taps + 1; 	// N_ is odd

		// design equations (lowpass, etc...)
        //
        
        const double kPi = 3.14159265358979323846;
        double wc = 2 * kPi * fc / fs_;
        
        double sinc;
        double theta;
        double hamming;
		
		switch(kind) {
			default:
			case kBypass:
				coeff[0] = 1.;
				for (int n = 1; n <= M_; n++) {
					coeff[n] = 0.;
				}
				break;
			case kLowpass:
				// design lowpass filter
				for (int n = 0; n <= M_; n++) {
					hamming = 0.54 - 0.46 * cos(2 * n * kPi / M_);
					theta = wc * (n - (M_ / 2)); // / kPi;
					sinc = (theta == 0) ? 1. : sin(theta) / theta;
					coeff[n] = (wc / kPi) * sinc * hamming;
				}
				break;
			case kHighpass:
				// TODO
				break;
		}
        

        // normalize coefficients for unity gain

        gain = this->calcGain();
        
        for (int n = 0; n <= M_; n++) {
            coeff[n] /= gain;
        }
        
        reset();
    };
    
    inline float run(float x_in)
    {
        if (++index == N_) index -= N_;
        state[index] = x_in; // store new sample to state buffer
        
        int k = index;
        
        double x_out = 0;
        // run filter
        for (int n = 0; n < N_; n++) {
            x_out += coeff[n] * state[k--];
            if (k < 0) k += N_;
        }
        return x_out;
    };
    
    void reset(void) {
        for (int n=0; n < N_; n++) {
            state[n] = 0.;
        }
        index = 0;
    };

	void init(float fs) {
		fs_ = fs;
		design(fs_, fc_, taps_, kind_);
	};

	void setFc(float Fc) {
		fc_ = Fc;
		design(fs_, fc_, taps_, kind_);
	};
	
	void setTaps(int taps) {
		taps_ = taps;
		design(fs_, fc_, taps_, kind_);
	};
	
	void setKind(int kind) {
		kind_ = kind;
		design(fs_, fc_, taps_, kind_);
	};
	
	// calculate the filter gain by summing all of the coefficients
    float calcGain(void) {
        double g = 0;
        for (int n = 0; n < N_; n++) {
            g += coeff[n];
        }
        return g;
    };
    
    // Calculates magnitude reponse |H(z)| for N-tap FIR filter at freq_hz
    // from http://www.earlevel.com/main/2016/12/01/evaluating-filter-frequency-response/
    //
    inline float getMagnitudeResponse(float freq_hz)
    {
        const double kPi = 3.14159265358979323846;
        double w = 2 * kPi * freq_hz / fs_;

        double realHw = 0;
        double imagHw = 0;
        
        for (int n = 0; n < N_; n++) {
            realHw += coeff[n] * cos(-n * w);
            imagHw += coeff[n] * sin(-n * w);
        }
        
        double magnitude = sqrt(realHw * realHw + imagHw * imagHw);
        
        return 20 * log10(magnitude); // magnitude in dB
    };
    
    void getMagnitudeResponse(float* data, int freqMin, int freqMax, int bins, bool normalized = false)
    {
        // create logarithmic frequency vector
        
        float log_min = log2f(freqMin);
        float log_max = log2f(freqMax);
        float dx = (log_max - log_min) / (bins - 1);
        
        data[0] = freqMin;
        float alpha = powf(2., dx);
        for (int n = 1; n < bins; n++) {
            data[n] = data[n - 1] * alpha;
        }
        
        float gainDb = normalized ? 20 * log10(this->calcGain()) : 0.;
        
        // overwrite data with computed (normalized) magnitude dB value at frequency point
        for (int n = 0; n < bins; n++) {
            data[n] = this->getMagnitudeResponse(data[n]) - gainDb;
        }
    };

private:
    float fs_ = 44100; // sample rate
	float fc_ = 1000; // corner freq
	
	int taps_ = 20;
	int kind_ = kBypass;
	
    float gain;
	
    int M_ = kMaxTaps;
    int N_ = kMaxTaps + 1; // num filter taps

	int index = 0;
	
    double coeff[kMaxTaps + 1];
    double state[kMaxTaps + 1];
};
