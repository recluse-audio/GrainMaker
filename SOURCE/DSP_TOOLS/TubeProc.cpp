//	Copyright (c) 2017 Antares Audio Technologies. All rights reserved.

#include "TubeProc.h"

//-----------------------------------------------------------------------
template <typename dspfloat_t>
TubeProc<dspfloat_t>::TubeProc()
{
#if 0
	comp_threshold = 1.; // 0dB
	comp_threshold_inv = 1.; // 0dB
#else
	comp_threshold = dBToGain(kMinCompThreshDb);
	comp_threshold_inv = 1.f / comp_threshold;
#endif
    
#ifdef USE_DB_LOOKUP_TABLES
    // create dB lookup tables
    
    float dx;
    
    dx = 0; // dB
    for (int i=0; i <= 24; i++) {
        dBToGainDrive[i] = pow(10., dx / 20.);
        dx += 0.5;
    }

    dx = 0; // dB
    for (int i=0; i <= 24; i++) {
        dBToGainRecov[i] = pow(10., dx / 20.);
        dx -= 0.15;
    }

    dx = 0; // dB
    for (int i=0; i <= 36; i++) {
        dBToGainCompr[i] = pow(10., dx / 20.);
        dx -= 1.0;
    }
#endif
    setSampleRate(44100.);
}

//-----------------------------------------------------------------------
template <typename dspfloat_t>
void TubeProc<dspfloat_t>::setControl(int paramId, float param)
{
    switch (paramId)
    {
        case kBypass:
            bypass = (bool) param;
            break;
        case kSetEqPost:
            eqpost = (bool) param;
            break;
        case kInputGain:
            input_gain_coef = (float)dBToGain(param);
            break;
        case kDriveGain:
            if (param <= 12.) {
                severity = 0;
                drive_gain_control = param;
                bypass = false;
            }
            else {
                severity = 1;
                drive_gain_control = param - 12.f;
                bypass = false;
            }
            updateGains();
            break;
        case kOutputGain:
            output_gain_coef = (float)dBToGain(param);
            break;
        case kCompEnable:
            compressor = (bool)param;
            break;
        case kCompThresh:
            // range 0 (bypass) to -36 dB
#ifdef USE_DB_LOOKUP_TABLES
            if (param > kMinCompThreshDb) {
                comp_threshold = dBToGainCompr[(int)(-param)];
            }
#else
            comp_threshold = (float)dBToGain(param);
#endif
            comp_threshold_inv = 1.f / comp_threshold;
            compressor = (param == 0) ? 0 : 1;
            break;
        case kCompAttack:
            comp_attack = pow(.5f, 1.f / (sampleRate * param));
            break;
        case kCompRelease:
            comp_release = pow(.5f, 1.f / (sampleRate * param));
            break;
        case kEnableTone:
            tubetone = (bool)param;
            break;
        case kSetToneLow:
            tubeTone.setControl(TubeTone<dspfloat_t>::kLow, param);
            break;
        case kSetToneMid:
            tubeTone.setControl(TubeTone<dspfloat_t>::kMid, param);
            break;
        case kSetToneHigh:
            tubeTone.setControl(TubeTone<dspfloat_t>::kTop, param);
            break;
        case kSetSeverity:
            severity = (bool)param;
            updateGains();
            break;
        default:
            // eeeek!
            break;
    }
}

//-----------------------------------------------------------------------
template <typename dspfloat_t>
void TubeProc<dspfloat_t>::setSampleRate(float rate)
{
	sampleRate = rate;
	dcblock_alpha = exp(-2.f * kPi * kDcBlockerFreqHz / sampleRate); // ~0.998 @44.1KHz
    smooth_alpha = exp(-2.f * kPi * 50.f / sampleRate); // 50 Hz = 20 msec time constant
	comp_attack = pow(.5f, 1.f / (sampleRate * kAttackHalfTime));
	comp_release = pow(.5f, 1.f / (sampleRate * kReleaseHalfTime));
    
    tubeTone.setSampleRate(sampleRate);
}

//-----------------------------------------------------------------------
template <typename dspfloat_t>
void TubeProc<dspfloat_t>::updateGains()
{
    // called every time drive gain, OmniTube, and Severity change values
    
#ifdef USE_DB_LOOKUP_TABLES
    int tableIndex = drive_gain_control;
    if (severity) {
        tableIndex += 12;
    }
    tableIndex = tableIndex > 24 ? 24 : tableIndex;
    
    drive_gain_coef = dBToGainDrive[tableIndex];
    drive_gain_recovery = dBToGainRecov[tableIndex];
#else
    dspfloat_t recovery_discount = .15f; // %
    
    dspfloat_t drive_gain_db = drive_gain_control * .5f;	// maps 0 to 12 into 0 to 6, for two pass algorithm
    dspfloat_t drive_gain_recovery_db = -drive_gain_control * recovery_discount; // gain recovery is recovery_discount of drive gain.
    
    if (severity) {
        drive_gain_db += 6;
        drive_gain_recovery_db += -12 * recovery_discount;
    }
    
	drive_gain_coef = dBToGain(drive_gain_db);
	drive_gain_recovery = dBToGain(drive_gain_recovery_db);
#endif
}

