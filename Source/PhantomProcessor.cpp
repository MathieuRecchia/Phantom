#include "SaboteurPhantomProcessor.h"
#include "SaboteurPhantomEditor.h"

SaboteurPhantomProcessor::SaboteurPhantomProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      visFft (fftOrder),
      visWindow (fftSize, juce::dsp::WindowingFunction<float>::hann, false)
{
    visDryFifo.assign (fftSize, 0.0f);
    visHarmFifo.assign (fftSize, 0.0f);
    fftWorkspaceDry.assign (fftSize * 2, 0.0f);
    fftWorkspaceHarm.assign (fftSize * 2, 0.0f);

    visDrySpectrum.assign (numBins + 1, 0.0f);
    visHarmSpectrum.assign (numBins + 1, 0.0f);
    visPocketCurve.assign (numBins + 1, 1.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout SaboteurPhantomProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Mode de comportement Rock
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "mode", "Target Mode",
        juce::StringArray { "Punch (Kick)", "Growl (Bass)", "Full Bus (Drums+Bass)" }, 1));

    // 2. Fréquence centrale du Sub à traduire (40 Hz - 110 Hz)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "sub_focus", "Sub Focus",
        juce::NormalisableRange<float> (40.0f, 110.0f, 0.5f, 0.65f), 68.0f, "Hz"));

    // 3. Quantité d'harmoniques fantômes générées
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "phantom_drive", "Phantom Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.8f), 55.0f, "%"));

    // 4. Dosage Harmoniques Paires (Octave ronde) <-> Impaires (Mordant Rock / SVT)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "character", "Character (Even/Odd)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    // 5. Auto-Pocket : creusement dynamique du bas-médium sec (140 - 320 Hz)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "auto_pocket", "Auto-Pocket",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 45.0f, "%"));

    // 6. Tightness : contrôle du sustain des harmoniques sur les jeux rapides
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "tightness", "Tightness",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, "%"));

    // 7. Sub Floor : ajustement du vrai grave sous 55 Hz pour gérer le headroom
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "sub_floor", "Sub Floor",
        juce::NormalisableRange<float> (-12.0f, 6.0f, 0.1f), 0.0f, "dB"));

    // 8. Mix parallèle
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    // 9. Boutons d'écoute : Harmonics Solo & Phone Check
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "harmonics_solo", "Harmonics Solo", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "phone_check", "Phone Speaker Check", false));

    return { params.begin(), params.end() };
}

bool SaboteurPhantomProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void SaboteurPhantomProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;

    const int numChannels = std::max (1, getTotalNumInputChannels());
    channelStates.resize (numChannels);
    for (auto& state : channelStates)
        state.reset();

    visFifoIndex = 0;
    std::fill (visDryFifo.begin(), visDryFifo.end(), 0.0f);
    std::fill (visHarmFifo.begin(), visHarmFifo.end(), 0.0f);

    // Latence nulle (0 échantillon) pour préserver l'impact des transitoires Rock
    setLatencySamples (0);
}

void SaboteurPhantomProcessor::releaseResources()
{
}

void SaboteurPhantomProcessor::updateFilterCoefficients (ChannelState& state, float subFocusHz, int mode)
{
    // 1. Isolation chirurgicale de la fondamentale (32 Hz -> subFocusHz)
    state.subHPF.setHighPass (currentSampleRate, 32.0f, 0.7071f);
    state.subLPF1.setLowPass (currentSampleRate, subFocusHz, 0.85f);
    state.subLPF2.setLowPass (currentSampleRate, subFocusHz * 1.15f, 0.7071f);

    // 2. Extraction du très bas grave (< 55 Hz) pour le réglage Sub Floor
    state.subFloorLowPass.setLowPass (currentSampleRate, 55.0f, 0.7071f);

    // 3. Fenêtre de projection des harmoniques fantômes (115 Hz -> 460 Hz)
    float harmHighCut = (mode == 1) ? 520.0f : ((mode == 0) ? 380.0f : 440.0f);
    state.harmHPF.setHighPass (currentSampleRate, subFocusHz * 1.55f, 0.75f);
    state.harmLPF.setLowPass  (currentSampleRate, harmHighCut, 0.75f);

    // 4. Bande Auto-Pocket centrée sur la zone de boue (environ 2.6x la fondamentale, 150-290 Hz)
    float pocketCenterHz = std::clamp (subFocusHz * 2.6f, 145.0f, 295.0f);
    float pocketQ = (mode == 2) ? 0.9f : 1.35f;
    state.pocketBPF.setBandPass (currentSampleRate, pocketCenterHz, pocketQ);

    // 5. Simulation de haut-parleur de smartphone (Coupe-bas brutal à 220 Hz + résonance 950 Hz + coupe-haut 6.5 kHz)
    state.phoneHPF1.setHighPass  (currentSampleRate, 220.0f, 0.85f);
    state.phoneHPF2.setHighPass  (currentSampleRate, 220.0f, 0.85f);
    state.phoneMidPeak.setPeaking (currentSampleRate, 950.0f, 1.4f, 4.5f);
    state.phoneLPF.setLowPass    (currentSampleRate, 6500.0f, 0.7071f);
}

void SaboteurPhantomProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (static_cast<int> (channelStates.size()) < totalNumInputChannels)
        channelStates.resize (totalNumInputChannels);

    // Lecture atomique des paramètres
    const int mode             = static_cast<int> (apvts.getRawParameterValue ("mode")->load());
    const float subFocusHz     = apvts.getRawParameterValue ("sub_focus")->load();
    const float phantomDrive   = apvts.getRawParameterValue ("phantom_drive")->load() * 0.01f;
    const float character      = apvts.getRawParameterValue ("character")->load() * 0.01f;
    const float autoPocket     = apvts.getRawParameterValue ("auto_pocket")->load() * 0.01f;
    const float tightness      = apvts.getRawParameterValue ("tightness")->load() * 0.01f;
    const float subFloorDb     = apvts.getRawParameterValue ("sub_floor")->load();
    const float wetMix         = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    const bool harmonicsSolo   = apvts.getRawParameterValue ("harmonics_solo")->load() > 0.5f;
    const bool phoneCheck      = apvts.getRawParameterValue ("phone_check")->load() > 0.5f;

    const float subFloorDeltaGain = juce::Decibels::decibelsToGain (subFloorDb) - 1.0f;

    // Constantes temporelles des suiveurs d'enveloppe selon le mode
    const float sr = static_cast<float> (currentSampleRate);
    const float fastAttack  = std::exp (-1.0f / (0.0015f * sr)); // 1.5 ms
    const float fastRelease = std::exp (-1.0f / ((mode == 0 ? 0.025f : 0.045f) * sr));
    const float slowAttack  = std::exp (-1.0f / (0.018f * sr));  // 18 ms
    const float slowRelease = std::exp (-1.0f / (0.090f * sr));  // 90 ms

    // Biais harmonique par mode : Punch favorise l'octave, Growl favorise les impaires (SVT)
    float oddBias = character;
    if (mode == 0)      oddBias = std::clamp (character * 0.75f, 0.0f, 1.0f);
    else if (mode == 1) oddBias = std::clamp (0.25f + character * 0.75f, 0.0f, 1.0f);
    const float evenBias = 1.0f - oddBias * 0.65f;

    // Gain de sortie des harmoniques fantômes
    const float driveGain = phantomDrive * 1.85f;

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        updateFilterCoefficients (channelStates[ch], subFocusHz, mode);

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto& st = channelStates[ch];
        auto* channelData = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = channelData[i];

            // --- A. ISOLATION DE LA FONDAMENTALE SUB ---
            float sub = st.subHPF.process (dry);
            sub = st.subLPF1.process (sub);
            sub = st.subLPF2.process (sub);

            // Suivi d'enveloppe rapide et lent de la fondamentale
            const float absSub = std::abs (sub);
            st.fastSubEnv = (absSub > st.fastSubEnv)
                          ? fastAttack * st.fastSubEnv + (1.0f - fastAttack) * absSub
                          : fastRelease * st.fastSubEnv + (1.0f - fastRelease) * absSub;

            st.slowSubEnv = (absSub > st.slowSubEnv)
                          ? slowAttack * st.slowSubEnv + (1.0f - slowAttack) * absSub
                          : slowRelease * st.slowSubEnv + (1.0f - slowRelease) * absSub;

            // --- B. POLYNÔMES DE TCHEBYCHEV NORMALISÉS PAR L'ENVELOPPE ---
            // Gate doux à -60 dB pour ne jamais amplifier le souffle entre les notes
            const float noiseGate = std::clamp ((st.fastSubEnv - 0.0008f) * 250.0f, 0.0f, 1.0f);

            // Onde porteuse normalisée entre [-1, +1] indépendante de la vélocité du bassiste
            const float normCarrier = std::tanh ((sub / (st.fastSubEnv + 0.0005f)) * 1.15f);
            const float u2 = normCarrier * normCarrier;
            const float u3 = u2 * normCarrier;
            const float u4 = u2 * u2;
            const float u5 = u4 * normCarrier;

            // Polynômes de Tchebychev T2, T3, T4, T5 (avec suppression de l'offset continu sur T2/T4)
            const float t2 = (2.0f * u2 - 1.0f);
            const float t3 = (4.0f * u3 - 3.0f * normCarrier);
            const float t4 = (8.0f * u4 - 8.0f * u2 + 1.0f);
            const float t5 = (16.0f * u5 - 20.0f * u3 + 5.0f * normCarrier);

            float rawEven = 0.75f * t2 + 0.25f * t4;
            st.dcStateEven = 0.995f * st.dcStateEven + 0.005f * rawEven;
            const float cleanEven = rawEven - st.dcStateEven;

            const float cleanOdd = 0.72f * t3 + 0.28f * t5;

            // --- C. SCULPTEUR D'ENVELOPPE "TIGHTNESS" ---
            // Compare l'enveloppe rapide à l'enveloppe lente pour raccourcir le sustain des harmoniques
            float sustainRatio = st.fastSubEnv / (st.slowSubEnv + 0.0005f);
            float tightMultiplier = std::clamp (std::pow (sustainRatio, tightness * 1.8f), 0.12f, 1.4f);

            float harmonicSignal = (evenBias * cleanEven + oddBias * cleanOdd)
                                 * st.fastSubEnv * noiseGate * tightMultiplier * driveGain;

            // Filtrage passe-bande musical des harmoniques (115 Hz - 480 Hz)
            harmonicSignal = st.harmHPF.process (harmonicSignal);
            harmonicSignal = st.harmLPF.process (harmonicSignal);

            // --- D. AUTO-POCKET (DÉMASQUAGE DYNAMIQUE DES BAS-MÉDIUMS) ---
            float dryLowMid = st.pocketBPF.process (dry);
            float harmEnergy = std::abs (harmonicSignal);
            st.pocketEnv = 0.992f * st.pocketEnv + 0.008f * harmEnergy;

            // Proportion de boue (150-300 Hz) soustraite dynamiquement du signal sec
            float pocketDuckAmount = std::clamp (st.pocketEnv * autoPocket * 3.2f, 0.0f, 0.78f);
            float carvedDry = dry - (dryLowMid * pocketDuckAmount);

            // Ajustement optionnel du Sub Floor (< 55 Hz)
            float veryLowSub = st.subFloorLowPass.process (dry);
            carvedDry += veryLowSub * subFloorDeltaGain;

            // --- E. ASSEMBLAGE & MODES D'ÉCOUTE ---
            float wetSignal = carvedDry + harmonicSignal;
            float outSample = harmonicsSolo ? harmonicSignal
                                            : ((1.0f - wetMix) * dry + wetMix * wetSignal);

            // Sécurité anti-saturation douce en sortie
            outSample = std::tanh (outSample * 0.95f) / 0.95f;
            channelData[i] = outSample;

            // Envoi au canal 0 vers l'analyseur graphique 60 FPS
            if (ch == 0)
            {
                pushVisualSample (dry, harmonicSignal, subFocusHz, 1.0f - pocketDuckAmount);
                visHarmonicActivity = 0.9995f * visHarmonicActivity
                                    + 0.0005f * std::clamp (harmEnergy * 4.5f, 0.0f, 1.0f);
            }
        }
    }

    // --- F. MODE "PHONE SPEAKER CHECK" (MONO + FILTRAGE HAUT-PARLEUR TÉLÉPHONE) ---
    if (phoneCheck)
    {
        if (totalNumInputChannels >= 2)
        {
            auto* left  = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            auto& stL   = channelStates[0];

            for (int i = 0; i < numSamples; ++i)
            {
                float mono = 0.5f * (left[i] + right[i]);
                mono = stL.phoneHPF1.process (mono);
                mono = stL.phoneHPF2.process (mono);
                mono = stL.phoneMidPeak.process (mono);
                mono = stL.phoneLPF.process (mono);
                left[i]  = mono;
                right[i] = mono;
            }
        }
        else if (totalNumInputChannels == 1)
        {
            auto* monoData = buffer.getWritePointer (0);
            auto& stL      = channelStates[0];
            for (int i = 0; i < numSamples; ++i)
            {
                float m = stL.phoneHPF1.process (monoData[i]);
                m = stL.phoneHPF2.process (m);
                m = stL.phoneMidPeak.process (m);
                m = stL.phoneLPF.process (m);
                monoData[i] = m;
            }
        }
    }
}

