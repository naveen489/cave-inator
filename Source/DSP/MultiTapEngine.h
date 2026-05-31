#pragma once

#include <JuceHeader.h>
#include <vector>

// Forward declarations or inner classes for Pitch Mutator and Diffusion
struct PitchMutator
{
    PitchMutator();
    void prepare(double sampleRate);
    void setMutationAmount(float amount);
    void setInstability(float amount);
    void updateParameters();
    float processSample(float input);
    
private:
    double fs = 44100.0;
    float mutation = 0.0f;
    float instability = 0.0f;
    
    // Smooth attenuation for high-pitched shifts
    float currentMultiplier = 1.0f;
    float targetMultiplier = 1.0f;
    
    // Delay line for crossfading pitch shift
    static constexpr int maxDelaySamples = 44100;
    std::vector<float> delayBuffer;
    int writeIndex = 0;
    
    float phase = 0.0f;
    float phaseInc = 0.0f;
    float targetPitchRatio = 1.0f;
    float currentPitchRatio = 1.0f;
    
    juce::Random random;
    
    void calculateNewPitchTarget();
};

struct DiffusionFilter
{
    DiffusionFilter();
    void prepare(double sampleRate, float maxDelayMs);
    void setParameters(float delayMs, float feedback);
    float process(float input);
    
private:
    std::vector<float> buffer;
    int writeIndex = 0;
    int delaySamples = 0;
    float fb = 0.5f;
    double fs = 44100.0;
};

class MultiTapEngine
{
public:
    MultiTapEngine();
    ~MultiTapEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // Update APVTS parameters
    void updateParameters(float caveSize, float instability, float mutation,
                          float density, float diffusion, float darkness,
                          float decay, float width, float ghost, float mix);

    void process(juce::AudioBuffer<float>& buffer);
    
    // Instantly snaps all tap smoothed values to their targets.
    // Call this after updateParameters() for offline/export use
    // to avoid the slow gain/delay ramp-up that is designed for live use.
    void snapParameters();
    
    // BPM sync: when enabled, Cave Size maps to musical note subdivisions at the given BPM
    void setBPMSync(bool enabled, float bpm);

private:
    double sampleRate = 44100.0;
    bool bpmSyncEnabled = false;
    float currentBPM = 120.0f;
    
    // Smoothing parameters to update reflection behavior every ~50-300ms    
    juce::dsp::Reverb caveReverb;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> reverbBuffer;
    
    // Initialize to max so the very first updateParameters() call always configures the taps
    int samplesSinceLastUpdate = std::numeric_limits<int>::max();
    int updateIntervalSamples = 4410;
    
    juce::Random random;
    
    // Master delay line for taps
    static constexpr int maxDelayTimeMs = 5000;
    std::vector<std::vector<float>> masterDelayBuffer; // stereo
    int masterWriteIndex = 0;
    
    // Tap structures
    struct Tap
    {
        float targetDelaySamples = 0.0f;
        float currentDelaySamples = 0.0f;
        float pan = 0.5f; // 0 L, 1 R
        float targetGain = 0.0f;
        float currentGain = 0.0f;
        
        PitchMutator mutator;
    };
    
    std::vector<Tap> taps;
    
    // Global effects
    std::vector<DiffusionFilter> diffusers[2]; // Stereo diffusers
    
    // Low pass filter for "Darkness"
    juce::dsp::IIR::Filter<float> lpf[2];
    
    // Parameter values
    float currentMix = 0.5f;
    float currentFeedback = 0.0f;
    float currentDarkness = 0.0f;
    
    void recalculateCaveEnvironment();
    float getInterpolatedSample(int channel, float delayInSamples);
};
