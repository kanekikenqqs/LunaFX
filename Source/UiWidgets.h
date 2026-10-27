// =============================================================================
//  Luna FX - lunar UI system: theme, typography, large parameter knobs, the
//  eight-slot XY circle and the instrument rack row. JUCE 8.
// =============================================================================
#pragma once
#include "PluginProcessor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace ui
{
using namespace juce;

// ----------------------------------------------------------------------------- theme
inline std::atomic<int> gTheme { 0 }; // 0 = Night, 1 = Day
inline void setTheme (int t) { gTheme.store (jlimit (0, 1, t)); }
inline bool isDay() { return gTheme.load() == 1; }

inline Colour bgTop()      { return isDay() ? Colour (0xfff7f4ec) : Colour (0xff0b0d12); }
inline Colour bgBottom()   { return isDay() ? Colour (0xffe9e5db) : Colour (0xff05060a); }
inline Colour panel()      { return isDay() ? Colour (0xfff3efe6) : Colour (0xff0f1218); }
inline Colour panelRaised(){ return isDay() ? Colour (0xfffbf8f1) : Colour (0xff141821); }
inline Colour line()       { return isDay() ? Colour (0xffcfc8ba) : Colour (0xff262b36); }
inline Colour lineBright() { return isDay() ? Colour (0xffa9a294) : Colour (0xff3a4050); }
inline Colour text()       { return isDay() ? Colour (0xff1a1c20) : Colour (0xffeeece7); }
inline Colour muted()      { return isDay() ? Colour (0xff6d6e70) : Colour (0xff969aa4); }
inline Colour silver()     { return isDay() ? Colour (0xff565d69) : Colour (0xffcfd3dc); }
inline Colour accent()     { return isDay() ? Colour (0xff4f5866) : Colour (0xffbcc4d3); }
inline Colour accentSoft() { return isDay() ? Colour (0xff7d8796) : Colour (0xff8f9bb3); }
inline Colour warning()    { return isDay() ? Colour (0xff9c4b3d) : Colour (0xffe0836e); }

inline Colour fxColour (int fx)
{
    if (fx < 0 || fx >= djx::kNumFx) return lineBright();
    Colour c = params::catColour (djx::kFxCategory[fx]);
    return isDay() ? c.darker (0.32f) : c;
}

inline String categoryLabel (int cat)
{
    if (cat >= 0 && cat <= 5) return params::kCatNames[cat];
    return "—";
}

// ----------------------------------------------------------------------------- look & feel
class LunaLnF : public LookAndFeel_V4
{
public:
    LunaLnF() { updateTheme(); }

    void updateTheme()
    {
        setColour (ComboBox::backgroundColourId, panelRaised());
        setColour (ComboBox::outlineColourId, line());
        setColour (ComboBox::textColourId, text());
        setColour (ComboBox::arrowColourId, muted());
        setColour (PopupMenu::backgroundColourId, panelRaised());
        setColour (PopupMenu::textColourId, text());
        setColour (PopupMenu::highlightedBackgroundColourId, accent().withAlpha (0.22f));
        setColour (PopupMenu::highlightedTextColourId, text());
        setColour (Label::textColourId, muted());
        setColour (Slider::trackColourId, accent());
        setColour (Slider::thumbColourId, text());
        setColour (Slider::backgroundColourId, line());
        setColour (TextButton::buttonColourId, panelRaised());
        setColour (TextButton::textColourOffId, muted());
        setColour (TextButton::textColourOnId, bgTop());
        setColour (ListBox::backgroundColourId, Colours::transparentBlack);
        setColour (ListBox::textColourId, text());
        setColour (TextEditor::backgroundColourId, panelRaised());
        setColour (TextEditor::textColourId, text());
        setColour (TextEditor::outlineColourId, line());
        setColour (TooltipWindow::backgroundColourId, panelRaised());
        setColour (TooltipWindow::textColourId, text());
        setColour (TooltipWindow::outlineColourId, line());
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, Slider& s) override
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float th = jmax (2.5f, r * 0.085f);
        const float ar = r - th * 0.7f;
        const Colour col = s.findColour (Slider::rotarySliderFillColourId);

        // outer soft track
        Path track;
        track.addCentredArc (c.x, c.y, ar, ar, 0.f, a0, a1, true);
        g.setColour (line());
        g.strokePath (track, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

        const float ang = a0 + jlimit (0.f, 1.f, pos) * (a1 - a0);
        Path value;
        value.addCentredArc (c.x, c.y, ar, ar, 0.f, a0, ang, true);
        g.setColour (col.withAlpha (0.95f));
        g.strokePath (value, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

        // faint glow behind the cap
        g.setGradientFill (ColourGradient (col.withAlpha (0.14f), c.x, c.y,
                                           Colours::transparentBlack, c.x + r, c.y, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);

        const float kr = r * 0.70f;
        g.setColour (isDay() ? Colour (0xffe6e0d4) : Colour (0xff161a22));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
        g.setColour (lineBright());
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.f);

        Path p;
        p.addRoundedRectangle (-1.6f, -kr + 3.f, 3.2f, kr * 0.46f, 1.6f);
        g.setColour (text());
        g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));

        // centre indicator dot
        g.setColour (col.withAlpha (0.65f));
        g.fillEllipse (c.x - 1.6f, c.y - 1.6f, 3.2f, 3.2f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos,
                           float, float, Slider::SliderStyle style, Slider& s) override
    {
        if (style != Slider::LinearHorizontal && style != Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0.f, 0.f, style, s);
            return;
        }
        const float cy = (float) y + (float) h * 0.5f;
        const float th = jmax (3.f, (float) h * 0.22f);
        auto track = Rectangle<float> ((float) x, cy - th * 0.5f, (float) w, th);
        g.setColour (line());
        g.fillRoundedRectangle (track, th * 0.5f);
        const Colour col = s.findColour (Slider::trackColourId);
        const float pw = jlimit (0.f, (float) w, pos - (float) x);
        g.setColour (col.withAlpha (0.85f));
        g.fillRoundedRectangle (track.withWidth (pw), th * 0.5f);
        g.setColour (s.isEnabled() ? text() : muted());
        g.fillRoundedRectangle (pos - 2.5f, cy - (float) h * 0.42f, 5.f, (float) h * 0.84f, 2.5f);
    }

    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box) override
    {
        auto b = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId));
        g.fillRoundedRectangle (b, 5.f);
        g.setColour (line());
        g.drawRoundedRectangle (b, 5.f, 1.f);
        Path p;
        const float ax = (float) w - 14.f, ay = (float) h * 0.5f - 2.f;
        p.addTriangle (ax, ay, ax + 8.f, ay, ax + 4.f, ay + 5.f);
        g.setColour (muted());
        g.fillPath (p);
    }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const Colour base = b.findColour (TextButton::buttonColourId);
        const bool transparent = base.isTransparent() || base.getAlpha() == 0;
        Colour fill = on ? b.findColour (TextButton::buttonOnColourId) : base;
        if (fill.isTransparent()) fill = panelRaised();
        if (over) fill = fill.brighter (isDay() ? 0.02f : 0.10f);
        if (down) fill = fill.darker (0.10f);
        if (! transparent || on || over)
        {
            g.setColour (fill);
            g.fillRoundedRectangle (r, 6.f);
        }
        if (on || over || ! transparent)
        {
            g.setColour (on ? fill.brighter (0.15f) : line());
            g.drawRoundedRectangle (r, 6.f, on ? 1.4f : 1.f);
        }
    }
};

