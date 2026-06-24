// TODO: Replace this stub with your WAV processor implementation.
//
// Interface contract
//   argv[1]  path to the WAV file to analyse
//   argv[2]  (optional) path to a baselines JSON file:
//            { "rms_db": -20.0 }  — the reference RMS level in dBFS.
//            baseline_delta_db = current rms_db − reference; pass if |delta| ≤ 6 dB.
//
//   stdout   one JSON object (single line), exit 0 on success
//   stderr   human-readable error message,  exit 1 on failure
//
// Required JSON output fields:
//   {
//     "rms_db":            -20.5,   // RMS level in dBFS
//     "peak_frequency_hz": 40000.0, // dominant frequency in Hz
//     "baseline_delta_db": 2.1,     // deviation from baseline RMS (0 if no baseline)
//     "passed_baseline":   true     // whether reading is within acceptable range
//   }


//include dr_wav single-header library
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

//include kissfft library
#include "kissfft/kiss_fft.h"
#include "kissfft/kiss_fftr.h"


#include <algorithm>
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iomanip>
#include <limits>
#include <iterator>


//created struct for reading audio data
struct AudioData {
    unsigned int channels = 0;
    unsigned int sample_rate = 0;

    //for every frame there are mono samples, type float
    std::vector<float> samples;
};


/* Function with input checking if WAV file exists, 
reads the audio samples using dr_wav library, 
turns mono audio into a mono vector, type float*/

static AudioData read_wav_as_mono(const std::string& wav_path) {
    
    // check if the .wav file opens, and has the correct path from worker
    std::ifstream file_check(wav_path, std::ios::binary | std::ios::ate);

    if (!file_check.is_open()) {
        throw std::runtime_error(
            "Cannot open input path as a file: " + wav_path
        );
    }
    
    // find file size and check if the file is empty
    const std::streamsize file_size = file_check.tellg();
    file_check.close();

    if (file_size <= 0) {
        throw std::runtime_error(
            "Input WAV file is empty: " + wav_path
        );
    }

    //create dr_wav object 
    drwav wav{};

    //open .wav file
    if (!drwav_init_file(&wav, wav_path.c_str(), nullptr)) {
        throw std::runtime_error(
            "dr_wav could not open or parse this WAV file: " + wav_path
        );
    }

    try {
       //reads .wav file, prints errors if channels,or sample rate, or PCM frames exist
        if (wav.channels == 0) {
            throw std::runtime_error("WAV header reports zero channels");
        }

        if (wav.sampleRate == 0) {
            throw std::runtime_error("WAV header reports zero sample rate");
        }

        if (wav.totalPCMFrameCount == 0) {
            throw std::runtime_error("WAV file contains zero PCM frames");
        }
       
        //storres data from .wav file header in our struct
        AudioData audio;
        audio.channels = wav.channels;
        audio.sample_rate = wav.sampleRate;

        
        // reserves space for mono samples, for each frame
        
        audio.samples.reserve(
            static_cast<size_t>(wav.totalPCMFrameCount)
        );

       //reads frames per block
        constexpr drwav_uint64 FRAMES_PER_BLOCK = 4096;

        
        std::vector<float> interleaved_buffer(
            static_cast<size_t>(FRAMES_PER_BLOCK) * wav.channels
        );
        
        //reserve how many frames are reserved for reading
        drwav_uint64 remaining_frames = wav.totalPCMFrameCount;
       
        //depending on frames per block, it demands the remainder
        while (remaining_frames > 0) {
            const drwav_uint64 frames_requested = std::min(
                FRAMES_PER_BLOCK,
                remaining_frames
            );

            //reads requested pcm frames, and turns sample in type float
            const drwav_uint64 frames_read =
                drwav_read_pcm_frames_f32(
                    &wav,
                    frames_requested,
                    interleaved_buffer.data()
                );
            
           //checks if PCM files are successfully decoded
            if (frames_read == 0) {
                throw std::runtime_error(
                    "dr_wav stopped before all PCM frames were decoded"
                );
            }

           //transformed interleaved samples in mono samples
            for (drwav_uint64 frame = 0; frame < frames_read; ++frame) {
                float sum = 0.0f;

                
                for (unsigned int channel = 0;
                    //pass through all channels
                     channel < wav.channels;
                     ++channel) {
                    
                    //finding the correct index in interleaved buffer
                    const size_t index =
                        static_cast<size_t>(frame) * wav.channels
                        + channel;
                    // sum of all channels
                    sum += interleaved_buffer[index];
                }

                //calculate mono samples mean
                const float mono_sample =
                    sum / static_cast<float>(wav.channels);

                audio.samples.push_back(mono_sample);
            }

            remaining_frames -= frames_read;
        }

        
        //closes .wav file
        drwav_uninit(&wav);

        return audio;
    }
    catch (...) {
       //closes wav due to errors
        drwav_uninit(&wav);
        throw;
    }
}

/* Calculate Root Mean Square level for audio signals, 
using mono .wav samples as input, and returns RMS calculation*/

