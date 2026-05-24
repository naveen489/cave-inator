#include "PluginProcessor.h"
#include "PluginEditor.h"

EerieCaveDelayAudioProcessorEditor::EerieCaveDelayAudioProcessorEditor (EerieCaveDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 450); // Increased height for file player

    setupSlider(caveSizeSlider, caveSizeLabel, "Cave Size", caveSizeAttachment, "CAVE_SIZE");
    setupSlider(instabilitySlider, instabilityLabel, "Instability", instabilityAttachment, "INSTABILITY");
    setupSlider(mutationSlider, mutationLabel, "Mutation", mutationAttachment, "MUTATION");
    setupSlider(densitySlider, densityLabel, "Density", densityAttachment, "DENSITY");
    setupSlider(diffusionSlider, diffusionLabel, "Diffusion", diffusionAttachment, "DIFFUSION");
    setupSlider(darknessSlider, darknessLabel, "Darkness", darknessAttachment, "DARKNESS");
    setupSlider(decaySlider, decayLabel, "Decay", decayAttachment, "DECAY");
    setupSlider(widthSlider, widthLabel, "Width", widthAttachment, "WIDTH");
    setupSlider(ghostSlider, ghostLabel, "Ghost", ghostAttachment, "GHOST");
    setupSlider(mixSlider, mixLabel, "Mix", mixAttachment, "MIX");
    
    addAndMakeVisible(loadButton);
    addAndMakeVisible(playStopButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(progressSlider);
    
    // Style buttons
    juce::Colour btnBg = juce::Colour(0xff1a222c);
    juce::Colour btnText = juce::Colours::darkseagreen;
    
    loadButton.setColour(juce::TextButton::buttonColourId, btnBg);
    loadButton.setColour(juce::TextButton::textColourOffId, btnText);
    playStopButton.setColour(juce::TextButton::buttonColourId, btnBg);
    playStopButton.setColour(juce::TextButton::textColourOffId, btnText);
    exportButton.setColour(juce::TextButton::buttonColourId, btnBg);
    exportButton.setColour(juce::TextButton::textColourOffId, btnText);
    
    // Style progress slider
    progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::darkseagreen.withAlpha(0.6f));
    progressSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff151a22));
    progressSlider.setColour(juce::Slider::thumbColourId, juce::Colours::ghostwhite.withAlpha(0.8f));
    
    progressSlider.setRange(0.0, 1.0);
    progressSlider.onValueChange = [this] {
        if (isDraggingSlider && audioProcessor.hasFileLoaded()) {
            audioProcessor.setPlaybackPosition(progressSlider.getValue());
        }
    };
    progressSlider.onDragStart = [this] { isDraggingSlider = true; };
    progressSlider.onDragEnd = [this] { isDraggingSlider = false; };
    
    loadButton.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Select Audio File", juce::File{}, "*.wav;*.mp3;*.aiff;*.ogg");
        auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                audioProcessor.loadFile(file);
                playStopButton.setButtonText("Play");
            }
        });
    };
    
    playStopButton.onClick = [this] {
        if (!audioProcessor.hasFileLoaded()) return;
        if (audioProcessor.isPlaying()) {
            audioProcessor.stop();
            playStopButton.setButtonText("Play");
        } else {
            audioProcessor.play();
            playStopButton.setButtonText("Pause");
        }
    };
    
    exportButton.onClick = [this] {
        if (!audioProcessor.hasFileLoaded()) return;
        exportChooser = std::make_unique<juce::FileChooser>("Export Audio", juce::File{}, "*.wav");
        auto folderChooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
        exportChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File{}) {
                audioProcessor.exportProcessedAudio(file);
            }
        });
    };
    
    startTimerHz(30);
}

EerieCaveDelayAudioProcessorEditor::~EerieCaveDelayAudioProcessorEditor()
{
}

void EerieCaveDelayAudioProcessorEditor::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramId)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    // Custom colors for an eerie aesthetic
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::ghostwhite.withAlpha(0.7f));
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::darkseagreen.withAlpha(0.5f));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1a1a24));
    
    addAndMakeVisible(slider);
    
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::silver);
    addAndMakeVisible(label);
    
    attachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramId, slider);
}

void EerieCaveDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Eerie dark background
    juce::ColourGradient bgGrad(juce::Colour(0xff0a0c10), 0, 0, juce::Colour(0xff151a22), 0, (float)getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    g.setColour (juce::Colours::darkseagreen.withAlpha(0.6f));
    g.setFont (30.0f);
    g.drawFittedText ("cave-inator", getLocalBounds().removeFromTop(60), juce::Justification::centred, 1);
}

void EerieCaveDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Reserve top for title
    area.removeFromTop(60);
    
    // Transport bar at the bottom
    auto transportArea = area.removeFromBottom(80);
    
    // Scrub bar takes full width of transport top
    progressSlider.setBounds(transportArea.removeFromTop(20).reduced(20, 0));
    transportArea.removeFromTop(15); // spacing
    
    // Center the buttons
    auto buttonArea = transportArea.withSizeKeepingCentre(420, 30);
    loadButton.setBounds(buttonArea.removeFromLeft(140).reduced(5, 0));
    playStopButton.setBounds(buttonArea.removeFromLeft(140).reduced(5, 0));
    exportButton.setBounds(buttonArea.removeFromLeft(140).reduced(5, 0));
    
    // Knobs in the remaining middle area
    area.reduce(10, 10);
    
    int numColumns = 5;
    int numRows = 2;
    int knobWidth = area.getWidth() / numColumns;
    int knobHeight = area.getHeight() / numRows;
    
    juce::Slider* sliders[] = { &caveSizeSlider, &instabilitySlider, &mutationSlider, &densitySlider, &diffusionSlider,
                                &darknessSlider, &decaySlider, &widthSlider, &ghostSlider, &mixSlider };
                                
    juce::Label* labels[] = { &caveSizeLabel, &instabilityLabel, &mutationLabel, &densityLabel, &diffusionLabel,
                              &darknessLabel, &decayLabel, &widthLabel, &ghostLabel, &mixLabel };
                              
    for (int i = 0; i < 10; ++i)
    {
        int row = i / numColumns;
        int col = i % numColumns;
        
        auto rect = juce::Rectangle<int>(area.getX() + col * knobWidth, area.getY() + row * knobHeight, knobWidth, knobHeight);
        
        labels[i]->setBounds(rect.removeFromBottom(30));
        sliders[i]->setBounds(rect.reduced(10));
    }
}

void EerieCaveDelayAudioProcessorEditor::timerCallback()
{
    if (audioProcessor.hasFileLoaded())
    {
        double length = audioProcessor.getPlaybackLength();
        if (length > 0.0)
        {
            if (progressSlider.getMaximum() != length)
                progressSlider.setRange(0.0, length, 0.01);
                
            if (!isDraggingSlider)
                progressSlider.setValue(audioProcessor.getPlaybackPosition(), juce::dontSendNotification);
        }
        
        if (!audioProcessor.isPlaying() && playStopButton.getButtonText() == "Pause")
        {
            playStopButton.setButtonText("Play");
        }
    }
}