// ----------------------------------------------------------------------------- helpers
class HitButton : public Button
{
public:
    HitButton() : Button ("") { setMouseCursor (MouseCursor::PointingHandCursor); }
    void paintButton (Graphics&, bool, bool) override {}
};

class PowerButton : public Button
{
public:
    PowerButton() : Button ("") { setClickingTogglesState (false); setMouseCursor (MouseCursor::PointingHandCursor); }
    void setAccent (Colour c) { col = c; repaint(); }
    void paintButton (Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f);
        const auto c = b.getCentre();
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f - 1.f;
        const bool on = getToggleState();
        if (on)
        {
            g.setColour (col.withAlpha (0.16f));
            g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);
        }
        g.setColour (on ? col : lineBright());
        g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, over ? 2.f : 1.4f);
        Path p;
        p.addCentredArc (c.x, c.y, r * 0.55f, r * 0.55f, 0.f, 0.7f, MathConstants<float>::twoPi - 0.7f, true);
        p.startNewSubPath (c.x, c.y - r * 0.75f);
        p.lineTo (c.x, c.y - 1.f);
        g.setColour (on ? text() : muted());
        g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }
private:
    Colour col = accent();
};

// ----------------------------------------------------------------------------- parameter knob card
class KnobCard : public Component,
                 public SettableTooltipClient
{
public:
    KnobCard (AudioProcessorValueTreeState& state, const String& id,
              const String& caption, Colour colour, bool compactCard = false)
        : param (state.getParameter (id)), captionText (caption), col (colour), compact (compactCard)
    {
        setMouseCursor (MouseCursor::NormalCursor);
        if (param == nullptr)
        {
            setName ("Unavailable parameter " + id);
            setTooltip (caption + ": parameter missing");
            return;
        }
        setTooltip (caption + " (" + param->getName (48) + ")");

        if (auto* choice = dynamic_cast<AudioParameterChoice*> (param))
        {
            combo = std::make_unique<ComboBox>();
            combo->addItemList (choice->choices, 1);
            combo->setJustificationType (Justification::centredLeft);
            addAndMakeVisible (*combo);
            comboAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *combo);
        }
        else if (dynamic_cast<AudioParameterBool*> (param) != nullptr)
        {
            button = std::make_unique<TextButton> (caption);
            button->setClickingTogglesState (true);
            button->setColour (TextButton::buttonOnColourId, col.withAlpha (0.85f));
            addAndMakeVisible (*button);
            buttonAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (state, id, *button);
        }
        else
        {
            slider = std::make_unique<Slider>();
            slider->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            slider->setRotaryParameters (MathConstants<float>::pi * 1.25f,
                                         MathConstants<float>::pi * 2.75f, true);
            slider->setColour (Slider::rotarySliderFillColourId, col);
            slider->setScrollWheelEnabled (true);
            slider->setPopupDisplayEnabled (true, true, nullptr);
            addAndMakeVisible (*slider);
            sliderAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, id, *slider);
            slider->setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
            slider->textFromValueFunction = [this] (double v)
            {
                return param != nullptr ? param->getText (param->convertTo0to1 ((float) v), 40) : String (v, 2);
            };

            value.setJustificationType (Justification::centred);
            value.setColour (Label::textColourId, col.brighter (isDay() ? 0.f : 0.15f));
            value.setFont (Font (compact ? 9.f : 12.f).boldened());
            value.setEditable (false, true, false);
            value.setTooltip ("Double-click to type a value");
            value.onTextChange = [this] { applyEditedText(); };
            value.onEditorShow = [this] { editingText = true; };
            value.onEditorHide = [this] { editingText = false; tick(); };
            addAndMakeVisible (value);
        }
        tick();
    }

    void setAccent (Colour c)
    {
        col = c;
        if (slider) slider->setColour (Slider::rotarySliderFillColourId, col);
        if (button) button->setColour (TextButton::buttonOnColourId, col.withAlpha (0.85f));
        if (value.isVisible()) value.setColour (Label::textColourId, col.brighter (isDay() ? 0.f : 0.15f));
        repaint();
    }

    RangedAudioParameter* getParam() const noexcept { return param; }

    void tick()
    {
        if (param == nullptr) return;
        if (slider && ! editingText)
        {
            const String t = param->getText (param->getValue(), 40);
            if (t != value.getText())
            {
                updating = true;
                value.setText (t, dontSendNotification);
                updating = false;
            }
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (compact ? 3 : 6);
        const int capH = compact ? 10 : 16;
        const int valH = compact ? 11 : 17;
        if (combo)
        {
            b.removeFromTop (capH);
            combo->setBounds (b.withSizeKeepingCentre (b.getWidth(), jmin (28, b.getHeight())));
        }
        else if (button)
        {
            b.removeFromTop (capH);
            button->setBounds (b.withSizeKeepingCentre (b.getWidth(), jmin (30, b.getHeight())));
        }
        else
        {
            if (! compact) b.removeFromTop (capH);
            b.removeFromBottom (valH);
            const int sz = jmin (b.getWidth(), b.getHeight());
            if (slider) slider->setBounds (b.withSizeKeepingCentre (sz, sz));
            value.setBounds (getLocalBounds().reduced (compact ? 3 : 6).removeFromBottom (valH));
        }
    }

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (panelRaised().withAlpha (isDay() ? 0.72f : 0.82f));
        g.fillRoundedRectangle (b, compact ? 5.f : 9.f);
        g.setColour (line());
        g.drawRoundedRectangle (b, compact ? 5.f : 9.f, 1.f);
        if (param == nullptr)
        {
            g.setColour (warning());
            g.setFont (Font (10.f).boldened());
            g.drawText ("UNAVAILABLE", getLocalBounds().reduced (4), Justification::centred, false);
            return;
        }
        g.setColour (col.withAlpha (0.65f));
        g.fillRoundedRectangle (b.getX() + 6.f, b.getY(), 22.f, 2.f, 1.f);

        const int capH = compact ? 10 : 16;
        g.setColour (muted());
        g.setFont (Font (compact ? 8.f : 10.f).boldened());
        g.drawText (captionText.toUpperCase(),
                    getLocalBounds().reduced (compact ? 3 : 6).removeFromTop (capH),
                    Justification::centred, false);
    }

