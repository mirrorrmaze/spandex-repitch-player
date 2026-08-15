#include "FxPanel.h"
#include "MatrixEffects.h"

namespace
{
    void styleTitle(juce::Label& l)
    {
        l.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    }
}

FxPanel::FxPanel(EffectsChain& chain)
    : fx(chain)
{
    styleTitle(reverbTitle);
    addAndMakeVisible(reverbTitle);

    reverbEnable.onClick = [this] { fx.setReverbEnabled(reverbEnable.getToggleState()); };
    addAndMakeVisible(reverbEnable);

    reverbSize.slider.setRange(0.0, 1.0, 0.001);
    reverbSize.slider.onValueChange = [this]
    {
        auto p = fx.getReverbParams();
        p.roomSize = (float) reverbSize.slider.getValue();
        fx.setReverbParams(p);
    };
    addAndMakeVisible(reverbSize);

    reverbDamping.slider.setRange(0.0, 1.0, 0.001);
    reverbDamping.slider.onValueChange = [this]
    {
        auto p = fx.getReverbParams();
        p.damping = (float) reverbDamping.slider.getValue();
        fx.setReverbParams(p);
    };
    addAndMakeVisible(reverbDamping);

    reverbWet.slider.setRange(0.0, 1.0, 0.001);
    reverbWet.slider.onValueChange = [this]
    {
        auto p = fx.getReverbParams();
        p.wetLevel = (float) reverbWet.slider.getValue();
        fx.setReverbParams(p);
    };
    addAndMakeVisible(reverbWet);

    reverbDry.slider.setRange(0.0, 1.0, 0.001);
    reverbDry.slider.onValueChange = [this]
    {
        auto p = fx.getReverbParams();
        p.dryLevel = (float) reverbDry.slider.getValue();
        fx.setReverbParams(p);
    };
    addAndMakeVisible(reverbDry);

    reverbWidth.slider.setRange(0.0, 1.0, 0.001);
    reverbWidth.slider.onValueChange = [this]
    {
        auto p = fx.getReverbParams();
        p.width = (float) reverbWidth.slider.getValue();
        fx.setReverbParams(p);
    };
    addAndMakeVisible(reverbWidth);

    styleTitle(delayTitle);
    addAndMakeVisible(delayTitle);

    delayEnable.onClick = [this] { fx.setDelayEnabled(delayEnable.getToggleState()); };
    addAndMakeVisible(delayEnable);

    delayTime.slider.setRange(1.0, 2000.0, 1.0);
    delayTime.slider.onValueChange = [this] { fx.setDelayTimeMs((float) delayTime.slider.getValue()); };
    addAndMakeVisible(delayTime);

    delayGrainSize.slider.setRange(5.0, 500.0, 1.0);
    delayGrainSize.slider.onValueChange = [this] { fx.setDelayGrainSizeMs((float) delayGrainSize.slider.getValue()); };
    addAndMakeVisible(delayGrainSize);

    delayDensity.slider.setRange(0.5, 60.0, 0.1);
    delayDensity.slider.setSkewFactorFromMidPoint(8.0);
    delayDensity.slider.onValueChange = [this] { fx.setDelayDensity((float) delayDensity.slider.getValue()); };
    addAndMakeVisible(delayDensity);

    delaySpread.slider.setRange(0.0, 500.0, 1.0);
    delaySpread.slider.onValueChange = [this] { fx.setDelaySpreadMs((float) delaySpread.slider.getValue()); };
    addAndMakeVisible(delaySpread);

    delayPitchScatter.slider.setRange(0.0, 24.0, 0.1);
    delayPitchScatter.slider.onValueChange = [this] { fx.setDelayPitchScatterSemitones((float) delayPitchScatter.slider.getValue()); };
    addAndMakeVisible(delayPitchScatter);

    delayFeedback.slider.setRange(0.0, 0.95, 0.001);
    delayFeedback.slider.onValueChange = [this] { fx.setDelayFeedback((float) delayFeedback.slider.getValue()); };
    addAndMakeVisible(delayFeedback);

    delayMix.slider.setRange(0.0, 1.0, 0.001);
    delayMix.slider.onValueChange = [this] { fx.setDelayMix((float) delayMix.slider.getValue()); };
    addAndMakeVisible(delayMix);

    styleTitle(shifterTitle);
    addAndMakeVisible(shifterTitle);

    shifterEnable.onClick = [this] { fx.setShifterEnabled(shifterEnable.getToggleState()); };
    addAndMakeVisible(shifterEnable);

    shifterModeBox.addItem("Shift", 1);
    shifterModeBox.addItem("Ring Mod", 2);
    shifterModeBox.onChange = [this]
    {
        fx.setShifterMode(shifterModeBox.getSelectedId() == 2 ? FreqShifter::Mode::RingMod : FreqShifter::Mode::Shift);
    };
    addAndMakeVisible(shifterModeBox);

    shifterCoarse.slider.setRange(-2000.0, 2000.0, 1.0);
    shifterCoarse.slider.setDoubleClickReturnValue(true, 0.0);
    shifterCoarse.slider.onValueChange = [this] { fx.setShifterCoarseHz((float) shifterCoarse.slider.getValue()); };
    addAndMakeVisible(shifterCoarse);

    shifterFine.slider.setRange(-50.0, 50.0, 0.1);
    shifterFine.slider.setDoubleClickReturnValue(true, 0.0);
    shifterFine.slider.onValueChange = [this] { fx.setShifterFineHz((float) shifterFine.slider.getValue()); };
    addAndMakeVisible(shifterFine);

    shifterSpread.slider.setRange(0.0, 200.0, 0.5);
    shifterSpread.slider.onValueChange = [this] { fx.setShifterSpreadHz((float) shifterSpread.slider.getValue()); };
    addAndMakeVisible(shifterSpread);

    shifterFeedback.slider.setRange(0.0, 0.95, 0.001);
    shifterFeedback.slider.onValueChange = [this] { fx.setShifterFeedback((float) shifterFeedback.slider.getValue()); };
    addAndMakeVisible(shifterFeedback);

    shifterMix.slider.setRange(0.0, 1.0, 0.001);
    shifterMix.slider.onValueChange = [this] { fx.setShifterMix((float) shifterMix.slider.getValue()); };
    addAndMakeVisible(shifterMix);

    styleTitle(smudgeTitle);
    addAndMakeVisible(smudgeTitle);

    smudgeEnable.onClick = [this] { fx.setSmudgeEnabled(smudgeEnable.getToggleState()); };
    addAndMakeVisible(smudgeEnable);

    smudgeAmount.slider.setRange(0.0, 1.0, 0.001);
    smudgeAmount.slider.onValueChange = [this] { fx.setSmudgeAmount((float) smudgeAmount.slider.getValue()); };
    addAndMakeVisible(smudgeAmount);

    smudgeRate.slider.setRange(5.0, 200.0, 0.5);
    smudgeRate.slider.setSkewFactorFromMidPoint(46.4);
    smudgeRate.slider.onValueChange = [this]
    {
        pendingSmudgeRateMs = (float) smudgeRate.slider.getValue();
        startTimer(120);
    };
    addAndMakeVisible(smudgeRate);

    smudgeFeedback.slider.setRange(0.0, 0.95, 0.001);
    smudgeFeedback.slider.onValueChange = [this] { fx.setSmudgeFeedback((float) smudgeFeedback.slider.getValue()); };
    addAndMakeVisible(smudgeFeedback);

    styleTitle(lossyTitle);
    addAndMakeVisible(lossyTitle);

    lossyEnable.onClick = [this] { fx.setLossyEnabled(lossyEnable.getToggleState()); };
    addAndMakeVisible(lossyEnable);

    lossyBits.slider.setRange(1.0, 16.0, 0.1);
    lossyBits.slider.setDoubleClickReturnValue(true, 8.0);
    lossyBits.slider.onValueChange = [this] { fx.setLossyBits((float) lossyBits.slider.getValue()); };
    addAndMakeVisible(lossyBits);

    lossyRate.slider.setRange(1.0, 200.0, 0.1);
    lossyRate.slider.setSkewFactorFromMidPoint(40.0);
    lossyRate.slider.setDoubleClickReturnValue(true, 40.0);
    lossyRate.slider.setTextValueSuffix(" Hz");
    lossyRate.slider.onValueChange = [this] { fx.setLossyRateHz((float) lossyRate.slider.getValue()); };
    addAndMakeVisible(lossyRate);

    lossyJitter.slider.setRange(0.0, 1.0, 0.001);
    lossyJitter.slider.setDoubleClickReturnValue(true, 0.3);
    lossyJitter.slider.onValueChange = [this] { fx.setLossyJitter((float) lossyJitter.slider.getValue()); };
    addAndMakeVisible(lossyJitter);

    lossyMix.slider.setRange(0.0, 1.0, 0.001);
    lossyMix.slider.setDoubleClickReturnValue(true, 1.0);
    lossyMix.slider.onValueChange = [this] { fx.setLossyMix((float) lossyMix.slider.getValue()); };
    addAndMakeVisible(lossyMix);

    styleTitle(gainTitle);
    addAndMakeVisible(gainTitle);

    inputGain.slider.setRange(-24.0, 24.0, 0.1);
    inputGain.slider.setDoubleClickReturnValue(true, 0.0);
    inputGain.slider.onValueChange = [this] { fx.setInputGainDb((float) inputGain.slider.getValue()); };
    addAndMakeVisible(inputGain);

    outputGain.slider.setRange(-24.0, 24.0, 0.1);
    outputGain.slider.setDoubleClickReturnValue(true, 0.0);
    outputGain.slider.onValueChange = [this] { fx.setOutputGainDb((float) outputGain.slider.getValue()); };
    addAndMakeVisible(outputGain);

    driveKnob.slider.setRange(0.0, 15.0, 0.1);
    driveKnob.slider.setDoubleClickReturnValue(true, 0.0);
    driveKnob.slider.onValueChange = [this] { fx.setDriveDb((float) driveKnob.slider.getValue()); };
    addAndMakeVisible(driveKnob);

    clipKnob.slider.setRange(0.0, 1.0, 0.001);
    clipKnob.slider.setDoubleClickReturnValue(true, 0.0);
    clipKnob.slider.onValueChange = [this] { fx.setClipAmount((float) clipKnob.slider.getValue()); };
    addAndMakeVisible(clipKnob);

    routingBar.onOrderChanged = [this](const std::vector<EffectsChain::FxStage>& order)
    {
        fx.setChainOrder(order);
    };
    addAndMakeVisible(routingBar);

    refreshFromEngine();
}

