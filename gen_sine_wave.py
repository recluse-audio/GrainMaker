import numpy as np
import scipy.io.wavfile as wavfile
import json

def generate_sine_wave(frequency, duration, sample_rate=44100, amplitude=0.5):
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False)
    data = amplitude * np.sin(2 * np.pi * frequency * t)
    return data

def save_wav_and_json(data, sample_rate, filename_prefix):
    # Save as .wav file
    wavfile.write(f"{filename_prefix}.wav", sample_rate, (data * 32767).astype(np.int16))
    
    # Save as .json file
    json_data = data.tolist()
    with open(f"{filename_prefix}.json", 'w') as json_file:
        json.dump(json_data, json_file)

# Example usage
frequency = 440  # A4 note
duration = 2     # 2 seconds
sample_rate = 44100
amplitude = 0.5

# Generate sine wave
sine_wave = generate_sine_wave(frequency, duration, sample_rate, amplitude)

# Save both .wav and .json files
save_wav_and_json(sine_wave, sample_rate, 'sine_wave_440Hz')
