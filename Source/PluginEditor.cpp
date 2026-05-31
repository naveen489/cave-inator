#include "PluginProcessor.h"
#include "PluginEditor.h"

EerieCaveDelayAudioProcessorEditor::EerieCaveDelayAudioProcessorEditor (EerieCaveDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 540); // Extra height for Batmanize strip + BPM row

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
    
    // Batmanize: special gold styling to mark it as a meta-parameter
    batmanizeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    batmanizeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    batmanizeSlider.setColour(juce::Slider::thumbColourId, juce::Colours::goldenrod.withAlpha(0.9f));
    batmanizeSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8b6914).withAlpha(0.7f));
    batmanizeSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1a1a24));
    addAndMakeVisible(batmanizeSlider);
    batmanizeLabel.setText("Batmanize", juce::dontSendNotification);
    batmanizeLabel.setJustificationType(juce::Justification::centred);
    batmanizeLabel.setColour(juce::Label::textColourId, juce::Colours::goldenrod);
    addAndMakeVisible(batmanizeLabel);
    batmanizeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "BATMANIZE", batmanizeSlider);
    
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
    
    // ---- BPM Sync UI ----
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(bpmEditor);
    addAndMakeVisible(syncButton);
    
    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::darkseagreen);
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    
    bpmEditor.setInputRestrictions(3, "0123456789");
    bpmEditor.setText(juce::String((int)audioProcessor.getBPM()), false);
    bpmEditor.setJustification(juce::Justification::centred);
    bpmEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a222c));
    bpmEditor.setColour(juce::TextEditor::textColourId, juce::Colours::ghostwhite);
    bpmEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::darkseagreen.withAlpha(0.4f));
    bpmEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::darkseagreen);
    bpmEditor.onReturnKey  = [this] {
        auto val = bpmEditor.getText().getFloatValue();
        if (val >= 20.0f && val <= 300.0f)
            audioProcessor.setBPM(val);
    };
    bpmEditor.onFocusLost = [this] {
        auto val = bpmEditor.getText().getFloatValue();
        if (val >= 20.0f && val <= 300.0f)
            audioProcessor.setBPM(val);
        else
            bpmEditor.setText(juce::String((int)audioProcessor.getBPM()), false);
    };
    
    syncButton.setClickingTogglesState(true);
    syncButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a222c));
    syncButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkseagreen.withAlpha(0.5f));
    syncButton.setColour(juce::TextButton::textColourOffId, juce::Colours::darkseagreen);
    syncButton.setColour(juce::TextButton::textColourOnId, juce::Colours::ghostwhite);
    syncButton.onClick = [this] {
        audioProcessor.setBPMSyncEnabled(syncButton.getToggleState());
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

    g.setColour(juce::Colours::darkseagreen.withAlpha(0.6f));
    g.setFont(30.0f);
    g.drawFittedText("cave-inator", getLocalBounds().removeFromTop(60), juce::Justification::centred, 1);
    
    // Batmanize strip: subtle gold-tinted background
    auto batStrip = getLocalBounds().removeFromTop(60 + 90).removeFromBottom(90);
    g.setColour(juce::Colour(0xff1a1508).withAlpha(0.6f));
    g.fillRect(batStrip);
    g.setColour(juce::Colours::goldenrod.withAlpha(0.15f));
    g.drawRect(batStrip, 1);
    
    // Tooltip text inside the Batmanize strip
    g.setColour(juce::Colours::goldenrod.withAlpha(0.35f));
    g.setFont(11.0f);
    auto tooltipArea = batStrip.removeFromRight(batStrip.getWidth() - 120);
    g.drawFittedText("Makes the cave increasingly alive, unstable,\nand suspiciously bat-filled.",
                     tooltipArea.reduced(10, 0), juce::Justification::centredLeft, 2);
}

void EerieCaveDelayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Reserve top for title
    area.removeFromTop(60);
    
    // Batmanize strip (90px) — knob on left, tooltip fills the rest
    auto batStrip = area.removeFromTop(90);
    auto batKnobArea = batStrip.removeFromLeft(120);
    batmanizeLabel.setBounds(batKnobArea.removeFromBottom(24));
    batmanizeSlider.setBounds(batKnobArea.reduced(8));
    
    // BPM sync row
    auto bpmRow = area.removeFromTop(35);
    bpmRow.removeFromLeft(10);
    bpmLabel.setBounds(bpmRow.removeFromLeft(40));
    bpmEditor.setBounds(bpmRow.removeFromLeft(55).reduced(0, 6));
    bpmRow.removeFromLeft(8);
    syncButton.setBounds(bpmRow.removeFromLeft(70).reduced(0, 6));
    
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