double calculate_rmsdb(const std::vector<float>& samples) {
    //checks if there are samples   
    if (samples.empty()) {
        throw std::runtime_error("Unable to calculate RMS: WAV has no samples");
    }
    
    //variable storing sum of the square samples
    double square_sum = 0.0;
    
    //calculate the sum of square audio mono samples
    for (float sample : samples ) {
        square_sum += static_cast<double>(sample) * sample;

    }

    //calculate square mean of mono samples
    const double square_mean = square_sum / static_cast<double>(samples.size());

    //calculate RMS
    const double rms = std::sqrt(square_mean);

    //for low rms, we return a default answer

    if (rms < 1e-12) {
        return -120.0;
    }
   
    //return rms calculation in DBs
    return 20.0 * std::log10(rms);

}

/*
Function using previously extracted mono samples, in order to calculate
peak frequency using FFT algorithm
*/

double calculate_peak_freq(const std::vector<float>& samples, unsigned int sample_rate) {

    //max sample size for analysis
        const size_t maxsize_fft = 65536;
    
        const size_t use_samples = std::min(samples.size(), maxsize_fft);
    
        //Finds the number of FFT samples, based on power of 2
        size_t nfft = 1;

        while (nfft * 2 <= use_samples) {
            nfft *=2;
        }

        //create input,output vectors for fft
        std::vector<kiss_fft_scalar> input(nfft);
        std::vector<kiss_fft_cpx> output(nfft / 2 + 1); //fft is for complex numbers

        //copy nfft WAV samples
        for (size_t i = 0; i < nfft; ++i) {

            input[i] = samples[i];
        }


        //prepare KissFFT
        kiss_fftr_cfg cfg = kiss_fftr_alloc(
            static_cast<int>(nfft), //number of samples
            0, //forward FFT
            nullptr, //internal library settings
            nullptr
        );

        //error if FFT can't be called, or allocate memory
        if (cfg == nullptr) {
            throw std::runtime_error("Unable to create KissFFT");
        }
 
        // basic FFT implementation
        kiss_fftr(cfg, input.data(), output.data());

        //initialize peak frequency bin, and max power
        size_t peak_bin = 1;
        double max_pow = 0.0;

        //input FFT frequency bins
        for (size_t bin = 1; bin < output.size(); ++bin) {
            const double real = output[bin].r;
            const double imaginary = output[bin].i;

            //calculate frequency power
            const double power = real * real + imaginary * imaginary;

            //if current bin is bigger than previous, then current bin is peak frequency
            if (power > max_pow) {

                max_pow = power;
                peak_bin = bin;
            }

           
        
        }

        //free KissFFT allocated memory
        kiss_fft_free(cfg);
       
        //Turn fft bin in Hz
        return static_cast<double>(peak_bin) * static_cast<double>(sample_rate) / static_cast<double>(nfft);
}

/*Function opening baselines.json file, in order to find rms_db, 
and return it as double */

double read_baseline_rms(const std::string& path) {

    //opens file from path to read it
    std::ifstream file(path);

    //outputs error if file does not open
    if (!file) {
       throw std::runtime_error("Unable to open baseline file: " + path);

    }

    //reds file and saves it in json string
    std::string json(
        (std::istreambuf_iterator<char>(file)),
         std::istreambuf_iterator<char>()
    );

    //uses json find to get rms_db
    const size_t key = json.find("\"rms_db\"");
    const size_t colon = json.find(':', key);

    //prints errors if json isn't in the expected form
    if (key == std::string::npos || colon == std::string::npos) {
        throw std::runtime_error("Invalid baselines JSON");
    }

    //returns value of rms_db, located after :
    return std::stod(json.substr(colon + 1));
}



int main(int argc, char* argv[]) {
    try {

        //checks if correct arguments are given
        if (argc < 2) {
            std::cerr
                << "Usage: wav_processor <wav_file> [baselines_file]\n";
            return 1;
        }
        
        //saves .wav path from python worker 
        const std::string wav_path = argv[1];

        //calls function reading .wav files and extracting mono samples
        AudioData audio = read_wav_as_mono(wav_path);

        //calls function to calculate rms
        const double rms_db = calculate_rmsdb(audio.samples);

        //calls function to calculate peak frequency with fft
        const double peak_frequency_hz = calculate_peak_freq (audio.samples, audio.sample_rate);

        //initialize values, when baseline.json doesnt exist
        double baseline_delta_db = 0.0;
        bool passed_baseline = true;

        //if clause for baseline path  given
        if (argc == 3) {

            //stores JSON path
            const std::string baseline_path = argv[2];

            const double baseline_rms_db = read_baseline_rms(argv[2]);
            
            //calculates baseline delta
            baseline_delta_db = rms_db - baseline_rms_db;

            //absolute value for passed baseline
            passed_baseline = std::abs(baseline_delta_db) <= 6.0;

        }

        std::cout
            << std::fixed << std::setprecision(2)
            << "{"
            << "\"rms_db\":" << rms_db << ","
            << "\"peak_frequency_hz\":" << peak_frequency_hz << ","
            << "\"baseline_delta_db\":" << baseline_delta_db << ","
            << "\"passed_baseline\":"  << (passed_baseline ? "true" : "false") //checks if delta is in acceptable range
            << "}"
            << std::endl;

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}