void FxPanel::timerCallback()
{
    stopTimer();
    fx.setSmudgeRateMs(pendingSmudgeRateMs);
}

void FxPanel::refreshFromEngine()
{
    reverbEnable.setToggleState(fx.isReverbEnabled(), juce::dontSendNotification);
    auto p = fx.getReverbParams();
    reverbSize.slider.setValue(p.roomSize, juce::dontSendNotification);
    reverbDamping.slider.setValue(p.damping, juce::dontSendNotification);
    reverbWet.slider.setValue(p.wetLevel, juce::dontSendNotification);
    reverbDry.slider.setValue(p.dryLevel, juce::dontSendNotification);
    reverbWidth.slider.setValue(p.width, juce::dontSendNotification);

    delayEnable.setToggleState(fx.isDelayEnabled(), juce::dontSendNotification);
    delayTime.slider.setValue(fx.getDelayTimeMs(), juce::dontSendNotification);
    delayGrainSize.slider.setValue(fx.getDelayGrainSizeMs(), juce::dontSendNotification);
    delayDensity.slider.setValue(fx.getDelayDensity(), juce::dontSendNotification);
    delaySpread.slider.setValue(fx.getDelaySpreadMs(), juce::dontSendNotification);
    delayPitchScatter.slider.setValue(fx.getDelayPitchScatterSemitones(), juce::dontSendNotification);
    delayFeedback.slider.setValue(fx.getDelayFeedback(), juce::dontSendNotification);
    delayMix.slider.setValue(fx.getDelayMix(), juce::dontSendNotification);

    shifterEnable.setToggleState(fx.isShifterEnabled(), juce::dontSendNotification);
    shifterModeBox.setSelectedId(fx.getShifterMode() == FreqShifter::Mode::RingMod ? 2 : 1, juce::dontSendNotification);
    shifterCoarse.slider.setValue(fx.getShifterCoarseHz(), juce::dontSendNotification);
    shifterFine.slider.setValue(fx.getShifterFineHz(), juce::dontSendNotification);
    shifterSpread.slider.setValue(fx.getShifterSpreadHz(), juce::dontSendNotification);
    shifterFeedback.slider.setValue(fx.getShifterFeedback(), juce::dontSendNotification);
    shifterMix.slider.setValue(fx.getShifterMix(), juce::dontSendNotification);

    smudgeEnable.setToggleState(fx.isSmudgeEnabled(), juce::dontSendNotification);
    smudgeAmount.slider.setValue(fx.getSmudgeAmount(), juce::dontSendNotification);
    smudgeRate.slider.setValue(fx.getSmudgeRateMs(), juce::dontSendNotification);
    smudgeFeedback.slider.setValue(fx.getSmudgeFeedback(), juce::dontSendNotification);

    lossyEnable.setToggleState(fx.isLossyEnabled(), juce::dontSendNotification);
    lossyBits.slider.setValue(fx.getLossyBits(), juce::dontSendNotification);
    lossyRate.slider.setValue(fx.getLossyRateHz(), juce::dontSendNotification);
    lossyJitter.slider.setValue(fx.getLossyJitter(), juce::dontSendNotification);
    lossyMix.slider.setValue(fx.getLossyMix(), juce::dontSendNotification);

    inputGain.slider.setValue(fx.getInputGainDb(), juce::dontSendNotification);
    outputGain.slider.setValue(fx.getOutputGainDb(), juce::dontSendNotification);
    driveKnob.slider.setValue(fx.getDriveDb(), juce::dontSendNotification);
    clipKnob.slider.setValue(fx.getClipAmount(), juce::dontSendNotification);

    routingBar.setOrder(fx.getChainOrder());
}

