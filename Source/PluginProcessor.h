#pragma once

#include <JuceHeader.h>
#include "DSP/MultiTapEngine.h"

class EerieCaveDelayAudioProcessor : public juce::AudioProcessor
{
public:
    EerieCaveDelayAudioProcessor();
    ~EerieCaveDelayAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // File playback
    void loadFile(const juce::File& file);
    void play();
    void stop();
    bool isPlaying() const;
    bool hasFileLoaded() const;
    
    double getPlaybackPosition() const;
    void setPlaybackPosition(double pos);
    double getPlaybackLength() const;
    
    void exportProcessedAudio(const juce::File& outputFile);

private:
    juce::File currentLoadedFile;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    
    MultiTapEngine multiTapEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EerieCaveDelayAudioProcessor)
};