private:
    void applyEditedText()
    {
        if (updating || param == nullptr || slider == nullptr) return;
        String t = value.getText().trim();
        if (t.isEmpty()) { tick(); return; }
        double v = t.getFloatValue();
        const String unit = param->getLabel();
        if (unit == "Hz" && (t.containsIgnoreCase ("k"))) v *= 1000.0;
        if (unit == "%" && param->getNormalisableRange().start == 0.f
            && param->getNormalisableRange().end == 1.f && v > 1.0) v *= 0.01;
        auto range = param->getNormalisableRange();
        v = (double) range.snapToLegalValue ((float) v);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) v));
        param->endChangeGesture();
        tick();
    }

    RangedAudioParameter* param = nullptr;
    String captionText;
    Colour col;
    bool compact = false;
    bool updating = false;
    bool editingText = false;
    std::unique_ptr<Slider> slider;
    std::unique_ptr<ComboBox> combo;
    std::unique_ptr<TextButton> button;
    Label value;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> buttonAtt;
};

// ----------------------------------------------------------------------------- category bar
class CategoryBar : public Component,
                    public Button::Listener
{
public:
    explicit CategoryBar (Colour accentColour) : col (accentColour)
    {
        for (int i = 0; i < 5; ++i)
        {
            auto* b = new TextButton (params::kVisibleCatNames[i]);
            b->setColour (TextButton::buttonColourId, Colours::transparentBlack);
            b->setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
            b->setColour (TextButton::textColourOffId, muted());
            b->addListener (this);
            tabs.add (b);
            addAndMakeVisible (b);
        }
    }

    void setActive (int cat)
    {
        if (cat == active) return;
        active = cat;
        repaint();
    }
    int getActive() const noexcept { return active; }
    void setUsed (int cat, bool used)
    {
        if (cat >= 0 && cat < 5 && usedCat[cat] != used)
        {
            usedCat[cat] = used;
            repaint();
        }
    }

    std::function<void (int)> onSelect;

    void buttonClicked (Button* b) override
    {
        for (int i = 0; i < tabs.size(); ++i)
            if (tabs[i] == b && onSelect) { onSelect (i); return; }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        const int gap = 6;
        const int w = (b.getWidth() - 4 * gap) / 5;
        for (int i = 0; i < tabs.size(); ++i)
            tabs[i]->setBounds (b.getX() + i * (w + gap), b.getY(), w, b.getHeight());
    }

    void paint (Graphics& g) override
    {
        for (int i = 0; i < tabs.size(); ++i)
        {
            auto r = tabs[i]->getBounds().toFloat().reduced (0.5f);
            const bool on = (i == active);
            const Colour cc = fxColour (i == 0 ? djx::Fx::Harmony
                                               : i == 1 ? djx::Fx::Fuzz
                                               : i == 2 ? djx::Fx::Granular
                                               : i == 3 ? djx::Fx::Chorus
                                               : djx::Fx::Delay);
            if (on)
            {
                g.setColour (cc.withAlpha (0.16f));
                g.fillRoundedRectangle (r, 5.f);
                g.setColour (cc);
                g.drawRoundedRectangle (r, 5.f, 1.2f);
            }
            else if (usedCat[i])
            {
                g.setColour (cc.withAlpha (0.35f));
                g.fillEllipse (r.getRight() - 12.f, r.getY() + 6.f, 3.f, 3.f);
            }
            tabs[i]->setColour (TextButton::textColourOffId, on ? text() : muted());
        }
    }

private:
    Colour col;
    int active = 0;
    std::array<bool, 5> usedCat { false, false, false, false, false };
    OwnedArray<TextButton> tabs;
};

