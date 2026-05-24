#include "MultiTapEngine.h"

// --- PitchMutator Implementation ---

PitchMutator::PitchMutator()
{
    delayBuffer.resize(maxDelaySamples, 0.0f);
}

void PitchMutator::prepare(double sampleRate)
{
    fs = sampleRate;
    writeIndex = 0;
    phase = 0.0f;
    std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
}

void PitchMutator::setMutationAmount(float amount)
{
    mutation = amount;
}

void PitchMutator::setInstability(float amount)
{
    instability = amount;
}

void PitchMutator::updateParameters()
{
    // Chance to mutate based on instability
    if (random.nextFloat() < (instability * 0.1f + 0.01f)) 
    {
        calculateNewPitchTarget();
    }
}

void PitchMutator::calculateNewPitchTarget()
{
    float r = random.nextFloat();
    float targetRatio = 1.0f;
    targetMultiplier = 1.0f;
    
    if (r < mutation)
    {
        float mutationType = random.nextFloat();
        
        if (mutationType < 0.4f) {
            // Bias downwards: sub-octave (0.5) or deep fifth (0.666)
            targetRatio = random.nextBool() ? 0.5f : 0.666667f;
        } else if (mutationType < 0.7f) {
            // Dissonant interval
            targetRatio = random.nextBool() ? 0.7071f : 1.05946f;
        } else if (mutationType < 0.9f) {
            // Microtonal offset (random slight shift)
            targetRatio = 1.0f + (random.nextFloat() * 0.1f - 0.05f);
        } else {
            // Octave up or Perfect Fifth Up (high pitched)
            targetRatio = random.nextBool() ? 2.0f : 1.5f;
            // Aggressively attenuate high-pitched squeaks
            targetMultiplier = 0.15f; 
        }
        
        // Attenuate slightly for any upward shift
        if (targetRatio > 1.05f && targetMultiplier == 1.0f) {
            targetMultiplier = 0.4f;
        }
    }
    
    targetPitchRatio = targetRatio;
}

float PitchMutator::processSample(float input)
{
    // Smooth the pitch ratio
    currentPitchRatio = currentPitchRatio * 0.999f + targetPitchRatio * 0.001f;
    
    // Store input in delay buffer
    delayBuffer[writeIndex] = input;
    
    // Simple window length for granular pitch shifting
    float windowLenSamples = (fs * 0.05f); // 50ms window
    
    // Advance phase
    float pitchShift = 1.0f - currentPitchRatio;
    phase += pitchShift;
    
    // Wrap phase
    if (phase > windowLenSamples) phase -= windowLenSamples;
    if (phase < 0.0f) phase += windowLenSamples;
    
    // Read from two points, half window apart for crossfading
    float phase2 = phase + (windowLenSamples * 0.5f);
    if (phase2 > windowLenSamples) phase2 -= windowLenSamples;
    
    // Get delays
    int readIdx1 = writeIndex - static_cast<int>(phase);
    if (readIdx1 < 0) readIdx1 += maxDelaySamples;
    
    int readIdx2 = writeIndex - static_cast<int>(phase2);
    if (readIdx2 < 0) readIdx2 += maxDelaySamples;
    
    // Hann window
    float w1 = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (phase / windowLenSamples)));
    float w2 = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (phase2 / windowLenSamples)));
    
    float out = (delayBuffer[readIdx1] * w1) + (delayBuffer[readIdx2] * w2);
    
    writeIndex++;
    if (writeIndex >= maxDelaySamples) writeIndex = 0;
    
    // Smooth the multiplier
    currentMultiplier += (targetMultiplier - currentMultiplier) * 0.005f;
    
    return out * currentMultiplier;
}

// --- DiffusionFilter Implementation ---

DiffusionFilter::DiffusionFilter() {}

void DiffusionFilter::prepare(double sampleRate, float maxDelayMs)
{
    fs = sampleRate;
    int maxSamples = static_cast<int>(fs * (maxDelayMs / 1000.0f)) + 10;
    buffer.resize(maxSamples, 0.0f);
    writeIndex = 0;
}

void DiffusionFilter::setParameters(float delayMs, float feedback)
{
    delaySamples = static_cast<int>(fs * (delayMs / 1000.0f));
    if (delaySamples >= buffer.size()) delaySamples = buffer.size() - 1;
    if (delaySamples < 1) delaySamples = 1;
    fb = feedback;
}

float DiffusionFilter::process(float input)
{
    int readIndex = writeIndex - delaySamples;
    if (readIndex < 0) readIndex += buffer.size();
    
    float delayed = buffer[readIndex];
    float output = -fb * input + delayed;
    float nextBuffer = input + fb * delayed;
    
    buffer[writeIndex] = nextBuffer;
    writeIndex++;
    if (writeIndex >= buffer.size()) writeIndex = 0;
    
    return output;
}

// --- MultiTapEngine Implementation ---

MultiTapEngine::MultiTapEngine()
{
    masterDelayBuffer.resize(2); // L, R
}

void MultiTapEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    
    int maxSamples = static_cast<int>(sampleRate * (maxDelayTimeMs / 1000.0f));
    masterDelayBuffer[0].resize(maxSamples, 0.0f);
    masterDelayBuffer[1].resize(maxSamples, 0.0f);
    masterWriteIndex = 0;
    
    wetBuffer.setSize(2, spec.maximumBlockSize);
    reverbBuffer.setSize(2, spec.maximumBlockSize);
    caveReverb.prepare(spec);
    
    // Initialize filters
    for (int i = 0; i < 2; ++i)
    {
        lpf[i].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 20000.0f);
        lpf[i].reset();
        
        diffusers[i].resize(4); // 4 stages of diffusion
        diffusers[i][0].prepare(sampleRate, 50.0f);
        diffusers[i][1].prepare(sampleRate, 50.0f);
        diffusers[i][2].prepare(sampleRate, 50.0f);
        diffusers[i][3].prepare(sampleRate, 50.0f);
    }
    
    taps.resize(8); // Max 8 taps
    for (auto& tap : taps) {
        tap.mutator.prepare(sampleRate);
    }
    
    reset();
}

void MultiTapEngine::reset()
{
    std::fill(masterDelayBuffer[0].begin(), masterDelayBuffer[0].end(), 0.0f);
    std::fill(masterDelayBuffer[1].begin(), masterDelayBuffer[1].end(), 0.0f);
    masterWriteIndex = 0;
}

void MultiTapEngine::updateParameters(float caveSize, float instability, float mutation,
                                      float density, float diffusion, float darkness,
                                      float decay, float width, float ghost, float mix)
{
    currentMix = mix;
    currentFeedback = decay;
    
    // Map darkness to LPF cutoff (inverse)
    float cutoff = juce::jmap(darkness, 0.0f, 1.0f, 20000.0f, 500.0f);
    if (std::abs(currentDarkness - cutoff) > 10.0f) {
        for (int i = 0; i < 2; ++i) {
            *lpf[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff);
        }
        currentDarkness = cutoff;
    }
    
    // Set diffusion parameters
    for (int i = 0; i < 2; ++i) {
        diffusers[i][0].setParameters(3.4f + i*0.1f, diffusion * 0.6f);
        diffusers[i][1].setParameters(5.7f + i*0.1f, diffusion * 0.6f);
        diffusers[i][2].setParameters(9.1f + i*0.1f, diffusion * 0.6f);
        diffusers[i][3].setParameters(15.2f + i*0.1f, diffusion * 0.6f);
    }
    
    // Reverb parameters
    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = juce::jmap(caveSize, 0.0f, 1.0f, 0.5f, 1.0f); // Massive
    reverbParams.damping = juce::jmap(darkness, 0.0f, 1.0f, 0.2f, 0.9f); // Dark
    reverbParams.wetLevel = 1.0f; // 100% wet for the reverb module (we mix later)
    reverbParams.dryLevel = 0.0f; // We already have the tap audio in the wetBuffer
    reverbParams.width = juce::jmap(width, 0.0f, 1.0f, 0.0f, 1.0f);
    reverbParams.freezeMode = 0.0f;
    caveReverb.setParameters(reverbParams);
    
    // Check if we should recalculate the cave environment based on instability
    updateIntervalSamples = static_cast<int>(sampleRate * juce::jmap(instability, 0.0f, 1.0f, 0.3f, 0.05f));
    
    if (samplesSinceLastUpdate >= updateIntervalSamples)
    {
        samplesSinceLastUpdate = 0;
        
        // Active taps based on density (0.0 to 1.0 maps to 1 to 8 taps)
        int numActiveTaps = static_cast<int>(juce::jmap(density, 0.0f, 1.0f, 1.0f, 8.0f));
        
        for (int i = 0; i < taps.size(); ++i)
        {
            if (i < numActiveTaps)
            {
                // Active tap
                taps[i].targetGain = 1.0f / (float)numActiveTaps;
                
                // Jitter target delay based on cave size and instability
                float baseDelay = juce::jmap(caveSize, 0.0f, 1.0f, 100.0f, 2000.0f) * ((float)(i+1)/(float)numActiveTaps);
                float jitter = (random.nextFloat() * 2.0f - 1.0f) * instability * baseDelay * 0.2f;
                float finalDelayMs = juce::jlimit(10.0f, (float)maxDelayTimeMs - 100.0f, baseDelay + jitter);
                taps[i].targetDelaySamples = (finalDelayMs / 1000.0f) * sampleRate;
                
                // Pan based on width
                float p = 0.5f;
                if (width > 0.01f) {
                    p = 0.5f + (random.nextFloat() * 2.0f - 1.0f) * width * 0.5f;
                }
                taps[i].pan = p;
                
                // Occasionally spawn a "ghost reflection"
                if (ghost > 0.0f && random.nextFloat() < (ghost * 0.1f)) {
                    taps[i].targetDelaySamples *= 1.5f; // Push it out further
                    taps[i].pan = random.nextFloat();
                }
            }
            else
            {
                // Inactive tap
                taps[i].targetGain = 0.0f;
            }
            
            // Mutate
            taps[i].mutator.setMutationAmount(mutation);
            taps[i].mutator.setInstability(instability);
            taps[i].mutator.updateParameters();
        }
    }
}

