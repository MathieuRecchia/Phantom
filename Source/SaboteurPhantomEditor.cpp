#include "SaboteurPhantomEditor.h"

PhantomCyanLookAndFeel::PhantomCyanLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,              juce::Colour (0xffb8f2ff));
    setColour (juce::Slider::textBoxBackgroundColourId,        juce::Colour (0xff0e161c));
    setColour (juce::Slider::textBoxOutlineColourId,           juce::Colour (0xff1c3342));
    setColour (juce::ComboBox::backgroundColourId,             juce::Colour (0xff111d26));
    setColour (juce::ComboBox::textColourId,                   juce::Colour (0xff00e5ff));
    setColour (juce::ComboBox::outlineColourId,                juce::Colour (0xff00b8d4));
    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff0d161d));
    setColour (juce::PopupMenu::textColourId,                  juce::Colour (0xffb8f2ff));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0097a7));
}

void PhantomCyanLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float sliderPos, const float rotaryStartAngle,
                                               const float rotaryEndAngle, juce::Slider&)
{
    const float radius  = static_cast<float> (juce::jmin (width / 2, height / 2)) - 6.0f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width)  * 0.5f;
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Corps du bouton (Obsidienne bleutée)
    g.setColour (juce::Colour (0xff101820));
    g.fillEllipse (rx, ry, rw, rw);

    g.setColour (juce::Colour (0xff1f3647));
    g.drawEllipse (rx, ry, rw, rw, 1.5f);

    // Piste de fond de l'arc
    juce::Path bgArc;
    bgArc.addCentredArc (centreX, centreY, radius + 3.0f, radius + 3.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff172836));
    g.strokePath (bgArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Arc actif Cyan Phantom
    juce::Path valueArc;
    valueArc.addCentredArc (centreX, centreY, radius + 3.0f, radius + 3.0f, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (juce::Colour (0xff00e5ff));
    g.strokePath (valueArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Aiguille indicatrice
    juce::Path pointer;
    const float pointerLength = radius * 0.68f;
    pointer.addRoundedRectangle (-1.5f, -radius + 3.0f, 3.0f, pointerLength, 1.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

    g.setColour (juce::Colour (0xff80f0ff));
    g.fillPath (pointer);
}

SaboteurPhantomEditor::SaboteurPhantomEditor (SaboteurPhantomProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&phantomLookAndFeel);

    // Sélecteur des 3 modes Rock
    modeSelector.addItemList ({ "Punch (Kick)", "Growl (Bass)", "Full Bus (Drums+Bass)" }, 1);
    addAndMakeVisible (modeSelector);
    modeAttach = std::make_unique<ComboAttachment> (processorRef.apvts, "mode", modeSelector);

    // Boutons d'écoute Harmonics Solo & Phone Check
    soloButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff80f0ff));
    soloButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (soloButton);
    soloAttach = std::make_unique<ButtonAttachment> (processorRef.apvts, "harmonics_solo", soloButton);

    phoneButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffffab40));
    phoneButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff6d00));
    addAndMakeVisible (phoneButton);
    phoneAttach = std::make_unique<ButtonAttachment> (processorRef.apvts, "phone_check", phoneButton);

    // Configuration des 7 potentiomètres
    setupKnob (subFocusSlider,     subFocusLabel,     "SUB FOCUS");
    setupKnob (phantomDriveSlider, phantomDriveLabel, "PHANTOM DRIVE");
    setupKnob (characterSlider,    characterLabel,    "CHARACTER");
    setupKnob (autoPocketSlider,   autoPocketLabel,   "AUTO-POCKET");
    setupKnob (tightnessSlider,    tightnessLabel,    "TIGHTNESS");
    setupKnob (subFloorSlider,     subFloorLabel,     "SUB FLOOR");
    setupKnob (mixSlider,          mixLabel,          "MIX");

    subFocusAttach     = std::make_unique<SliderAttachment> (processorRef.apvts, "sub_focus",     subFocusSlider);
    phantomDriveAttach = std::make_unique<SliderAttachment> (processorRef.apvts, "phantom_drive", phantomDriveSlider);
    characterAttach    = std::make_unique<SliderAttachment> (processorRef.apvts, "character",     characterSlider);
    autoPocketAttach   = std::make_unique<SliderAttachment> (processorRef.apvts, "auto_pocket",   autoPocketSlider);
    tightnessAttach    = std::make_unique<SliderAttachment> (processorRef.apvts, "tightness",     tightnessSlider);
    subFloorAttach     = std::make_unique<SliderAttachment> (processorRef.apvts, "sub_floor",     subFloorSlider);
    mixAttach          = std::make_unique<SliderAttachment> (processorRef.apvts, "mix",           mixSlider);

    setSize (860, 520);
    startTimerHz (60);
}

SaboteurPhantomEditor::~SaboteurPhantomEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SaboteurPhantomEditor::setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xff4dd0e1));
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void SaboteurPhantomEditor::timerCallback()
{
    processorRef.getVisualSnapshot (guiDrySpec, guiHarmSpec, guiPocketCurve,
                                    guiHarmActivity, guiPocketGainDb, guiSampleRate);
    repaint();
}