// ----------------------------------------------------------------------------- rack row
class SlotRow : public Component,
                public SettableTooltipClient
{
public:
    SlotRow (LunaFXProcessor& p, int index) : proc (p), slot (index)
    {
        power.setAccent (col);
        addAndMakeVisible (power);
        addAndMakeVisible (nameHit);
        addAndMakeVisible (assignBtn);
        addAndMakeVisible (maxSlider);

        assignBtn.setColour (TextButton::buttonColourId, Colours::transparentBlack);
        assignBtn.setColour (TextButton::textColourOffId, muted());
        assignBtn.setMouseCursor (MouseCursor::PointingHandCursor);

        maxSlider.setSliderStyle (Slider::LinearHorizontal);
        maxSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        maxSlider.setColour (Slider::trackColourId, col);
        maxSlider.setDoubleClickReturnValue (true, 0.55);
        maxSlider.setTooltip ("Maximum mix for this slot. XY proximity scales this value; it is not the live mix.");
        maxSlider.onValueChange = [this]
        {
            if (updating) return;
            const int fx = proc.slotFx (slot);
            if (fx < 0) return;
            if (auto* p = proc.apvts.getParameter (params::amtId (fx)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) maxSlider.getValue()));
                p->endChangeGesture();
            }
        };

        power.onClick = [this]
        {
            const bool next = ! power.getToggleState();
            power.setToggleState (next, dontSendNotification);
            if (auto* p = proc.apvts.getParameter (params::slotEnableId (slot)))
                p->setValueNotifyingHost (next ? 1.f : 0.f);
            if (onSelect) onSelect (slot);
        };
        nameHit.onClick = [this] { if (onSelect) onSelect (slot); };
        assignBtn.onClick = [this] { showAssignMenu(); };

        setTooltip ("Slot " + String (slot + 1) + " - click the name to edit, CHANGE to assign an instrument.");
        refresh();
    }

    void setSelected (bool s) { selected = s; repaint(); }
    void flashPreset() { presetFlash = 1.f; repaint(); }
    void tickFlash (float dt) { if (presetFlash > 0.f) { presetFlash = jmax (0.f, presetFlash - dt * 0.8f); repaint(); } }

    std::function<void (int)> onSelect;

    void refresh()
    {
        const int fx = proc.slotFx (slot);
        const bool enabled = rawValue (params::slotEnableId (slot)) > 0.5f;
        const float live = proc.slotLiveMix (slot);
        const float maxMix = fx >= 0 ? rawValue (params::amtId (fx)) : 0.f;

        updating = true;
        if (! maxSlider.isMouseButtonDown()) maxSlider.setValue (maxMix, dontSendNotification);
        power.setToggleState (enabled, dontSendNotification);
        updating = false;

        const Colour wanted = fx >= 0 ? fxColour (fx) : lineBright();
        if (wanted != col)
        {
            col = wanted;
            power.setAccent (col);
            maxSlider.setColour (Slider::trackColourId, col);
            repaint();
        }
        if (fx != shownFx)
        {
            shownFx = fx;
            const Colour c = fx >= 0 ? fxColour (fx) : lineBright();
            col = c;
            power.setAccent (c);
            maxSlider.setColour (Slider::trackColourId, c);
            maxSlider.setEnabled (fx >= 0);
            assignBtn.setButtonText (fx >= 0 ? "CHANGE" : "ASSIGN");
            repaint();
        }

        if (std::abs (live - lastLive) > 0.0015f || std::abs (maxMix - lastMax) > 0.0015f
            || enabled != lastEnabled)
        {
            lastLive = live; lastMax = maxMix; lastEnabled = enabled;
            repaint();
        }
    }

    void paint (Graphics& g) override
    {
        const int fx = proc.slotFx (slot);
        const bool enabled = rawValue (params::slotEnableId (slot)) > 0.5f;
        const bool assigned = fx >= 0;
        const float live = proc.slotLiveMix (slot);
        const float maxMix = assigned ? rawValue (params::amtId (fx)) : 0.f;
        const Colour c = assigned ? fxColour (fx) : lineBright();
        const float alpha = enabled && assigned ? 1.f : 0.55f;

        auto r = getLocalBounds().toFloat().reduced (1.f, 1.f);
        g.setColour (selected ? c.withAlpha (0.12f + 0.10f * presetFlash)
                              : (assigned ? panel().withAlpha (0.72f) : panel().withAlpha (0.34f)));
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (selected ? c : (presetFlash > 0.f && assigned ? c.withAlpha (0.35f + 0.5f * presetFlash) : line()));
        g.drawRoundedRectangle (r, 5.f, selected ? 1.8f : 1.f);

        if (selected)
        {
            g.setColour (c);
            g.fillRoundedRectangle (r.getX(), r.getY() + 8.f, 2.5f, r.getHeight() - 16.f, 1.2f);
        }

        // name + category
        const String name = assigned ? String (djx::kFxNames[fx]) : String ("Unassigned");
        g.setColour (assigned ? (enabled ? text() : muted()) : muted());
        g.setFont (Font (12.f).boldened());
        g.drawFittedText (name, Rectangle<int> (36, 4, 150, 20), Justification::centredLeft, 1, 0.76f);
        g.setColour (assigned ? c.withAlpha (alpha * 0.9f) : muted().withAlpha (0.6f));
        g.setFont (Font (8.f).boldened());
        g.drawText (assigned ? categoryLabel (djx::kFxCategory[fx]).toUpperCase() : String ("LIBRARY FREE"),
                    Rectangle<int> (36, 23, 150, 12), Justification::centredLeft, false);

        // badge
        auto badge = Rectangle<float> (192.f, 7.f, 38.f, 18.f);
        g.setColour (assigned ? c.withAlpha (0.10f) : line().withAlpha (0.45f));
        g.fillRoundedRectangle (badge, 4.f);
        g.setColour (assigned ? c.withAlpha (0.7f) : line());
        g.drawRoundedRectangle (badge, 4.f, 1.f);
        g.setColour (assigned ? c : muted());
        g.setFont (Font (8.f).boldened());
        g.drawText ("XY " + String (slot + 1), badge, Justification::centred);

        // values above the two meters
        g.setFont (Font (8.f).boldened());
        g.setColour (muted().withAlpha (0.9f));
        g.drawText ("MAX", 36, 30, 30, 11, Justification::centredLeft, false);
        g.drawText (String (juce::roundToInt (maxMix * 100.f)) + "%", 66, 30, 108, 11,
                    Justification::centredRight, false);
        g.drawText ("LIVE", 186, 30, 30, 11, Justification::centredLeft, false);
        g.drawText (String (juce::roundToInt (live * 100.f)) + "%", 216, 30, 108, 11,
                    Justification::centredRight, false);

        // live meter
        auto meter = Rectangle<float> (186.f, 44.f, 140.f, 8.f);
        g.setColour (line());
        g.fillRoundedRectangle (meter, 4.f);
        if (live > 0.0005f)
        {
            g.setColour (c.withAlpha (enabled ? 0.95f : 0.4f));
            g.fillRoundedRectangle (meter.withWidth (jmax (2.f, meter.getWidth() * jlimit (0.f, 1.f, live))), 4.f);
        }
        const float mx = meter.getX() + meter.getWidth() * jlimit (0.f, 1.f, maxMix);
        g.setColour (text().withAlpha (0.65f));
        g.fillRect (mx - 0.5f, meter.getY() - 1.f, 1.f, meter.getHeight() + 2.f);

        if (! enabled && assigned)
        {
            g.setColour (bgTop().withAlpha (0.30f));
            g.fillRoundedRectangle (r, 5.f);
        }
    }

    void resized() override
    {
        power.setBounds (10, 19, 20, 20);
        nameHit.setBounds (34, 4, 152, 36);
        assignBtn.setBounds (238, 7, 68, 20);
        maxSlider.setBounds (36, 43, 140, 10);
    }

    void mouseDown (const MouseEvent&) override
    {
        if (onSelect) onSelect (slot);
    }

private:
    float rawValue (const std::string& id) const
    {
        if (auto* p = proc.apvts.getRawParameterValue (id)) return p->load();
        return 0.f;
    }

    void showAssignMenu()
    {
        const int current = proc.slotFx (slot);
        PopupMenu m;
        m.addItem (1, "None (unassign)", true, current < 0);
        m.addSeparator();
        std::vector<int> idToFx (1, -1);
        for (int cat = 0; cat < 6; ++cat)
        {
            if (cat == 4) continue;
            PopupMenu sub;
            for (int fx = 0; fx < djx::kNumFx; ++fx)
            {
                if (! params::isVisibleFx (fx) || djx::kFxCategory[fx] != cat) continue;
                idToFx.push_back (fx);
                sub.addItem ((int) idToFx.size(), djx::kFxNames[fx], true, fx == current);
            }
            if (sub.getNumItems() > 0)
                m.addSubMenu (params::kCatNames[cat], sub);
        }

        Component::SafePointer<SlotRow> sp (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&assignBtn),
                         [sp, idToFx] (int result)
        {
            if (sp == nullptr || result <= 0) return;
            const int fx = (result == 1) ? -1 : idToFx[(size_t) result - 1];
            auto& p = sp->proc;

            // Enforce one instrument per slot pair: clear a duplicate elsewhere.
            if (fx >= 0)
                for (int s = 0; s < params::kNumSlots; ++s)
                    if (s != sp->slot && p.slotFx (s) == fx)
                        if (auto* q = p.apvts.getParameter (params::slotChoiceId (s)))
                            q->setValueNotifyingHost (q->convertTo0to1 (0.f));

            if (auto* q = p.apvts.getParameter (params::slotChoiceId (sp->slot)))
                q->setValueNotifyingHost (q->convertTo0to1 ((float) (fx >= 0 ? params::choiceForFx (fx) : 0)));
            if (auto* q = p.apvts.getParameter (params::slotEnableId (sp->slot)))
                q->setValueNotifyingHost (1.f);
            if (fx >= 0)
                if (auto* q = p.apvts.getParameter (params::amtId (fx)))
                    if (q->convertFrom0to1 (q->getValue()) < 0.01f)
                        q->setValueNotifyingHost (q->convertTo0to1 (0.5f));

            if (sp->onSelect) sp->onSelect (sp->slot);
            sp->refresh();
            sp->repaint();
        });
    }

    LunaFXProcessor& proc;
    int slot;
    Colour col = accent();
    int shownFx = -2;
    float lastLive = -1.f, lastMax = -1.f;
    bool lastEnabled = false;
    bool selected = false;
    bool updating = false;
    float presetFlash = 0.f;

    PowerButton power;
    HitButton nameHit;
    TextButton assignBtn { "ASSIGN" };
    Slider maxSlider;
};

