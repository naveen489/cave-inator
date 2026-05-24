#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EerieCaveDelayAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           public juce::Timer
{
public:
    EerieCaveDelayAudioProcessorEditor (EerieCaveDelayAudioProcessor&);
    ~EerieCaveDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    EerieCaveDelayAudioProcessor& audioProcessor;
    
    // Sliders
    juce::Slider caveSizeSlider;
    juce::Slider instabilitySlider;
    juce::Slider mutationSlider;
    juce::Slider densitySlider;
    juce::Slider diffusionSlider;
    juce::Slider darknessSlider;
    juce::Slider decaySlider;
    juce::Slider widthSlider;
    juce::Slider ghostSlider;
    juce::Slider mixSlider;
    
    // Labels
    juce::Label caveSizeLabel;
    juce::Label instabilityLabel;
    juce::Label mutationLabel;
    juce::Label densityLabel;
    juce::Label diffusionLabel;
    juce::Label darknessLabel;
    juce::Label decayLabel;
    juce::Label widthLabel;
    juce::Label ghostLabel;
    juce::Label mixLabel;
    
    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> caveSizeAttachment;
    std::unique_ptr<SliderAttachment> instabilityAttachment;
    std::unique_ptr<SliderAttachment> mutationAttachment;
    std::unique_ptr<SliderAttachment> densityAttachment;
    std::unique_ptr<SliderAttachment> diffusionAttachment;
    std::unique_ptr<SliderAttachment> darknessAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> ghostAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    
    // File player UI
    juce::TextButton loadButton{"Load Audio File"};
    juce::TextButton playStopButton{"Play/Pause"};
    juce::TextButton exportButton{"Export"};
    juce::Slider progressSlider{juce::Slider::LinearHorizontal, juce::Slider::NoTextBox};
    bool isDraggingSlider = false;
    
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> exportChooser;
    
    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EerieCaveDelayAudioProcessorEditor)
};