//-----------------------------------------------------------------------
template <typename dspfloat_t>
void TubeProc<dspfloat_t>::getLevels(float& inputLevel, float& outputLevel)
{
	inputLevel = float(input_level);
	outputLevel = float(output_level);
	clear_levels.store(true);
}

//-----------------------------------------------------------------------
template <typename dspfloat_t>
void TubeProc<dspfloat_t>::getCompLevels(float& compLevel, float& compGain)
{
    if (compressor) {
        compLevel = float(comp_envelope);
        compGain = float(comp_gain / comp_threshold_inv);
    } else {
        compLevel = 0;
        compGain = 0;
    }
}

//-----------------------------------------------------------------------
//   this version is for mono
//
template <typename dspfloat_t>
dspfloat_t TubeProc<dspfloat_t>::run(dspfloat_t xin)
{
	dspfloat_t xi = xin;
    dspfloat_t xo;

    // reset meters
    if (clear_levels.exchange(false)) {
        input_level = 0.;
        output_level = 0.;
    }

    if (bypass) {
        xo = xi;
    }
    else
    {
        dspfloat_t x;

        // scale input

        x = input_gain_coef * xi;
        
        // run input meter
		
		input_level = fmax(input_level, fabs(x));

        // clamp input to 0dBFS (but why?)

		x = fmin(x,  1.f);
		x = fmax(x, -1.f);
		
        // run compressor

        if (compressor) {
            
            // run envelope follower
            
            dspfloat_t x_abs = fabs(x);

            if (x_abs > comp_envelope)
                comp_envelope = comp_attack * comp_envelope + (1.f - comp_attack) * x_abs;
            else
                comp_envelope = comp_release * comp_envelope + (1.f - comp_release) * x_abs;

            // apply upward compression
            
            comp_gain = comp_threshold_inv_f;
            if (comp_envelope > comp_threshold_f)
                comp_gain = 1.f / comp_envelope;

            x *= comp_gain;
        }
        
        // run tone stack pre-distortion
        
        if (tubetone && !eqpost) {
            x = tubeTone.runBuffered(x);
        }

        // apply distortion model (AK: should this be oversampled?)
        
        if (drive_gain_control > 0) {
        //if (1) {
            if (x > 0) {
                x = x * (drive_gain_coef_f - (drive_gain_coef_f - 1.f) * x);
                x = x * (drive_gain_coef_f - (drive_gain_coef_f - 1.f) * x);
            }
            else {
                x *= (drive_gain_coef_f * drive_gain_coef_f);
				x = fmax(x, -1.f);
            }
            
            // remove compressor gain boost
            
            if (compressor) {
                x /= comp_gain;
            }
            
			//	apply one pole + one zero DC blocker filter:
            //	y(i) = x(i) - x(i-1) + lowcut_alpha * y(i-1), x(i-1) = x(i)
            
            dcblock_state_y = x - dcblock_state_x + dcblock_alpha * dcblock_state_y;
            dcblock_state_x = x;
            
            // apply drive gain recovery
            
            x = drive_gain_recovery_f * dcblock_state_y;
        }
#if 0
        else {
            // compressor make-up gain
            x *= comp_threshold_f;
        }
#endif
        
        // run tone stack post-distortion
        
        if (tubetone && eqpost) {
            x = tubeTone.runBuffered(x);
        }
        
        x *= output_gain_coef;

        // run output meter
		
		output_level = fmax(output_level, fabs(x));
        
        // clamp output
        
		x = fmin(x,  k0dBFS);
		x = fmax(x, -k0dBFS);

        // output the sample
        
        xo = x;
        
		// run coefficient smoothing filters (TODO: run at decimated rate)
        
        drive_gain_coef_f = smooth_alpha * drive_gain_coef_f + (1.f - smooth_alpha) * drive_gain_coef;
        drive_gain_recovery_f = smooth_alpha * drive_gain_recovery_f + (1.f - smooth_alpha) * drive_gain_recovery;

		comp_threshold_f = smooth_alpha * comp_threshold_f + (1.f - smooth_alpha) * comp_threshold;
        comp_threshold_inv_f = smooth_alpha * comp_threshold_inv_f + (1.f - smooth_alpha) * comp_threshold_inv;
    }
	
    return xo;
}

template class TubeProc<float>;
template class TubeProc<double>;