void SaboteurPhantomEditor::paint (juce::Graphics& g)
{
    // Dégradé de fond Saboteur Phantom (Obsidienne -> Bleu Nuit profond)
    juce::ColourGradient bgGrad (juce::Colour (0xff0b1015), 0.0f, 0.0f,
                                 juce::Colour (0xff121d26), 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // Bandeau supérieur
    auto headerArea = getLocalBounds().removeFromTop (52);
    g.setColour (juce::Colour (0xff0e171f));
    g.fillRect (headerArea);
    g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.45f));
    g.drawHorizontalLine (51, 0.0f, static_cast<float> (getWidth()));

    // Titre SABOTEUR // PHANTOM
    g.setColour (juce::Colour (0xff00e5ff));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("SABOTEUR", 20, 10, 140, 32, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffb8f2ff));
    g.setFont (juce::FontOptions (20.0f, juce::Font::plain));
    g.drawText ("// PHANTOM", 142, 10, 160, 32, juce::Justification::centredLeft);

    // Zone centrale de l'analyseur Basses & Harmoniques
    juce::Rectangle<int> graphBounds (20, 66, getWidth() - 40, 280);
    drawPhantomVisualizer (g, graphBounds);
}

void SaboteurPhantomEditor::drawPhantomVisualizer (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const float x0 = static_cast<float> (bounds.getX());
    const float y0 = static_cast<float> (bounds.getY());
    const float w  = static_cast<float> (bounds.getWidth());
    const float h  = static_cast<float> (bounds.getHeight());

    // Fond de l'écran spectral
    g.setColour (juce::Colour (0xff070b0e));
    g.fillRoundedRectangle (bounds.toFloat(), 8.0f);

    // Échelle logarithmique focalisée sur le grave et bas-médium (25 Hz -> 2500 Hz)
    const float minFreq = 25.0f;
    const float maxFreq = 2500.0f;
    auto freqToX = [&] (float freq)
    {
        float norm = std::log (std::clamp (freq, minFreq, maxFreq) / minFreq)
                   / std::log (maxFreq / minFreq);
        return x0 + norm * w;
    };

    // Grille des fréquences clés (Sub, Harmoniques 2f-5f, Bas-médiums)
    g.setFont (juce::FontOptions (11.0f));
    const float gridFreqs[] = { 40.0f, 60.0f, 100.0f, 160.0f, 250.0f, 400.0f, 800.0f, 1600.0f };
    for (float gf : gridFreqs)
    {
        float gx = freqToX (gf);
        g.setColour (juce::Colour (0xff14222e));
        g.drawVerticalLine (static_cast<int> (gx), y0, y0 + h);

        g.setColour (juce::Colour (0xff527a92));
        juce::String lbl = (gf >= 1000.0f) ? juce::String (gf / 1000.0f, 1) + "k"
                                           : juce::String (static_cast<int> (gf)) + "Hz";
        g.drawText (lbl, static_cast<int> (gx) - 24, static_cast<int> (y0 + h - 18.0f), 48, 16, juce::Justification::centred);
    }

    // Halo de la fondamentale ciblée (SUB FOCUS)
    const float subFocusHz = static_cast<float> (subFocusSlider.getValue());
    const float fxSub = freqToX (subFocusHz);
    {
        juce::ColourGradient subGrad (juce::Colour (0xff00e5ff).withAlpha (0.22f), fxSub, y0,
                                      juce::Colours::transparentBlack, fxSub + 55.0f, y0, true);
        g.setGradientFill (subGrad);
        g.fillRect (bounds);

        g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.65f));
        g.drawVerticalLine (static_cast<int> (fxSub), y0, y0 + h);

        // Repères subtils des harmoniques 2f, 3f et 4f
        for (int mult : { 2, 3, 4 })
        {
            float hx = freqToX (subFocusHz * static_cast<float> (mult));
            g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.22f));
            g.drawVerticalLine (static_cast<int> (hx), y0 + 24.0f, y0 + h - 20.0f);
            g.drawText (juce::String (mult) + "f", static_cast<int> (hx) - 12, static_cast<int> (y0 + 26.0f), 24, 14, juce::Justification::centred);
        }
    }

    if (guiDrySpec.size() > SaboteurPhantomProcessor::numBins)
    {
        juce::Path dryPath;
        juce::Path harmPath;
        juce::Path pocketPath;

        dryPath.startNewSubPath (x0, y0 + h);
        harmPath.startNewSubPath (x0, y0 + h);
        pocketPath.startNewSubPath (x0, y0);

        const float binHz = static_cast<float> (guiSampleRate) / static_cast<float> (SaboteurPhantomProcessor::fftSize);

        for (int i = 1; i <= SaboteurPhantomProcessor::numBins; ++i)
        {
            float freq = i * binHz;
            if (freq < minFreq || freq > maxFreq)
                continue;

            float px = freqToX (freq);

            // 1. Spectre Sec (Gris bleu acier en arrière-plan)
            float dryDb   = juce::Decibels::gainToDecibels (guiDrySpec[i] + 1e-7f, -72.0f);
            float dryNorm = std::clamp ((dryDb + 72.0f) / 72.0f, 0.0f, 1.0f);
            float pyDry   = (y0 + h) - dryNorm * (h * 0.84f);
            dryPath.lineTo (px, pyDry);

            // 2. Spectre des Harmoniques Fantômes générées (Cyan lumineux)
            float harmDb   = juce::Decibels::gainToDecibels (guiHarmSpec[i] + 1e-7f, -72.0f);
            float harmNorm = std::clamp ((harmDb + 72.0f) / 72.0f, 0.0f, 1.0f);
            float pyHarm   = (y0 + h) - harmNorm * (h * 0.84f);
            harmPath.lineTo (px, pyHarm);

            // 3. Courbe de démasquage Auto-Pocket (suspendue en haut en Ambre/Corail)
            float pocketDb  = juce::Decibels::gainToDecibels (std::clamp (guiPocketCurve[i], 0.1f, 1.0f));
            float redRatio  = std::clamp (std::abs (pocketDb) / 15.0f, 0.0f, 1.0f);
            float pyPocket  = y0 + redRatio * (h * 0.55f);
            pocketPath.lineTo (px, pyPocket);
        }

        dryPath.lineTo (x0 + w, y0 + h);
        dryPath.closeSubPath();

        harmPath.lineTo (x0 + w, y0 + h);
        harmPath.closeSubPath();

        pocketPath.lineTo (x0 + w, y0);
        pocketPath.closeSubPath();

        // Dessin du spectre sec
        g.setColour (juce::Colour (0xff4a6984).withAlpha (0.20f));
        g.fillPath (dryPath);
        g.setColour (juce::Colour (0xff6c93b4).withAlpha (0.45f));
        g.strokePath (dryPath, juce::PathStrokeType (1.2f));

        // Dessin des harmoniques fantômes (Cyan électrique)
        juce::ColourGradient harmGrad (juce::Colour (0xff00e5ff).withAlpha (0.45f), x0, y0 + h * 0.2f,
                                       juce::Colour (0xff006064).withAlpha (0.10f), x0, y0 + h, false);
        g.setGradientFill (harmGrad);
        g.fillPath (harmPath);

        g.setColour (juce::Colour (0xff00e5ff));
        g.strokePath (harmPath, juce::PathStrokeType (2.0f));

        // Dessin de la poche dynamique Auto-Pocket (Ambre/Corail)
        g.setColour (juce::Colour (0xffff6d00).withAlpha (0.28f));
        g.fillPath (pocketPath);
        g.setColour (juce::Colour (0xffff9100));
        g.strokePath (pocketPath, juce::PathStrokeType (2.0f));
    }

    // Témoin d'activité des harmoniques (en haut à droite du graphe)
    juce::Rectangle<float> badgeBox (x0 + w - 165.0f, y0 + 10.0f, 152.0f, 22.0f);
    g.setColour (juce::Colour (0xff101c26));
    g.fillRoundedRectangle (badgeBox, 5.0f);
    g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.25f + 0.75f * std::clamp (guiHarmActivity, 0.0f, 1.0f)));
    g.fillRoundedRectangle (badgeBox.reduced (2.0f).withWidth (badgeBox.getWidth() * std::clamp (guiHarmActivity, 0.08f, 1.0f)), 4.0f);
    g.setColour (juce::Colour (0xffe0f7fa));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("PHANTOM HARMONICS", badgeBox.toNearestInt(), juce::Justification::centred);

    // Cadre de finition
    g.setColour (juce::Colour (0xff00b8d4).withAlpha (0.65f));
    g.drawRoundedRectangle (bounds.toFloat(), 8.0f, 1.5f);
}

void SaboteurPhantomEditor::resized()
{
    // Barre supérieure
    modeSelector.setBounds (310, 12, 185, 28);
    soloButton.setBounds   (510, 12, 145, 28);
    phoneButton.setBounds  (665, 12, 145, 28);

    // Rangée des 7 potentiomètres en bas
    const int knobY   = 382;
    const int knobW   = 108;
    const int knobH   = 112;
    const int startX  = 24;
    const int spacing = 116;

    juce::Slider* sliders[] = { &subFocusSlider, &phantomDriveSlider, &characterSlider, &autoPocketSlider, &tightnessSlider, &subFloorSlider, &mixSlider };
    juce::Label*  labels[]  = { &subFocusLabel,  &phantomDriveLabel,  &characterLabel,  &autoPocketLabel,  &tightnessLabel,  &subFloorLabel,  &mixLabel  };

    for (int i = 0; i < 7; ++i)
    {
        int x = startX + i * spacing;
        labels[i]->setBounds  (x, knobY - 22, knobW, 20);
        sliders[i]->setBounds (x, knobY,      knobW, knobH);
    }
}