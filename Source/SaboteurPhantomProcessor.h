#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>

class SaboteurPhantomProcessor : public juce::AudioProcessor
{
public:
    SaboteurPhantomProcessor();
    ~SaboteurPhantomProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // --- MÉTHODES STANDARD JUCE OBLIGATOIRES ---
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    juce::AudioProcessorValueTreeState apvts;

    // Constantes pour l'analyseur visuel (60 FPS)
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder; // 2048
    static constexpr int numBins  = fftSize / 2;   // 1024

    void getVisualSnapshot (std::vector<float>& outDrySpec,
                            std::vector<float>& outHarmSpec,
                            std::vector<float>& outPocketCurve,
                            float& outHarmonicActivity,
                            float& outPocketGainDb,
                            double& outSampleRate);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Filtre Biquad interne léger et sans allocation pour le traitement échantillon par échantillon
    struct SimpleBiquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }

        float process (float x) noexcept
        {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void setLowPass (double sr, float freq, float q = 0.7071f) noexcept
        {
            float w0 = juce::MathConstants<float>::twoPi * std::clamp (freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos (w0), sinw = std::sin (w0);
            float alpha = sinw / (2.0f * q);
            float a0Inv = 1.0f / (1.0f + alpha);
            b0 = ((1.0f - cosw) * 0.5f) * a0Inv;
            b1 = (1.0f - cosw) * a0Inv;
            b2 = b0;
            a1 = (-2.0f * cosw) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }

        void setHighPass (double sr, float freq, float q = 0.7071f) noexcept
        {
            float w0 = juce::MathConstants<float>::twoPi * std::clamp (freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos (w0), sinw = std::sin (w0);
            float alpha = sinw / (2.0f * q);
            float a0Inv = 1.0f / (1.0f + alpha);
            b0 = ((1.0f + cosw) * 0.5f) * a0Inv;
            b1 = -(1.0f + cosw) * a0Inv;
            b2 = b0;
            a1 = (-2.0f * cosw) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }

        void setBandPass (double sr, float freq, float q = 1.0f) noexcept
        {
            float w0 = juce::MathConstants<float>::twoPi * std::clamp (freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos (w0), sinw = std::sin (w0);
            float alpha = sinw / (2.0f * q);
            float a0Inv = 1.0f / (1.0f + alpha);
            b0 = alpha * a0Inv;
            b1 = 0.0f;
            b2 = -alpha * a0Inv;
            a1 = (-2.0f * cosw) * a0Inv;
            a2 = (1.0f - alpha) * a0Inv;
        }

        void setPeaking (double sr, float freq, float q, float gainDb) noexcept
        {
            float A = std::pow (10.0f, gainDb / 40.0f);
            float w0 = juce::MathConstants<float>::twoPi * std::clamp (freq, 20.0f, static_cast<float> (sr * 0.48)) / static_cast<float> (sr);
            float cosw = std::cos (w0), sinw = std::sin (w0);
            float alpha = sinw / (2.0f * q);
            float a0Inv = 1.0f / (1.0f + alpha / A);
            b0 = (1.0f + alpha * A) * a0Inv;
            b1 = (-2.0f * cosw) * a0Inv;
            b2 = (1.0f - alpha * A) * a0Inv;
            a1 = b1;
            a2 = (1.0f - alpha / A) * a0Inv;
        }
    };

    struct ChannelState
    {
        // Isolation de la fondamentale Sub
        SimpleBiquad subHPF;
        SimpleBiquad subLPF1;
        SimpleBiquad subLPF2;

        // Filtre de trim du vrai Sub (< 55 Hz)
        SimpleBiquad subFloorLowPass;

        // Façonnage spectral des harmoniques générées (110 Hz - 480 Hz)
        SimpleBiquad harmHPF;
        SimpleBiquad harmLPF;

        // Filtre d'extraction du bas-médium sec pour l'Auto-Pocket (140 - 320 Hz)
        SimpleBiquad pocketBPF;

        // Filtres de simulation "Phone Check"
        SimpleBiquad phoneHPF1;
        SimpleBiquad phoneHPF2;
        SimpleBiquad phoneMidPeak;
        SimpleBiquad phoneLPF;

        // Suiveurs d'enveloppe et bloqueurs de continu (DC)
        float fastSubEnv  = 0.0f;
        float slowSubEnv  = 0.0f;
        float pocketEnv   = 0.0f;
        float dcStateEven = 0.0f;

        void reset() noexcept
        {
            subHPF.reset();
            subLPF1.reset();
            subLPF2.reset();
            subFloorLowPass.reset();
            harmHPF.reset();
            harmLPF.reset();
            pocketBPF.reset();
            phoneHPF1.reset();
            phoneHPF2.reset();
            phoneMidPeak.reset();
            phoneLPF.reset();
            fastSubEnv  = 0.0f;
            slowSubEnv  = 0.0f;
            pocketEnv   = 0.0f;
            dcStateEven = 0.0f;
        }
    };

    double currentSampleRate = 48000.0;
    std::vector<ChannelState> channelStates;

    // Analyseur FFT pour l'interface graphique uniquement (latence audio = 0)
    juce::dsp::FFT visFft;
    juce::dsp::WindowingFunction<float> visWindow;
    std::vector<float> visDryFifo;
    std::vector<float> visHarmFifo;
    std::vector<float> fftWorkspaceDry;
    std::vector<float> fftWorkspaceHarm;
    int visFifoIndex = 0;

    juce::SpinLock visualLock;
    std::vector<float> visDrySpectrum;
    std::vector<float> visHarmSpectrum;
    std::vector<float> visPocketCurve;
    float visHarmonicActivity = 0.0f;
    float visPocketGainDb     = 0.0f;

    void updateFilterCoefficients (ChannelState& state, float subFocusHz, int mode);
    void pushVisualSample (float drySample, float harmSample, float subFocusHz, float pocketReductionLinear);
    void computeVisualFft (float subFocusHz, float pocketReductionLinear);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaboteurPhantomProcessor)
};