#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SaboteurPhantomProcessor.h"

class PhantomCyanLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PhantomCyanLookAndFeel();
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider& slider) override;
};

class SaboteurPhantomEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit SaboteurPhantomEditor (SaboteurPhantomProcessor&);
    ~SaboteurPhantomEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawPhantomVisualizer (juce::Graphics& g, juce::Rectangle<int> bounds);
    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text);

    SaboteurPhantomProcessor& processorRef;
    PhantomCyanLookAndFeel phantomLookAndFeel;

    // Barre supérieure
    juce::ComboBox modeSelector;
    juce::ToggleButton soloButton  { "HARMONICS SOLO" };
    juce::ToggleButton phoneButton { "PHONE CHECK" };

    // 7 Potentiomètres rotatifs
    juce::Slider subFocusSlider, phantomDriveSlider, characterSlider, autoPocketSlider;
    juce::Slider tightnessSlider, subFloorSlider, mixSlider;

    juce::Label subFocusLabel, phantomDriveLabel, characterLabel, autoPocketLabel;
    juce::Label tightnessLabel, subFloorLabel, mixLabel;

    // Liaisons APVTS
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAttachment>  modeAttach;
    std::unique_ptr<ButtonAttachment> soloAttach, phoneAttach;
    std::unique_ptr<SliderAttachment> subFocusAttach, phantomDriveAttach, characterAttach, autoPocketAttach;
    std::unique_ptr<SliderAttachment> tightnessAttach, subFloorAttach, mixAttach;

    // Données graphiques 60 FPS
    std::vector<float> guiDrySpec;
    std::vector<float> guiHarmSpec;
    std::vector<float> guiPocketCurve;
    float guiHarmActivity = 0.0f;
    float guiPocketGainDb = 0.0f;
    double guiSampleRate  = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaboteurPhantomEditor)
};