// ----------------------------------------------------------------------------- XY circle
class LunarCircle : public Component,
                    public SettableTooltipClient
{
public:
    explicit LunarCircle (LunaFXProcessor& p) : proc (p)
    {
        setMouseCursor (MouseCursor::CrosshairCursor);
        setWantsKeyboardFocus (true);
        setName ("XY performance circle");
        setTooltip ("Drag to perform. Distance to a node sets its live mix. Click a node to edit that slot.");
        px = p.apvts.getParameter ("padX");
        py = p.apvts.getParameter ("padY");
        pe = p.apvts.getParameter ("engage");
        pTouch = p.apvts.getParameter ("padTouch");
        pSpring = p.apvts.getParameter ("padSpring");
        pBypass = p.apvts.getParameter ("bypass");
        setDisplayedWeights();
    }

    std::function<void (int)> onSelectSlot;

    void flashPreset() { presetFlash = 1.f; lastPresetRevision = proc.uiPresetRevision.load (std::memory_order_relaxed); repaint(); }

    Rectangle<float> disc() const
    {
        auto b = getLocalBounds().toFloat().reduced (8.f);
        const float s = jmin (b.getWidth(), b.getHeight()) - 24.f;
        return Rectangle<float> (s, s).withCentre (b.getCentre());
    }

    void tick (float dt)
    {
        const unsigned int rev = proc.uiPresetRevision.load (std::memory_order_relaxed);
        if (rev != lastPresetRevision) { lastPresetRevision = rev; presetFlash = 1.f; }

        bool changed = false;
        if (presetFlash > 0.f)
        {
            presetFlash = jmax (0.f, presetFlash - dt * 0.65f);
            changed = true;
        }

        // Smooth the visual proximity independently from the audio smoothing so
        // the glow always eases instead of switching on.
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const auto p = params::clampToDisk (px ? px->getValue() * 2.f - 1.f : 0.f,
                                                py ? py->getValue() * 2.f - 1.f : 0.f);
            const float target = params::slotProximity (p.x, p.y, i);
            const float k = target > disp[i] ? 0.35f : 0.16f;
            const float n = disp[i] + (target - disp[i]) * k;
            if (std::abs (n - disp[i]) > 0.0004f) { disp[i] = n; changed = true; }
        }

        if (springing)
        {
            const float x = px->getValue(), y = py->getValue();
            const float nx = x + (0.5f - x) * 0.22f;
            const float ny = y + (0.5f - y) * 0.22f;
            if (std::abs (nx - 0.5f) < 0.003f && std::abs (ny - 0.5f) < 0.003f)
            {
                px->setValueNotifyingHost (0.5f);
                py->setValueNotifyingHost (0.5f);
                springing = false;
                closeGestures();
            }
            else
            {
                px->setValueNotifyingHost (nx);
                py->setValueNotifyingHost (ny);
            }
            changed = true;
        }
        if (changed) repaint();
    }

    void paint (Graphics& g) override
    {
        auto d = disc();
        const auto c = d.getCentre();
        const float R = d.getWidth() * 0.5f;
        const bool byp = pBypass != nullptr && pBypass->getValue() > 0.5f;

        // soft shadow and disc
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (d.translated (0.f, 8.f));
        g.setGradientFill (ColourGradient (isDay() ? Colour (0xfffaf7ef) : Colour (0xff1b202b), c.x, c.y - R,
                                           isDay() ? Colour (0xffded8cb) : Colour (0xff080a10), c.x, c.y + R, false));
        g.fillEllipse (d);

        {
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addEllipse (d);
            g.reduceClipRegion (clip);

            // lunar maria (vector, smooth - no pixel texture)
            g.setColour ((isDay() ? Colours::white : Colours::white).withAlpha (isDay() ? 0.10f : 0.035f));
            g.fillEllipse (c.x - R * 0.72f, c.y - R * 0.55f, R * 0.72f, R * 0.42f);
            g.setColour (Colours::white.withAlpha (isDay() ? 0.06f : 0.025f));
            g.fillEllipse (c.x + R * 0.05f, c.y - R * 0.10f, R * 0.55f, R * 0.38f);
            g.fillEllipse (c.x - R * 0.15f, c.y + R * 0.42f, R * 0.48f, R * 0.27f);

            // terminator shading
            g.setGradientFill (ColourGradient (Colours::transparentBlack, c.x - R * 0.4f, c.y,
                                               (isDay() ? Colours::black : Colours::black).withAlpha (isDay() ? 0.08f : 0.42f),
                                               c.x + R, c.y + R * 0.25f, true));
            g.fillEllipse (d);
        }

        g.setColour (lineBright());
        g.drawEllipse (d.reduced (1.f), 1.6f);
        g.setColour (line());
        g.drawEllipse (d.reduced (9.f), 1.f);

        // rim ticks
        for (int i = 0; i < 72; ++i)
        {
            const float a = (float) i * MathConstants<float>::twoPi / 72.f - MathConstants<float>::halfPi;
            const bool major = (i % 9) == 0;
            const float inner = R - (major ? 18.f : 11.f);
            const float outer = R - 7.f;
            g.setColour ((major ? lineBright() : line()).withAlpha (major ? 0.9f : 0.55f));
            g.drawLine (c.x + std::cos (a) * inner, c.y + std::sin (a) * inner,
                        c.x + std::cos (a) * outer, c.y + std::sin (a) * outer, major ? 1.5f : 1.f);
        }

        // faint orbit rings
        g.setColour (line().withAlpha (0.5f));
        g.drawEllipse (c.x - R * 0.55f, c.y - R * 0.55f, R * 1.10f, R * 1.10f, 1.f);
        g.drawEllipse (c.x - R * 0.20f, c.y - R * 0.20f, R * 0.40f, R * 0.40f, 0.8f);

        const auto puck = puckPos();
        const bool enabled = ! byp;

        // connections first
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const int fx = proc.slotFx (i);
            if (! params::isVisibleFx (fx)) continue;
            const bool on = rawBool (params::slotEnableId (i));
            const auto n = nodePos (i, c, R);
            const float w = disp[i];
            if (on && w > 0.02f)
            {
                g.setColour (fxColour (fx).withAlpha (0.05f + 0.30f * w));
                g.drawLine (puck.x, puck.y, n.x, n.y, 0.8f + 1.8f * w);
            }
        }

        for (int i = 0; i < params::kNumSlots; ++i)
            drawNode (g, i, c, R, byp, enabled);

        // puck
        const float pr = 15.f;
        if (enabled)
        {
            g.setGradientFill (ColourGradient (accent().withAlpha (0.22f + 0.12f * proc.uiEngage.load()), puck.x, puck.y,
                                               Colours::transparentBlack, puck.x + 55.f, puck.y, true));
            g.fillEllipse (puck.x - 55.f, puck.y - 55.f, 110.f, 110.f);
        }
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (puck.x - pr, puck.y - pr + 3.f, pr * 2.f, pr * 2.f);
        g.setColour (isDay() ? Colour (0xffeee8dc) : Colour (0xff0b0d12));
        g.fillEllipse (puck.x - pr, puck.y - pr, pr * 2.f, pr * 2.f);
        g.setColour (dragging ? accent() : text());
        g.drawEllipse (puck.x - pr, puck.y - pr, pr * 2.f, pr * 2.f, 2.2f);
        g.setColour (text());
        g.fillEllipse (puck.x - 3.f, puck.y - 3.f, 6.f, 6.f);
        g.setColour (muted());
        g.setFont (Font (9.f).boldened());
        g.drawText ("XY", (int) (puck.x - 12.f), (int) (puck.y + 19.f), 24, 12, Justification::centred, false);

        if (byp)
        {
            g.setColour (warning());
            g.setFont (Font (20.f).boldened());
            g.drawText ("BYPASSED", d.withSizeKeepingCentre (280.f, 30.f).translated (0.f, R * 0.58f), Justification::centred);
        }

        // legend
        g.setColour (muted().withAlpha (0.85f));
        g.setFont (Font (9.f).boldened());
        g.drawText ("GLOW = LIVE PROXIMITY", (int) d.getX(), (int) getHeight() - 20, (int) d.getWidth() / 2, 14,
                    Justification::centredLeft, false);
        g.setColour (silver().withAlpha (0.85f));
        g.drawText ("SILVER RING = PRESET ASSIGNED", (int) d.getCentreX(), (int) getHeight() - 20,
                    (int) d.getWidth() / 2, 14, Justification::centredRight, false);

        if (hasKeyboardFocus (true))
        {
            g.setColour (accent().withAlpha (0.55f));
            g.drawEllipse (d.reduced (-4.f), 1.2f);
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus();
        stopSpring();
        nodeSelecting = false;

        auto d = disc();
        const auto c = d.getCentre();
        const float R = d.getWidth() * 0.5f;
        for (int i = 0; i < params::kNumSlots; ++i)
        {
            const auto n = nodePos (i, c, R);
            if (e.position.getDistanceFrom (n) <= 22.f)
            {
                if (onSelectSlot) onSelectSlot (i);
                nodeSelecting = true;
                repaint();
                return;
            }
        }

        if (px == nullptr || py == nullptr) return;
        px->beginChangeGesture();
        py->beginChangeGesture();
        gestureOpen = true;
        setFromMouse (e.position);
        if (pTouch != nullptr && pe != nullptr && pTouch->getValue() > 0.5f && pe->getValue() < 0.5f)
        {
            touchEngaged = true;
            pe->beginChangeGesture();
            pe->setValueNotifyingHost (1.f);
        }
        dragging = true;
        repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (nodeSelecting) return;
        setFromMouse (e.position);
    }

    void mouseUp (const MouseEvent&) override
    {
        dragging = false;
        if (nodeSelecting) { nodeSelecting = false; return; }
        if (touchEngaged && pe != nullptr)
        {
            pe->setValueNotifyingHost (0.f);
            pe->endChangeGesture();
            touchEngaged = false;
        }
        if (pSpring != nullptr && pSpring->getValue() > 0.5f && ! nodeSelecting)
            springing = true;
        else
            closeGestures();
        repaint();
    }

    void mouseDoubleClick (const MouseEvent&) override
    {
        if (px == nullptr || py == nullptr) return;
        stopSpring();
        px->beginChangeGesture(); py->beginChangeGesture();
        px->setValueNotifyingHost (0.5f); py->setValueNotifyingHost (0.5f);
        px->endChangeGesture(); py->endChangeGesture();
    }

    void mouseMove (const MouseEvent& e) override
    {
        int hover = -1;
        auto d = disc();
        for (int i = 0; i < params::kNumSlots; ++i)
            if (e.position.getDistanceFrom (nodePos (i, d.getCentre(), d.getWidth() * 0.5f)) <= 22.f) hover = i;
        if (hover != hoverSlot) { hoverSlot = hover; repaint(); }
    }
    void mouseExit (const MouseEvent&) override { if (hoverSlot >= 0) { hoverSlot = -1; repaint(); } }

    bool keyPressed (const KeyPress& key) override
    {
        if (px == nullptr || py == nullptr) return false;
        const float step = key.getModifiers().isShiftDown() ? 0.004f : 0.02f;
        float x = px->getValue(), y = py->getValue();
        if (key == KeyPress (KeyPress::leftKey)) x -= step;
        else if (key == KeyPress (KeyPress::rightKey)) x += step;
        else if (key == KeyPress (KeyPress::upKey)) y += step;
        else if (key == KeyPress (KeyPress::downKey)) y -= step;
        else if (key == KeyPress (KeyPress::homeKey)) { x = 0.5f; y = 0.5f; }
        else return false;
        const auto p = params::clampToDisk (x * 2.f - 1.f, y * 2.f - 1.f);
        px->beginChangeGesture(); py->beginChangeGesture();
        px->setValueNotifyingHost (0.5f + p.x * 0.5f);
        py->setValueNotifyingHost (0.5f + p.y * 0.5f);
        px->endChangeGesture(); py->endChangeGesture();
        repaint();
        return true;
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    Point<float> nodePos (int slot, Point<float> c, float R) const
    {
        const auto p = params::slotPosition (slot);
        return { c.x + p.x * R, c.y - p.y * R };
    }

    Point<float> puckPos() const
    {
        auto d = disc();
        const float R = d.getWidth() * 0.5f;
        const float x = px != nullptr ? px->getValue() : 0.5f;
        const float y = py != nullptr ? py->getValue() : 0.5f;
        return { d.getCentreX() + (x - 0.5f) * 2.f * R, d.getCentreY() - (y - 0.5f) * 2.f * R };
    }

    void setFromMouse (Point<float> pos)
    {
        auto d = disc();
        const float R = d.getWidth() * 0.5f;
        const float dx = (pos.x - d.getCentreX()) / R;
        const float dy = (d.getCentreY() - pos.y) / R;
        const auto p = params::clampToDisk (dx, dy);
        if (px != nullptr) px->setValueNotifyingHost (0.5f + p.x * 0.5f);
        if (py != nullptr) py->setValueNotifyingHost (0.5f + p.y * 0.5f);
        repaint();
    }

    bool rawBool (const std::string& id) const
    {
        if (auto* p = proc.apvts.getRawParameterValue (id)) return p->load() > 0.5f;
        return false;
    }

    float rawValue (const std::string& id) const
    {
        if (auto* p = proc.apvts.getRawParameterValue (id)) return p->load();
        return 0.f;
    }

    void drawNode (Graphics& g, int slot, Point<float> c, float R, bool byp, bool enabled)
    {
        const auto n = nodePos (slot, c, R);
        const int fx = proc.slotFx (slot);
        const bool assigned = params::isVisibleFx (fx);
        const bool on = assigned && rawBool (params::slotEnableId (slot)) && enabled;
        const float w = assigned ? disp[slot] : 0.f;
        const Colour col = assigned ? fxColour (fx) : lineBright();
        const bool selected = slot == selectedSlot;
        const float nodeR = 9.f;

        if (on && w > 0.01f)
        {
            g.setGradientFill (ColourGradient (col.withAlpha (0.34f * w), n.x, n.y,
                                               Colours::transparentBlack, n.x + 34.f, n.y + 34.f, true));
            g.fillEllipse (n.x - 34.f, n.y - 34.f, 68.f, 68.f);
        }

        if (presetFlash > 0.f && on)
        {
            const float pulse = presetFlash * (0.55f + 0.45f * std::sin (presetFlash * 12.f));
            g.setColour (silver().withAlpha (0.45f * pulse));
            g.drawEllipse (n.x - nodeR - 7.f, n.y - nodeR - 7.f,
                           (nodeR + 7.f) * 2.f, (nodeR + 7.f) * 2.f, 1.4f);
        }

        // base disc
        g.setColour (isDay() ? Colour (0xffeee9df) : Colour (0xff0b0d12));
        g.fillEllipse (n.x - nodeR - 3.f, n.y - nodeR - 3.f, (nodeR + 3.f) * 2.f, (nodeR + 3.f) * 2.f);
        g.setColour (on ? col.withAlpha (0.30f + 0.65f * w) : (assigned ? lineBright() : line()));
        g.fillEllipse (n.x - nodeR, n.y - nodeR, nodeR * 2.f, nodeR * 2.f);
        g.setColour (on ? col.brighter (0.2f) : line());
        g.drawEllipse (n.x - nodeR, n.y - nodeR, nodeR * 2.f, nodeR * 2.f, 1.2f);

        // inner dot / disabled slash
        if (on)
        {
            g.setColour (text().withAlpha (0.5f + 0.5f * w));
            g.fillEllipse (n.x - 2.2f, n.y - 2.2f, 4.4f, 4.4f);
        }
        else if (assigned)
        {
            g.setColour (muted().withAlpha (0.7f));
            g.drawLine (n.x - 3.f, n.y + 3.f, n.x + 3.f, n.y - 3.f, 1.2f);
        }

        if (selected)
        {
            g.setColour (text().withAlpha (0.9f));
            g.drawEllipse (n.x - nodeR - 5.f, n.y - nodeR - 5.f,
                           (nodeR + 5.f) * 2.f, (nodeR + 5.f) * 2.f, 1.6f);
        }
        if (hoverSlot == slot)
        {
            g.setColour (text().withAlpha (0.35f));
            g.drawEllipse (n.x - nodeR - 9.f, n.y - nodeR - 9.f,
                           (nodeR + 9.f) * 2.f, (nodeR + 9.f) * 2.f, 1.f);
        }

        // label inside the rim, never clipped by the editor edge
        Rectangle<int> box;
        const float nameW = 104.f;
        if (n.x >= c.x)
            box = Rectangle<int> ((int) (n.x - 16.f - nameW), (int) (n.y - 8.f), (int) nameW, 17);
        else
            box = Rectangle<int> ((int) (n.x + 16.f), (int) (n.y - 8.f), (int) nameW, 17);
        const String label = assigned ? String (djx::kFxNames[fx]) : String ("Empty");
        g.setColour (on ? text() : (assigned ? muted() : muted().withAlpha (0.65f)));
        g.setFont (Font (10.f).boldened());
        g.drawFittedText (label, box, n.x >= c.x ? Justification::centredRight : Justification::centredLeft,
                          1, 0.72f);

        if (assigned)
        {
            Rectangle<int> sub (box.getX(), box.getBottom() - 1, box.getWidth(), 12);
            g.setColour (on ? col.withAlpha (0.75f) : muted().withAlpha (0.55f));
            g.setFont (Font (8.f));
            g.drawText (on ? String (juce::roundToInt (w * 100.f)) + "% LIVE"
                           : String ("DISABLED"),
                        sub, n.x >= c.x ? Justification::centredRight : Justification::centredLeft, false);
        }
    }

    void closeGestures()
    {
        if (gestureOpen && px != nullptr && py != nullptr)
        {
            px->endChangeGesture();
            py->endChangeGesture();
            gestureOpen = false;
        }
    }
    void stopSpring() { springing = false; closeGestures(); }
    void setDisplayedWeights()
    {
        for (int i = 0; i < params::kNumSlots; ++i)
            disp[i] = params::slotProximity (px ? px->getValue() * 2.f - 1.f : 0.f,
                                             py ? py->getValue() * 2.f - 1.f : 0.f, i);
    }

    LunaFXProcessor& proc;
    RangedAudioParameter* px = nullptr;
    RangedAudioParameter* py = nullptr;
    RangedAudioParameter* pe = nullptr;
    RangedAudioParameter* pTouch = nullptr;
    RangedAudioParameter* pSpring = nullptr;
    RangedAudioParameter* pBypass = nullptr;

    std::array<float, params::kNumSlots> disp {};
    int selectedSlot = -1;
public:
    void setSelectedSlot (int s) { selectedSlot = s; repaint(); }
private:
    int hoverSlot = -1;
    bool dragging = false, springing = false, gestureOpen = false, touchEngaged = false, nodeSelecting = false;
    float presetFlash = 0.f;
    unsigned int lastPresetRevision = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunarCircle)
};