void FxPanel::paint(juce::Graphics& g)
{
    g.fillAll(AppLookAndFeel::bg);

    for (auto* card : { &reverbCard, &delayCard, &shifterCard, &smudgeCard, &lossyCard, &gainCard })
        MatrixEffects::drawCardFrame(g, *card);

    MatrixEffects::drawScanlines(g, getLocalBounds());
}

void FxPanel::layoutSection(juce::Rectangle<int> area, juce::Label& title, juce::ToggleButton* enable,
                             std::initializer_list<LabeledKnob*> knobs, juce::ComboBox* modeBox)
{
    area.reduce(16, 12);
    auto top = area.removeFromTop(28);
    if (enable != nullptr)
    {
        title.setBounds(top.removeFromLeft(top.getWidth() - 70));
        enable->setBounds(top);
    }
    else
    {
        title.setBounds(top);
    }

    area.removeFromTop(10);

    // The mode box (if any) claims a small guaranteed strip off the bottom
    // first, before the knobs get laid out - sized to stay usable (a real
    // clickable dropdown, not a sliver) but small enough that a card with
    // one only gives up a little knob height, not a lot. Reserving it here,
    // up front, means it can never be squeezed to nothing the way it was
    // when it only got whatever the knob row happened to leave over - at
    // the smallest window size that leftover was zero. Sections without a
    // mode box (the majority) skip this entirely and keep the full card
    // height for their knobs, same as before Freq Shifter ever grew one.
    constexpr int modeBoxHeight = 22, modeBoxGap = 6;
    juce::Rectangle<int> modeBoxArea;
    if (modeBox != nullptr)
        modeBoxArea = area.removeFromBottom(modeBoxHeight + modeBoxGap).removeFromBottom(modeBoxHeight);

    // No wrapping, ever: knobs share the row's width with flexGrow so a
    // section can never spill into the space below it (the bug that
    // previously made Reverb's Width knob and Delay's Mix knob overlap the
    // section drawn underneath). withMinWidth stops them shrinking below
    // what their own text box needs - past that point the row overflows
    // the card rather than squashing into illegible/clipped digits.
    //
    // Height is whatever's actually left in `area` (capped at 112, the
    // knob's natural size) so a knob row can never request more height than
    // its card has and get pushed past the card's bottom edge - it shrinks
    // to fit instead, same as the width already did.
    constexpr int knobRowHeight = 112;
    const float knobHeight = (float) juce::jlimit(1, knobRowHeight, area.getHeight());

    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    for (auto* knob : knobs)
        fb.items.add(juce::FlexItem(*knob).withFlex(1.0f).withMinWidth(52.0f).withMaxWidth(105).withHeight(knobHeight).withMargin(4));
    fb.performLayout(area.removeFromTop((int) knobHeight));

    if (modeBox != nullptr)
        modeBox->setBounds(modeBoxArea);
}