float MultiTapEngine::getInterpolatedSample(int channel, float delayInSamples)
{
    float readIdx = (float)masterWriteIndex - delayInSamples;
    while (readIdx < 0.0f) readIdx += masterDelayBuffer[channel].size();
    
    int index1 = (int)readIdx;
    int index2 = index1 + 1;
    if (index2 >= masterDelayBuffer[channel].size()) index2 -= masterDelayBuffer[channel].size();
    
    float frac = readIdx - (float)index1;
    
    return masterDelayBuffer[channel][index1] * (1.0f - frac) + masterDelayBuffer[channel][index2] * frac;
}

void MultiTapEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Ensure our internal buffers are the right size
    wetBuffer.setSize(2, numSamples, false, false, true);
    wetBuffer.clear();
    
    reverbBuffer.setSize(2, numSamples, false, false, true);
    reverbBuffer.clear();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        samplesSinceLastUpdate++;
        
        float inputL = buffer.getReadPointer(0)[sample];
        float inputR = numChannels > 1 ? buffer.getReadPointer(1)[sample] : inputL;
        
        float wetL = 0.0f;
        float wetR = 0.0f;
        
        // Sum taps
        for (auto& tap : taps)
        {
            // Smooth parameters
            tap.currentDelaySamples += (tap.targetDelaySamples - tap.currentDelaySamples) * 0.001f;
            tap.currentGain += (tap.targetGain - tap.currentGain) * 0.001f;
            
            if (tap.currentGain > 0.0001f)
            {
                float rawTapL = getInterpolatedSample(0, tap.currentDelaySamples);
                float rawTapR = getInterpolatedSample(1, tap.currentDelaySamples);
                
                // Mix to mono for the mutator, creating a point source reflection
                float rawTapMono = (rawTapL + rawTapR) * 0.5f;
                
                // Process Mutator
                float mutatedMono = tap.mutator.processSample(rawTapMono);
                
                // Pan
                float gainL = tap.currentGain * std::cos(tap.pan * juce::MathConstants<float>::halfPi);
                float gainR = tap.currentGain * std::sin(tap.pan * juce::MathConstants<float>::halfPi);
                
                wetL += mutatedMono * gainL;
                wetR += mutatedMono * gainR;
            }
            else
            {
                // Advance mutator phase even when muted to keep it aligned
                tap.mutator.processSample(0.0f);
            }
        }
        
        // Process global effects on the summed wet signal
        
        // Diffusion
        for (int i = 0; i < 4; ++i) {
            wetL = diffusers[0][i].process(wetL);
            wetR = diffusers[1][i].process(wetR);
        }
        
        // LPF
        wetL = lpf[0].processSample(wetL);
        wetR = lpf[1].processSample(wetR);
        
        // Write to master delay line with feedback
        float fbL = wetL * currentFeedback;
        float fbR = wetR * currentFeedback;
        
        masterDelayBuffer[0][masterWriteIndex] = inputL + fbL;
        masterDelayBuffer[1][masterWriteIndex] = inputR + fbR;
        
        masterWriteIndex++;
        if (masterWriteIndex >= masterDelayBuffer[0].size()) masterWriteIndex = 0;
        
        // Output Mix (write wet signal into our wetBuffer)
        wetBuffer.setSample(0, sample, wetL);
        wetBuffer.setSample(1, sample, wetR);
        
        // Feed the echoes AND the original dry input into the reverb buffer
        reverbBuffer.setSample(0, sample, wetL + inputL);
        reverbBuffer.setSample(1, sample, wetR + inputR);
    }
    
    // Process the accumulated reverb input through the massive cave reverb.
    // Since dryLevel = 0.0f, the reverbBuffer will be replaced with ONLY the reverb tail!
    juce::dsp::AudioBlock<float> block(reverbBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    caveReverb.process(context);
    
    // Final Mix with input buffer
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float inputL = buffer.getReadPointer(0)[sample];
        float inputR = numChannels > 1 ? buffer.getReadPointer(1)[sample] : inputL;
        
        float echoesL = wetBuffer.getSample(0, sample);
        float echoesR = wetBuffer.getSample(1, sample);
        
        float reverbTailL = reverbBuffer.getSample(0, sample);
        float reverbTailR = reverbBuffer.getSample(1, sample);
        
        // Total wet signal is the distinct echoes PLUS the massive reverb tail
        float finalWetL = echoesL + reverbTailL;
        float finalWetR = echoesR + reverbTailR;
        
        if (numChannels > 0) buffer.getWritePointer(0)[sample] = inputL * (1.0f - currentMix) + finalWetL * currentMix;
        if (numChannels > 1) buffer.getWritePointer(1)[sample] = inputR * (1.0f - currentMix) + finalWetR * currentMix;
    }
}