// ----------------------------------------------------------------------------- performance controls
class AccentButton : public Component,
                     public SettableTooltipClient
{
public:
    explicit AccentButton (LunaFXProcessor& p) : proc (p)
    {
        pe = p.apvts.getParameter ("engage");
        pm = p.apvts.getParameter ("engMode");
        setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip ("Accent: latch or momentary. Click the HOLD/LATCH label to change the mode.");
        setName ("Accent");
    }
    std::function<void()> onModeChanged;
    void paint (Graphics& g) override
    {
        auto d = getLocalBounds().toFloat();
        const float footer = 18.f;
        d.removeFromBottom (footer);
        const float s = jmin (d.getWidth(), d.getHeight());
        auto c = d.withSizeKeepingCentre (s, s).reduced (6.f);
        const float e = proc.uiEngage.load();
        if (e > 0.01f)
        {
            g.setGradientFill (ColourGradient (accent().withAlpha (0.22f * e), c.getCentreX(), c.getCentreY(),
                                               Colours::transparentBlack, c.getCentreX() + s * 0.8f, c.getCentreY(), true));
            g.fillEllipse (c.expanded (s * 0.2f));
        }
        g.setColour (panelRaised());
        g.fillEllipse (c);
        g.setColour (e > 0.5f ? accent() : lineBright());
        g.drawEllipse (c, e > 0.5f ? 3.f : 1.6f);
        g.setColour (e > 0.5f ? text() : muted());
        g.setFont (Font (13.f).boldened());
        g.drawText ("ACCENT", c, Justification::centred);

        g.setColour (muted());
        g.setFont (Font (9.f).boldened());
        g.drawText (pm != nullptr && pm->getValue() > 0.5f ? "HOLD" : "LATCH",
                    getLocalBounds().removeFromBottom (18), Justification::centred);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.position.y > (float) getHeight() - 18.f)
        {
            if (pm != nullptr)
            {
                pm->beginChangeGesture();
                pm->setValueNotifyingHost (pm->getValue() > 0.5f ? 0.f : 1.f);
                pm->endChangeGesture();
            }
            repaint();
            return;
        }
        if (pe == nullptr) return;
        if (pm != nullptr && pm->getValue() > 0.5f)
        {
            pe->beginChangeGesture();
            pe->setValueNotifyingHost (1.f);
            held = true;
        }
        else
        {
            pe->beginChangeGesture();
            pe->setValueNotifyingHost (pe->getValue() > 0.5f ? 0.f : 1.f);
            pe->endChangeGesture();
        }
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (held && pe != nullptr) { pe->setValueNotifyingHost (0.f); pe->endChangeGesture(); held = false; repaint(); }
    }