void FxPanel::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    const int gap = 10;

    constexpr int routingBarHeight = 40;
    auto routingArea = bounds.removeFromBottom(routingBarHeight);
    bounds.removeFromBottom(gap);
    routingBar.setBounds(routingArea);

    const int rowHeight = (bounds.getHeight() - gap) / 2;

    auto topRow = bounds.removeFromTop(rowHeight);
    bounds.removeFromTop(gap);
    auto bottomRow = bounds;

    // Split by each card's knob count (5:7:5) rather than a fixed fraction -
    // Reverb and Freq Shifter both have 5 knobs but a flat 1/3 vs "whatever's
    // left after Delay's 3/5" split gave them very different widths, which
    // could starve Shifter below its own knobs' combined minimum width and
    // clip its rightmost knob/readout off the window's edge.
    constexpr int reverbWeight = 5, delayWeight = 7, shifterWeight = 5;
    constexpr int totalWeight = reverbWeight + delayWeight + shifterWeight;
    const int available = topRow.getWidth() - 2 * gap;
    reverbCard = topRow.removeFromLeft(available * reverbWeight / totalWeight);
    topRow.removeFromLeft(gap);
    delayCard = topRow.removeFromLeft(available * delayWeight / totalWeight);
    topRow.removeFromLeft(gap);
    shifterCard = topRow;

    // Same weighted-by-knob-count approach as the top row - Smudge (3),
    // Lossy (4), and Gain (4) split unevenly rather than a flat thirds,
    // so none of them gets starved below its own knobs' minimum width.
    constexpr int smudgeWeight = 3, lossyWeight = 4, gainWeight = 4;
    constexpr int bottomTotalWeight = smudgeWeight + lossyWeight + gainWeight;
    const int bottomAvailable = bottomRow.getWidth() - 2 * gap;
    smudgeCard = bottomRow.removeFromLeft(bottomAvailable * smudgeWeight / bottomTotalWeight);
    bottomRow.removeFromLeft(gap);
    lossyCard = bottomRow.removeFromLeft(bottomAvailable * lossyWeight / bottomTotalWeight);
    bottomRow.removeFromLeft(gap);
    gainCard = bottomRow;

    layoutSection(reverbCard, reverbTitle, &reverbEnable, { &reverbSize, &reverbDamping, &reverbWet, &reverbDry, &reverbWidth });
    layoutSection(delayCard, delayTitle, &delayEnable,
                  { &delayTime, &delayGrainSize, &delayDensity, &delaySpread, &delayPitchScatter, &delayFeedback, &delayMix });
    layoutSection(shifterCard, shifterTitle, &shifterEnable,
                  { &shifterCoarse, &shifterFine, &shifterSpread, &shifterFeedback, &shifterMix }, &shifterModeBox);
    layoutSection(smudgeCard, smudgeTitle, &smudgeEnable, { &smudgeAmount, &smudgeRate, &smudgeFeedback });
    layoutSection(lossyCard, lossyTitle, &lossyEnable, { &lossyBits, &lossyRate, &lossyJitter, &lossyMix });
    layoutSection(gainCard, gainTitle, nullptr, { &inputGain, &outputGain, &driveKnob, &clipKnob });
}
