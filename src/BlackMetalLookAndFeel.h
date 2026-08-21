// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Andrea De Murtas

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace theme
{
    inline const juce::Colour background  { 0xff0b1116 }; // deep night
    inline const juce::Colour panel       { 0xff151d23 }; // blue-grey slate
    inline const juce::Colour outline     { 0xff48443b }; // muted bronze
    inline const juce::Colour bone        { 0xffe7decc }; // warm ivory
    inline const juce::Colour boneDim     { 0xff9f998d }; // weathered inscription
    inline const juce::Colour blood       { 0xffa66b2b }; // deep amber
    inline const juce::Colour bloodBright { 0xffd5a653 }; // returning sunlight
    inline const juce::Colour iron        { 0xff1b252c }; // carved stone
}

class BlackMetalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlackMetalLookAndFeel()
        : titleTypeface(juce::Typeface::createSystemTypefaceFor(
              BinaryData::IMFellEnglish_ttf, BinaryData::IMFellEnglish_ttfSize)),
          bodyTypeface(juce::Typeface::createSystemTypefaceFor(
              BinaryData::IMFellEnglish_ttf, BinaryData::IMFellEnglish_ttfSize))
    {
        setColour(juce::Label::textColourId, theme::bone);
        setColour(juce::Slider::textBoxTextColourId, theme::bone);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, theme::blood.withAlpha(0.4f));
        setColour(juce::Slider::trackColourId, theme::blood);
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff28343c));
        setColour(juce::Slider::thumbColourId, theme::bone);
        setColour(juce::BubbleComponent::backgroundColourId, theme::panel);
        setColour(juce::BubbleComponent::outlineColourId, theme::outline);
        setColour(juce::ComboBox::backgroundColourId, theme::iron);
        setColour(juce::ComboBox::textColourId, theme::bone);
        setColour(juce::ComboBox::outlineColourId, theme::outline);
        setColour(juce::ComboBox::arrowColourId, theme::bloodBright);
        setColour(juce::PopupMenu::backgroundColourId, theme::panel);
        setColour(juce::PopupMenu::textColourId, theme::bone);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::blood.withAlpha(0.4f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::textColourId, theme::bone);
        setColour(juce::TextEditor::highlightColourId, theme::blood.withAlpha(0.4f));
        setColour(juce::CaretComponent::caretColourId, theme::bloodBright);
        setColour(juce::TextButton::buttonColourId, theme::iron);
        setColour(juce::TextButton::buttonOnColourId, theme::blood.withAlpha(0.6f));
        setColour(juce::TextButton::textColourOffId, theme::bone);
        setColour(juce::TextButton::textColourOnId, theme::bone);
        setColour(juce::TooltipWindow::backgroundColourId, theme::panel);
        setColour(juce::TooltipWindow::textColourId, theme::bone);
        setColour(juce::TooltipWindow::outlineColourId, theme::outline);
    }

    // Set by the editor when the window is rescaled.
    float uiScale = 1.0f;

    juce::Font getTitleFont(float height) const
    {
        return juce::Font(juce::FontOptions(titleTypeface).withHeight(height));
    }

    juce::Font getBodyFont(float height) const
    {
        return juce::Font(juce::FontOptions(bodyTypeface).withHeight(height));
    }

    juce::Font getLabelFont(juce::Label&) override        { return getBodyFont(15.0f * uiScale); }
    juce::Font getComboBoxFont(juce::ComboBox&) override  { return getBodyFont(16.0f * uiScale); }
    juce::Font getPopupMenuFont() override                { return getBodyFont(17.0f * uiScale); }

    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return getBodyFont(15.0f * uiScale);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                          const juce::Colour&, bool highlighted, bool down) override
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float corner = 2.5f * uiScale;

    const auto resting = button.getToggleState()
                           ? button.findColour(juce::TextButton::buttonOnColourId)
                           : theme::iron;

    auto top = resting.brighter(highlighted ? 0.24f : 0.10f);
    auto bottom = resting.darker(down ? 0.02f : 0.16f);

    if (down)
        top = theme::blood.withAlpha(0.58f);

    juce::ColourGradient stoneButton(
        top, bounds.getX(), bounds.getY(),
        bottom, bounds.getX(), bounds.getBottom(), false);

    g.setGradientFill(stoneButton);
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(button.getToggleState()
                    ? theme::bloodBright.withAlpha(0.82f)
                    : theme::outline.withAlpha(highlighted ? 0.92f : 0.68f));
    g.drawRoundedRectangle(bounds, corner, 1.0f * uiScale);

    g.setColour(theme::bone.withAlpha(highlighted ? 0.18f : 0.08f));
    g.drawLine(bounds.getX() + 3.0f * uiScale,
               bounds.getY() + 1.5f * uiScale,
               bounds.getRight() - 3.0f * uiScale,
               bounds.getY() + 1.5f * uiScale,
               1.0f * uiScale);
}

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                      float sliderPos, float minSliderPos, float maxSliderPos,
                      juce::Slider::SliderStyle style, juce::Slider& slider) override
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider(
            g, x, y, width, height, sliderPos,
            minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const float startX = (float) x + 5.0f * uiScale;
    const float endX = (float) (x + width) - 5.0f * uiScale;
    const float centreY = (float) y + (float) height * 0.5f;
    const float valueX = juce::jlimit(startX, endX, sliderPos);

    // Recessed stone track.
    g.setColour(juce::Colour(0xff28343c));
    g.drawLine(startX, centreY, endX, centreY, 4.0f * uiScale);

    // Warm orbital value trail.
    g.setColour(theme::blood.withAlpha(0.28f));
    g.drawLine(startX, centreY, valueX, centreY, 7.0f * uiScale);
    g.setColour(theme::bloodBright.withAlpha(0.9f));
    g.drawLine(startX, centreY, valueX, centreY, 2.0f * uiScale);

    // Ivory celestial node.
    const float nodeRadius = 5.0f * uiScale;
    g.setColour(theme::bone);
    g.fillEllipse(valueX - nodeRadius, centreY - nodeRadius,
                  nodeRadius * 2.0f, nodeRadius * 2.0f);
    g.setColour(theme::bloodBright.withAlpha(0.75f));
    g.drawEllipse(valueX - nodeRadius, centreY - nodeRadius,
                  nodeRadius * 2.0f, nodeRadius * 2.0f,
                  1.0f * uiScale);
}
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(6.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centre = bounds.getCentre();
        auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto arcRadius = radius - 2.0f;

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff28343c));
        g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));

        // value arc: wide translucent pass first for an ember-like glow
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, angle, true);
        g.setColour(theme::blood.withAlpha(0.35f));
        g.strokePath(value, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved));
        g.setColour(theme::bloodBright);
        g.strokePath(value, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved));

        auto knobRadius = radius * 0.68f;
        auto knobArea = juce::Rectangle<float>(knobRadius * 2.0f, knobRadius * 2.0f).withCentre(centre);
        juce::ColourGradient body(theme::iron.brighter(0.25f), centre.x, centre.y - knobRadius,
                                  juce::Colour(0xff090e12), centre.x, centre.y + knobRadius, false);
        g.setGradientFill(body);
        g.fillEllipse(knobArea);
        g.setColour(theme::outline);
        g.drawEllipse(knobArea, 1.5f);
        // Fine inner orbit and central ember.
        auto innerOrbit = knobArea.reduced(knobRadius * 0.22f);
        g.setColour(theme::boneDim.withAlpha(0.16f));
        g.drawEllipse(innerOrbit, 1.0f);

        const float emberRadius = 1.5f + sliderPos * 0.8f;
        g.setColour(theme::bloodBright.withAlpha(0.22f + sliderPos * 0.28f));
        g.fillEllipse(centre.x - emberRadius, centre.y - emberRadius,
              emberRadius * 2.0f, emberRadius * 2.0f);

        juce::Path pointer;
        pointer.startNewSubPath(0.0f, -(knobRadius - 3.0f));
        pointer.lineTo(-2.2f, -knobRadius * 0.18f);
        pointer.lineTo(0.0f, knobRadius * 0.10f);
        pointer.lineTo(2.2f, -knobRadius * 0.18f);
        pointer.closeSubPath();
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        g.setColour(theme::bone);
        g.fillPath(pointer);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                  int, int, int, int, juce::ComboBox& box) override
{
    juce::Rectangle<int> bounds(0, 0, width, height);
    auto face = bounds.toFloat().reduced(0.5f);
    const float corner = 2.0f * uiScale;

    juce::ColourGradient stoneBox(
        theme::iron.brighter(0.10f),
        face.getX(), face.getY(),
        theme::iron.darker(0.16f),
        face.getX(), face.getBottom(), false);

    g.setGradientFill(stoneBox);
    g.fillRoundedRectangle(face, corner);

    g.setColour(box.hasKeyboardFocus(true)
                    ? theme::bloodBright.withAlpha(0.82f)
                    : theme::outline.withAlpha(0.72f));
    g.drawRoundedRectangle(face, corner, 1.0f * uiScale);

    g.setColour(theme::bone.withAlpha(0.07f));
    g.drawLine(face.getX() + 3.0f * uiScale,
               face.getY() + 1.5f * uiScale,
               face.getRight() - 3.0f * uiScale,
               face.getY() + 1.5f * uiScale,
               1.0f * uiScale);

    auto arrowZone = bounds.removeFromRight(26).toFloat();
    juce::Path arrow;
    arrow.addTriangle(arrowZone.getCentreX() - 5.0f, arrowZone.getCentreY() - 2.5f,
                      arrowZone.getCentreX() + 5.0f, arrowZone.getCentreY() - 2.5f,
                      arrowZone.getCentreX(), arrowZone.getCentreY() + 4.5f);

    g.setColour(findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

private:
    juce::Typeface::Ptr titleTypeface, bodyTypeface;
};