private:
    LunaFXProcessor& proc;
    RangedAudioParameter* pe = nullptr;
    RangedAudioParameter* pm = nullptr;
    bool held = false;
};

class KillButton : public Component,
                   public SettableTooltipClient
{
public:
    explicit KillButton (LunaFXProcessor& p) : proc (p), param (p.apvts.getParameter ("kill"))
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip ("Panic: clears delay, granular and reverb tails. Does not change your parameters.");
        setName ("Kill switch");
    }
    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.f);
        const double since = Time::getMillisecondCounterHiRes() - lastHit;
        const bool flash = since < 700.0;
        const Colour c = flash ? warning() : panelRaised();
        g.setColour (c);
        g.fillRoundedRectangle (b, 6.f);
        g.setColour (flash ? warning() : lineBright());
        g.drawRoundedRectangle (b, 6.f, 1.2f);
        g.setColour (flash ? bgTop() : text());
        g.setFont (Font (11.f).boldened());
        g.drawText (flash ? "CLEARED" : "KILL", b, Justification::centred);
    }
    void mouseDown (const MouseEvent&) override
    {
        lastHit = Time::getMillisecondCounterHiRes();
        proc.requestKill();
        if (param != nullptr)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (1.f);
        }
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (param != nullptr)
        {
            param->setValueNotifyingHost (0.f);
            param->endChangeGesture();
        }
        repaint();
    }
