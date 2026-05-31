#include "PluginProcessor.h"
#include "PluginEditor.h"

EerieCaveDelayAudioProcessor::EerieCaveDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    formatManager.registerBasicFormats();
}

EerieCaveDelayAudioProcessor::~EerieCaveDelayAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout EerieCaveDelayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("CAVE_SIZE", "Cave Size", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("INSTABILITY", "Instability", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MUTATION", "Mutation", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DENSITY", "Reflection Density", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DIFFUSION", "Diffusion", 0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DARKNESS", "Darkness", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DECAY", "Decay", 0.0f, 1.1f, 0.6f)); // Feedback can go > 1.0 for self-oscillation
    params.push_back(std::make_unique<juce::AudioParameterFloat>("WIDTH", "Width", 0.0f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GHOST", "Ghost Reflections", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("BATMANIZE", "Batmanize", 0.0f, 1.0f, 0.0f));

    return { params.begin(), params.end() };
}

const juce::String EerieCaveDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EerieCaveDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EerieCaveDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EerieCaveDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EerieCaveDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EerieCaveDelayAudioProcessor::getNumPrograms()
{
    return 1;
}

int EerieCaveDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EerieCaveDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EerieCaveDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void EerieCaveDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void EerieCaveDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    
    multiTapEngine.prepare(spec);
    batmanizeModulator.prepare(sampleRate);
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void EerieCaveDelayAudioProcessor::releaseResources()
{
    transportSource.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EerieCaveDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Allow any Mono or Stereo configurations (including 0-in, 2-out or 1-in, 2-out)
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Allow 0, 1, or 2 input channels
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()
     && layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
  #endif
}
#endif

void EerieCaveDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
        
    // If we have an audio file loaded and playing, overwrite the buffer with the file
    if (transportSource.isPlaying())
    {
        // Clear input so we only hear the file
        buffer.clear();
        
        juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
        transportSource.getNextAudioBlock(info);
    }
        
    // Read anchor values from APVTS
    float caveSize    = apvts.getRawParameterValue("CAVE_SIZE")->load();
    float instability = apvts.getRawParameterValue("INSTABILITY")->load();
    float mutation    = apvts.getRawParameterValue("MUTATION")->load();
    float density     = apvts.getRawParameterValue("DENSITY")->load();
    float diffusion   = apvts.getRawParameterValue("DIFFUSION")->load();
    float darkness    = apvts.getRawParameterValue("DARKNESS")->load();
    float decay       = apvts.getRawParameterValue("DECAY")->load();
    float width       = apvts.getRawParameterValue("WIDTH")->load();
    float ghost       = apvts.getRawParameterValue("GHOST")->load();
    float mix         = apvts.getRawParameterValue("MIX")->load();
    float batman      = apvts.getRawParameterValue("BATMANIZE")->load();
    
    // Tick Batmanize modulators and apply modulation around anchor values
    batmanizeModulator.tick(buffer.getNumSamples());
    auto& bm = batmanizeModulator;
    float modInstability = BatmanizeModulator::mod(instability, bm.instability.get(), 0.20f, batman);
    float modDensity     = BatmanizeModulator::mod(density,     bm.density.get(),     0.10f, batman);
    float modMutation    = BatmanizeModulator::mod(mutation,    bm.mutation.get(),    0.25f, batman);
    float modGhost       = BatmanizeModulator::modGhost(ghost,  bm.ghost.get(),              batman);
    float modWidth       = BatmanizeModulator::mod(width,       bm.width.get(),       0.15f, batman);
    float modDarkness    = BatmanizeModulator::mod(darkness,    bm.darkness.get(),    0.10f, batman);
    float modDecay       = BatmanizeModulator::mod(decay,       bm.decay.get(),       0.08f, batman);
    
    multiTapEngine.updateParameters(caveSize, modInstability, modMutation, modDensity, diffusion, modDarkness, modDecay, modWidth, modGhost, mix);
    multiTapEngine.setBPMSync(bpmSyncEnabled.load(), bpmValue.load());

    multiTapEngine.process(buffer);
}

bool EerieCaveDelayAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EerieCaveDelayAudioProcessor::createEditor()
{
    return new EerieCaveDelayAudioProcessorEditor (*this);
}

void EerieCaveDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EerieCaveDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EerieCaveDelayAudioProcessor();
}

// File playback implementation
void EerieCaveDelayAudioProcessor::loadFile(const juce::File& file)
{
    if (auto* reader = formatManager.createReaderFor(file))
    {
        currentLoadedFile = file;
        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
    }
}

void EerieCaveDelayAudioProcessor::play()
{
    transportSource.start();
}

void EerieCaveDelayAudioProcessor::stop()
{
    transportSource.stop();
    // transportSource.setPosition(0.0); // Removed so it acts as pause
}

bool EerieCaveDelayAudioProcessor::isPlaying() const
{
    return transportSource.isPlaying();
}

bool EerieCaveDelayAudioProcessor::hasFileLoaded() const
{
    return readerSource != nullptr;
}

double EerieCaveDelayAudioProcessor::getPlaybackPosition() const
{
    return transportSource.getCurrentPosition();
}

void EerieCaveDelayAudioProcessor::setPlaybackPosition(double pos)
{
    transportSource.setPosition(pos);
}

double EerieCaveDelayAudioProcessor::getPlaybackLength() const
{
    return transportSource.getLengthInSeconds();
}

void EerieCaveDelayAudioProcessor::setBPM(float bpm)
{
    bpmValue.store(juce::jlimit(20.0f, 300.0f, bpm));
    multiTapEngine.setBPMSync(bpmSyncEnabled.load(), bpmValue.load());
}

void EerieCaveDelayAudioProcessor::setBPMSyncEnabled(bool enabled)
{
    bpmSyncEnabled.store(enabled);
    multiTapEngine.setBPMSync(enabled, bpmValue.load());
}

void EerieCaveDelayAudioProcessor::exportProcessedAudio(const juce::File& outputFile)
{
    if (!currentLoadedFile.existsAsFile()) return;
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(currentLoadedFile));
    if (reader == nullptr) return;
    
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(new juce::FileOutputStream(outputFile), 
                                                    reader->sampleRate, 
                                                    2, 16, {}, 0));
    if (writer == nullptr) return;
    
    MultiTapEngine offlineEngine;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = reader->sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    offlineEngine.prepare(spec);
    offlineEngine.setBPMSync(bpmSyncEnabled.load(), bpmValue.load());
    
    // Separate Batmanize modulator for the offline render (starts fresh)
    BatmanizeModulator offlineBatmanize;
    offlineBatmanize.prepare(spec.sampleRate);
    float batman = apvts.getRawParameterValue("BATMANIZE")->load();
    
    float caveSize    = apvts.getRawParameterValue("CAVE_SIZE")->load();
    float instability = apvts.getRawParameterValue("INSTABILITY")->load();
    float mutation    = apvts.getRawParameterValue("MUTATION")->load();
    float density     = apvts.getRawParameterValue("DENSITY")->load();
    float diffusion   = apvts.getRawParameterValue("DIFFUSION")->load();
    float darkness    = apvts.getRawParameterValue("DARKNESS")->load();
    float decay       = apvts.getRawParameterValue("DECAY")->load();
    float width       = apvts.getRawParameterValue("WIDTH")->load();
    float ghost       = apvts.getRawParameterValue("GHOST")->load();
    float mix         = apvts.getRawParameterValue("MIX")->load();
    
    // Initial update with base (un-modulated) parameters to configure taps
    offlineEngine.updateParameters(caveSize, instability, mutation, density, diffusion, darkness, decay, width, ghost, mix);
    
    // Snap all tap gains/delays to their targets immediately so the full
    // delay effect is heard from the very first exported sample, bypassing
    // the slow smoothing ramp that is designed for live real-time use.
    offlineEngine.snapParameters();
    
    juce::AudioBuffer<float> buffer(2, 512);
    juce::int64 totalSamples = reader->lengthInSamples;
    juce::int64 tailSamples = static_cast<juce::int64>(5.0 * reader->sampleRate); // 5 sec tail
    juce::int64 currentSample = 0;
    
    while (currentSample < totalSamples + tailSamples)
    {
        int numSamples = (int)juce::jmin((juce::int64)512, totalSamples + tailSamples - currentSample);
        buffer.setSize(2, numSamples, false, false, true);
        buffer.clear();
        
        if (currentSample < totalSamples)
        {
            int samplesToRead = (int)juce::jmin((juce::int64)numSamples, totalSamples - currentSample);
            reader->read(&buffer, 0, samplesToRead, currentSample, true, true);
        }
        
        // Tick Batmanize and apply modulation around anchor values each block
        offlineBatmanize.tick(numSamples);
        auto& bm = offlineBatmanize;
        float modInstability = BatmanizeModulator::mod(instability, bm.instability.get(), 0.20f, batman);
        float modDensity     = BatmanizeModulator::mod(density,     bm.density.get(),     0.10f, batman);
        float modMutation    = BatmanizeModulator::mod(mutation,    bm.mutation.get(),    0.25f, batman);
        float modGhost       = BatmanizeModulator::modGhost(ghost,  bm.ghost.get(),              batman);
        float modWidth       = BatmanizeModulator::mod(width,       bm.width.get(),       0.15f, batman);
        float modDarkness    = BatmanizeModulator::mod(darkness,    bm.darkness.get(),    0.10f, batman);
        float modDecay       = BatmanizeModulator::mod(decay,       bm.decay.get(),       0.08f, batman);
        offlineEngine.updateParameters(caveSize, modInstability, modMutation, modDensity, diffusion, modDarkness, modDecay, modWidth, modGhost, mix);
        offlineEngine.process(buffer);
        writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
        currentSample += numSamples;
    }
}