void SaboteurPhantomProcessor::pushVisualSample (float drySample, float harmSample,
                                                 float subFocusHz, float pocketReductionLinear)
{
    visDryFifo[visFifoIndex]  = drySample;
    visHarmFifo[visFifoIndex] = harmSample;
    ++visFifoIndex;

    if (visFifoIndex >= fftSize)
    {
        visFifoIndex = 0;
        computeVisualFft (subFocusHz, pocketReductionLinear);
    }
}

void SaboteurPhantomProcessor::computeVisualFft (float subFocusHz, float pocketReductionLinear)
{
    std::fill (fftWorkspaceDry.begin(), fftWorkspaceDry.end(), 0.0f);
    std::fill (fftWorkspaceHarm.begin(), fftWorkspaceHarm.end(), 0.0f);

    std::copy (visDryFifo.begin(), visDryFifo.end(), fftWorkspaceDry.begin());
    std::copy (visHarmFifo.begin(), visHarmFifo.end(), fftWorkspaceHarm.begin());

    visWindow.multiplyWithWindowingTable (fftWorkspaceDry.data(), fftSize);
    visWindow.multiplyWithWindowingTable (fftWorkspaceHarm.data(), fftSize);

    visFft.performRealOnlyForwardTransform (fftWorkspaceDry.data());
    visFft.performRealOnlyForwardTransform (fftWorkspaceHarm.data());

    const juce::SpinLock::ScopedTryLockType lock (visualLock);
    if (lock.isLocked())
    {
        const float invNorm = 2.0f / static_cast<float> (fftSize);
        const float binHz   = static_cast<float> (currentSampleRate) / static_cast<float> (fftSize);
        const float pocketCenterHz = std::clamp (subFocusHz * 2.6f, 145.0f, 295.0f);

        for (int i = 1; i <= numBins; ++i)
        {
            float dr = fftWorkspaceDry[i * 2];
            float di = fftWorkspaceDry[i * 2 + 1];
            float dryMag = std::sqrt (dr * dr + di * di) * invNorm;

            float hr = fftWorkspaceHarm[i * 2];
            float hi = fftWorkspaceHarm[i * 2 + 1];
            float harmMag = std::sqrt (hr * hr + hi * hi) * invNorm;

            visDrySpectrum[i]  = 0.65f * visDrySpectrum[i]  + 0.35f * dryMag;
            visHarmSpectrum[i] = 0.60f * visHarmSpectrum[i] + 0.40f * harmMag;

            // Courbe de creusement Auto-Pocket autour de pocketCenterHz
            float freq = i * binHz;
            float octDist = std::log2 (std::max (20.0f, freq) / pocketCenterHz);
            float bell = std::exp (-3.5f * octDist * octDist);
            float targetGain = 1.0f - (1.0f - pocketReductionLinear) * bell;
            visPocketCurve[i] = 0.70f * visPocketCurve[i] + 0.30f * targetGain;
        }

        visPocketGainDb = juce::Decibels::gainToDecibels (std::clamp (pocketReductionLinear, 0.1f, 1.0f));
    }
}