private:
    LunaFXProcessor& proc;
    RangedAudioParameter* param = nullptr;
    double lastHit = -10000.0;
};

class BypassButton : public Component,
                     public SettableTooltipClient
{
public:
    explicit BypassButton (LunaFXProcessor& p) : pb (p.apvts.getParameter ("bypass"))
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip ("Bypass the whole plug-in.");
        setName ("Bypass");
    }
    void paint (Graphics& g) override
    {
        const bool byp = pb != nullptr && pb->getValue() > 0.5f;
        auto b = getLocalBounds().toFloat().reduced (1.f);
        g.setColour (byp ? warning() : panelRaised());
        g.fillRoundedRectangle (b, 6.f);
        g.setColour (byp ? warning() : lineBright());
        g.drawRoundedRectangle (b, 6.f, 1.2f);
        const auto c = Point<float> (b.getX() + 18.f, b.getCentreY());
        Path p;
        p.addCentredArc (c.x, c.y, 6.f, 6.f, 0.f, 0.7f, MathConstants<float>::twoPi - 0.7f, true);
        p.startNewSubPath (c.x, c.y - 8.f);
        p.lineTo (c.x, c.y - 1.f);
        g.setColour (byp ? bgTop() : accent());
        g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (byp ? bgTop() : text());
        g.setFont (Font (11.f).boldened());
        g.drawText (byp ? "BYPASSED" : "BYPASS", b.withTrimmedLeft (32.f), Justification::centredLeft);
    }
    void mouseDown (const MouseEvent&) override
    {
        if (pb == nullptr) return;
        pb->beginChangeGesture();
        pb->setValueNotifyingHost (pb->getValue() > 0.5f ? 0.f : 1.f);
        pb->endChangeGesture();
        repaint();
    }
private:
    RangedAudioParameter* pb = nullptr;
};

class ParamToggle : public TextButton
{
public:
    ParamToggle (AudioProcessorValueTreeState& state, const String& id, const String& label)
        : TextButton (label)
    {
        setClickingTogglesState (true);
        setColour (TextButton::buttonColourId, Colours::transparentBlack);
        setColour (TextButton::buttonOnColourId, accent().withAlpha (0.22f));
        attach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (state, id, *this);
        setMouseCursor (MouseCursor::PointingHandCursor);
    }
private:
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> attach;
};

} // namespace ui
