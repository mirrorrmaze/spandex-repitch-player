#include "EqPanel.h"
#include "MatrixEffects.h"

namespace
{
    constexpr int idOffset = 1;

    juce::String nameForType(EffectsChain::FilterType t)
    {
        using FT = EffectsChain::FilterType;
        switch (t)
        {
            case FT::HighPass:  return "High Pass";
            case FT::LowShelf:  return "Low Shelf";
            case FT::Bell:      return "Bell";
            case FT::Notch:     return "Notch";
            case FT::BandPass:  return "Band Pass";
            case FT::HighShelf: return "High Shelf";
            case FT::LowPass:   return "Low Pass";
        }
        return "Bell";
    }
}

EqBandColumn::EqBandColumn(EffectsChain& chain, int bandIndex)
    : fx(chain), index(bandIndex)
{
    const auto colour = EqCurveEditor::colourForBand(index);

    header.setText("Band " + juce::String(index + 1), juce::dontSendNotification);
    header.setColour(juce::Label::textColourId, colour);
    header.setJustificationType(juce::Justification::centred);
    header.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(header);

    enableToggle.onClick = [this]
    {
        fx.setEqBandEnabled(index, enableToggle.getToggleState());
        if (onChanged) onChanged();
    };
    addAndMakeVisible(enableToggle);

    using FT = EffectsChain::FilterType;
    for (auto t : { FT::HighPass, FT::LowShelf, FT::Bell, FT::BandPass, FT::Notch, FT::HighShelf, FT::LowPass })
        typeBox.addItem(nameForType(t), (int) t + idOffset);
    typeBox.onChange = [this]
    {
        fx.setEqBandType(index, (EffectsChain::FilterType) (typeBox.getSelectedId() - idOffset));
        if (onChanged) onChanged();
    };
    addAndMakeVisible(typeBox);

    freqKnob.slider.setRange(20.0, 20000.0, 1.0);
    freqKnob.slider.setSkewFactorFromMidPoint(1000.0);
    freqKnob.slider.setTextValueSuffix(" Hz");
    freqKnob.slider.onValueChange = [this]
    {
        fx.setEqBandFrequency(index, (float) freqKnob.slider.getValue());
        if (onChanged) onChanged();
    };
    addAndMakeVisible(freqKnob);

    gainKnob.slider.setRange(-24.0, 24.0, 0.1);
    gainKnob.slider.setTextValueSuffix(" dB");
    gainKnob.slider.onValueChange = [this]
    {
        fx.setEqBandGainDb(index, (float) gainKnob.slider.getValue());
        if (onChanged) onChanged();
    };
    addAndMakeVisible(gainKnob);

    qKnob.slider.setRange(0.1, 18.0, 0.01);
    qKnob.slider.setSkewFactorFromMidPoint(1.0);
    qKnob.slider.onValueChange = [this]
    {
        fx.setEqBandQ(index, (float) qKnob.slider.getValue());
        if (onChanged) onChanged();
    };
    addAndMakeVisible(qKnob);

    refresh();
}

void EqBandColumn::lookAndFeelChanged()
{
    header.setColour(juce::Label::textColourId, EqCurveEditor::colourForBand(index));
}

void EqBandColumn::refresh()
{
    auto& band = fx.getEqBand(index);
    enableToggle.setToggleState(band.enabled.load(), juce::dontSendNotification);
    typeBox.setSelectedId((int) band.type.load() + idOffset, juce::dontSendNotification);
    freqKnob.slider.setValue(band.frequency.load(), juce::dontSendNotification);
    gainKnob.slider.setValue(band.gainDb.load(), juce::dontSendNotification);
    qKnob.slider.setValue(band.q.load(), juce::dontSendNotification);
}

void EqBandColumn::resized()
{
    auto bounds = getLocalBounds().reduced(4);
    header.setBounds(bounds.removeFromTop(18));
    enableToggle.setBounds(bounds.removeFromTop(22));
    typeBox.setBounds(bounds.removeFromTop(26).reduced(2, 0));

    bounds.removeFromTop(4);
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.items.add(juce::FlexItem(freqKnob).withFlex(1.0f));
    fb.items.add(juce::FlexItem(gainKnob).withFlex(1.0f));
    fb.items.add(juce::FlexItem(qKnob).withFlex(1.0f));
    fb.performLayout(bounds);
}

EqPanel::EqPanel(EffectsChain& chain)
    : fx(chain), curveEditor(chain)
{
    addAndMakeVisible(curveEditor);

    for (int i = 0; i < EffectsChain::numEqBands; ++i)
    {
        auto* column = columns.add(new EqBandColumn(fx, i));
        addAndMakeVisible(column);
    }

    curveEditor.onBandChanged = [this](int idx)
    {
        if (idx >= 0 && idx < columns.size())
            columns[idx]->refresh();
    };
    curveEditor.onBandSelected = [this](int idx)
    {
        curveEditor.setSelectedBand(idx);
    };

    for (auto* column : columns)
        column->onChanged = [this] { curveEditor.repaint(); };
}

void EqPanel::paint(juce::Graphics& g)
{
    g.fillAll(AppLookAndFeel::bg);

    MatrixEffects::drawCardFrame(g, stripCard);

    for (int i = 1; i < columns.size(); ++i)
    {
        const int x = columns[i]->getX() - 3;
        g.setColour(AppLookAndFeel::dimmer);
        g.drawVerticalLine(x, (float) stripCard.getY() + 8.0f, (float) stripCard.getBottom() - 8.0f);
    }

    MatrixEffects::drawScanlines(g, getLocalBounds());
}

void EqPanel::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    stripCard = bounds.removeFromBottom(180);
    curveEditor.setBounds(bounds.reduced(4, 8));

    auto stripArea = stripCard.reduced(10, 8);
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    for (auto* column : columns)
        fb.items.add(juce::FlexItem(*column).withFlex(1.0f).withMargin(2));
    fb.performLayout(stripArea);
}