void SaboteurPhantomProcessor::getVisualSnapshot (std::vector<float>& outDrySpec,
                                                  std::vector<float>& outHarmSpec,
                                                  std::vector<float>& outPocketCurve,
                                                  float& outHarmonicActivity,
                                                  float& outPocketGainDb,
                                                  double& outSampleRate)
{
    const juce::SpinLock::ScopedLockType lock (visualLock);
    outDrySpec          = visDrySpectrum;
    outHarmSpec         = visHarmSpectrum;
    outPocketCurve      = visPocketCurve;
    outHarmonicActivity = visHarmonicActivity;
    outPocketGainDb     = visPocketGainDb;
    outSampleRate       = currentSampleRate;
}

// --- MÉTHODES STANDARD JUCE OBLIGATOIRES ---
const juce::String SaboteurPhantomProcessor::getName() const { return "Saboteur Phantom"; }
bool SaboteurPhantomProcessor::acceptsMidi() const { return false; }
bool SaboteurPhantomProcessor::producesMidi() const { return false; }
bool SaboteurPhantomProcessor::isMidiEffect() const { return false; }
double SaboteurPhantomProcessor::getTailLengthSeconds() const { return 0.0; }

int SaboteurPhantomProcessor::getNumPrograms() { return 1; }
int SaboteurPhantomProcessor::getCurrentProgram() { return 0; }
void SaboteurPhantomProcessor::setCurrentProgram (int) {}
const juce::String SaboteurPhantomProcessor::getProgramName (int) { return {}; }
void SaboteurPhantomProcessor::changeProgramName (int, const juce::String&) {}

void SaboteurPhantomProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SaboteurPhantomProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* SaboteurPhantomProcessor::createEditor()
{
    return new SaboteurPhantomEditor (*this);
}

bool SaboteurPhantomProcessor::hasEditor() const { return true; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SaboteurPhantomProcessor();
}