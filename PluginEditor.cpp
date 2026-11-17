#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace juce;

namespace
{
    constexpr float topBar = 36.0f;

    Colour hsv (float h, float s, float v, float a = 1.0f)
    {
        h -= std::floor (h);
        return Colour::fromHSV (h, s, v, a);
    }

    void glowStroke (Graphics& g, const Path& p, Colour c, float width)
    {
        g.setColour (c.withMultipliedAlpha (0.10f));
        g.strokePath (p, PathStrokeType (width * 5.0f, PathStrokeType::curved));
        g.setColour (c.withMultipliedAlpha (0.25f));
        g.strokePath (p, PathStrokeType (width * 2.5f, PathStrokeType::curved));
        g.setColour (c);
        g.strokePath (p, PathStrokeType (width, PathStrokeType::curved));
    }

    void glowLine (Graphics& g, Line<float> l, Colour c, float width)
    {
        g.setColour (c.withMultipliedAlpha (0.12f));
        g.drawLine (l, width * 5.0f);
        g.setColour (c.withMultipliedAlpha (0.28f));
        g.drawLine (l, width * 2.5f);
        g.setColour (c);
        g.drawLine (l, width);
    }
}

//==============================================================================
SynthVisualizerEditor::SynthVisualizerEditor (SynthVisualizerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    modeBox.addItemList (StringArray { "Synthwave", "Spectrum", "Oscilloscope", "Radial", "Particles" }, 1);
    addAndMakeVisible (modeBox);
    themeButton.setTooltip ("Theme Settings");
    
    themeButton.setColour (
        TextButton::buttonColourId,
        Colour (0xff120a24));
    
    themeButton.setColour (
        TextButton::buttonOnColourId,
        Colour (0xff252044));
    
    themeButton.setColour (
        TextButton::textColourOffId,
        Colour (0xff00e5ff));
    
    themeButton.setColour (
        TextButton::textColourOnId,
        Colours::white);
    
    themeButton.onClick = [this]
    {
        openThemeMenu();
    };
    
    addAndMakeVisible (themeButton);

    gainSlider.setSliderStyle (Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (Slider::TextBoxRight, false, 52, 22);
    addAndMakeVisible (gainSlider);

    gainLabel.setText ("Sensitivity", dontSendNotification);
    gainLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.8f));
    addAndMakeVisible (gainLabel);

    modeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mode", modeBox);
    gainAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>   (proc.apvts, "gain", gainSlider);

    setResizable (true, true);
    setResizeLimits (480, 300, 2600, 1500);
    setSize (900, 520);

    startTimerHz (60);

    loadTheme();

}

SynthVisualizerEditor::~SynthVisualizerEditor()
{
    stopTimer();
}

void SynthVisualizerEditor::resized()
{
    auto bar = getLocalBounds()
                   .removeFromTop ((int) topBar)
                   .reduced (8, 5);

    // Theme-Einstellungen rechts oben
    themeButton.setBounds (
        bar.removeFromRight (32));

    bar.removeFromRight (8);

    // Sensitivity-Regler
    auto right = bar.removeFromRight (
        juce::jmin (360, bar.getWidth()));

    gainSlider.setBounds (
        right.removeFromRight (
            juce::jmax (100, right.getWidth() - 84)));

    gainLabel.setBounds (
        right.removeFromLeft (84));

    // Visualisierungs-Auswahl links
    modeBox.setBounds (
        bar.removeFromLeft (170));
}

//==============================================================================
void SynthVisualizerEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    animTime += dt;

    analyse();

    scrollPhase += dt * (0.25f + 1.6f * level);
    scrollPhase -= std::floor (scrollPhase);
    rotation += dt * (0.25f + 1.2f * level);

    updateParticles (dt);
    repaint();
}

void SynthVisualizerEditor::analyse()
{
    gainValue = proc.apvts.getRawParameterValue ("gain")->load();

    proc.getLatest (scopeL.data(), scopeR.data(), fftSize);

    float sumSq = 0.0f;
    for (int i = 0; i < fftSize; ++i)
    {
        mono[(size_t) i] = 0.5f * (scopeL[(size_t) i] + scopeR[(size_t) i]);
        sumSq += mono[(size_t) i] * mono[(size_t) i];
    }

    std::copy (mono.begin(), mono.end(), fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const float binHz = (float) (sr / (double) fftSize);
    const float fMin = 30.0f;
    const float fMax = jmin (18000.0f, (float) (sr * 0.5 * 0.95));

    for (int b = 0; b < numBands; ++b)
    {
        const float f0 = fMin * std::pow (fMax / fMin, (float) b / (float) numBands);
        const float f1 = fMin * std::pow (fMax / fMin, (float) (b + 1) / (float) numBands);
        const int i0 = jmax (1, (int) (f0 / binHz));
        const int i1 = jmin (fftSize / 2, jmax (i0 + 1, (int) (f1 / binHz)));

        float m = 0.0f;
        for (int i = i0; i < i1; ++i)
            m = jmax (m, fftData[(size_t) i]);

        const float fc = std::sqrt (f0 * f1);
        const float tilt = 2.5f * std::log2 (fc / 500.0f);
        const float amp = m / ((float) fftSize * 0.25f) * gainValue;
        const float db = Decibels::gainToDecibels (amp, -100.0f) + tilt;
        const float v = jlimit (0.0f, 1.0f, (db + 70.0f) / 70.0f);

        auto& cur = bands[(size_t) b];
        cur = v > cur ? v : cur * 0.90f + v * 0.10f;
        peaks[(size_t) b] = jmax (cur, peaks[(size_t) b] - 0.006f);
    }

    float bassNow = 0.0f;
    for (int i = 0; i < 6; ++i) bassNow += bands[(size_t) i];
    bassNow /= 6.0f;
    bass = bass * 0.6f + bassNow * 0.4f;

    const float rms = std::sqrt (sumSq / (float) fftSize);
    const float levelTarget = jlimit (0.0f, 1.0f, rms * gainValue * 2.5f);
    level = levelTarget > level ? levelTarget : level * 0.92f + levelTarget * 0.08f;

    if (bass > bassAvg * 1.3f && bass > 0.3f && beat < 0.3f)
        beat = 1.0f;
    beat *= 0.90f;
    bassAvg = bassAvg * 0.95f + bass * 0.05f;
}

void SynthVisualizerEditor::updateParticles (float dt)
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    if (mode != 4)
    {
        particles.clear();
        return;
    }

    const float w = (float) getWidth(), h = (float) getHeight();
    const float cx = w * 0.5f, cy = topBar + (h - topBar) * 0.5f;

    const int spawn = (int) (level * 5.0f + beat * 30.0f);
    for (int i = 0; i < spawn && particles.size() < 1500; ++i)
    {
        const float ang = rng.nextFloat() * MathConstants<float>::twoPi;
        const float spd = 60.0f + rng.nextFloat() * 220.0f * (0.5f + bass);
        particles.push_back ({ cx, cy, std::cos (ang) * spd, std::sin (ang) * spd, 1.0f,
                               (float) animTime * 0.1f + rng.nextFloat() * 0.25f });
    }

    for (auto& p : particles)
    {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vx *= 0.995f;
        p.vy *= 0.995f;
        p.life -= dt * 0.55f;
    }

    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [] (const Particle& p) { return p.life <= 0.0f; }),
                     particles.end());
}

//==============================================================================
void SynthVisualizerEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff07050d));

    auto area = getLocalBounds().toFloat();
    auto vis = area.withTrimmedTop (topBar);
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();

    g.saveState();
    g.reduceClipRegion (vis.toNearestInt());
    switch (mode)
    {
        case 0:  drawSynthwave (g, vis); break;
        case 1:  drawSpectrum  (g, vis); break;
        case 2:  drawScope     (g, vis); break;
        case 3:  drawRadial    (g, vis); break;
        default: drawParticles (g, vis); break;
    }
    g.restoreState();

    g.setColour (Colour (0xff120a24));
    g.fillRect (0.0f, 0.0f, area.getWidth(), topBar);
    g.setColour (Colour (0xff00e5ff).withAlpha (0.5f));
    g.fillRect (0.0f, topBar - 1.0f, area.getWidth(), 1.0f);
}

//==============================================================================
void SynthVisualizerEditor::drawSynthwave (Graphics& g, Rectangle<float> r)
{
    const float x0 = r.getX(), y0 = r.getY(), w = r.getWidth(), h = r.getHeight();
    const float bottom = y0 + h;
    const float horizon = y0 + h * 0.58f;
    const float cx = x0 + w * 0.5f;

    // Himmel
    ColourGradient sky (
        theme.background.brighter (0.35f), x0, y0,
        theme.background.darker (0.15f), x0, horizon, false);
    
    sky.addColour (0.65, theme.background);
    
    g.setGradientFill (sky);
    g.fillRect (x0, y0, w, horizon - y0);
    
    // Boden
    ColourGradient ground (
        theme.background, x0, horizon,
        theme.background.darker (0.65f), x0, bottom, false);
    
    g.setGradientFill (ground);
    g.fillRect (x0, horizon, w, bottom - horizon);
    
    // Sterne
    {
        Random sr (1234);
        for (int i = 0; i < 80; ++i)
        {
            const float sx = x0 + sr.nextFloat() * w;
            const float sy = y0 + sr.nextFloat() * (horizon - y0) * 0.75f;
            const float tw = 0.35f + 0.65f * std::abs (std::sin ((float) animTime * 1.5f + (float) i));
            g.setColour (Colours::white.withAlpha (tw * 0.8f));
            g.fillEllipse (sx, sy, 1.8f, 1.8f);
        }
    }

    // Sonne
    const float sunR = jmin (w, h) * 0.24f * (1.0f + 0.10f * level + 0.14f * beat);
    const float sunCy = horizon - sunR * 0.40f;

    for (int k = 4; k >= 1; --k)
    {
        const float rr = sunR * (1.0f + 0.22f * (float) k);
       g.setColour (theme.sweep.withAlpha (0.045f + 0.03f * level));
        g.fillEllipse (cx - rr, sunCy - rr, rr * 2.0f, rr * 2.0f);
    }

    g.saveState();
    g.reduceClipRegion (Rectangle<float> (x0, y0, w, horizon - y0).toNearestInt());

   ColourGradient sunG (
    theme.accent.brighter (0.5f), cx, sunCy - sunR,
    theme.sweep, cx, sunCy + sunR * 0.6f, false);
    g.setGradientFill (sunG);
    g.fillEllipse (cx - sunR, sunCy - sunR, sunR * 2.0f, sunR * 2.0f);

    for (int i = 0; i < 7; ++i)
    {
        const float t = (float) i / 6.0f;
        const float sy = sunCy + sunR * (0.02f + 0.42f * t);
        const float th = (1.5f + 6.0f * t) * (0.8f + 0.6f * bass);
        g.setColour (theme.background);
        g.fillRect (cx - sunR, sy, sunR * 2.0f, th);
    }
    g.restoreState();

    // Berge aus dem Spektrum (Bass in der Mitte)
    {
        const int pts = 120;
        Path mtn, ridge;
        mtn.startNewSubPath (x0, horizon);
        for (int i = 0; i < pts; ++i)
        {
            const float u = (float) i / (float) (pts - 1);
            const float t = std::abs (u * 2.0f - 1.0f);
            const float fi = t * (float) (numBands - 1);
            const int i0 = (int) fi;
            const int i1 = jmin (i0 + 1, numBands - 1);
            const float v = bands[(size_t) i0] + (bands[(size_t) i1] - bands[(size_t) i0]) * (fi - (float) i0);
            const float px = x0 + u * w;
            const float py = horizon - v * h * 0.30f - 2.0f;
            mtn.lineTo (px, py);
            if (i == 0) ridge.startNewSubPath (px, py);
            else        ridge.lineTo (px, py);
        }
        mtn.lineTo (x0 + w, horizon);
        mtn.closeSubPath();
        g.setColour (theme.background.darker (0.35f));
        g.fillPath (mtn);
        glowStroke (g, ridge, theme.waveform, 1.5f);
    }

   glowLine (g, Line<float> (x0, horizon, x0 + w, horizon), theme.accent, 2.0f);

    // Gitter
    g.saveState();
    g.reduceClipRegion (Rectangle<float> (x0, horizon, w, bottom - horizon).toNearestInt());

    const float gridAlpha = 0.45f + 0.4f * jmax (level, beat);

    for (int i = -24; i <= 24; ++i)
    {
        const float bx = cx + (float) i * w / 12.0f;
        const float fade = jmax (0.1f, 1.0f - std::abs ((float) i) / 30.0f);
        g.setColour (theme.grid.withAlpha (jlimit (0.0f, 1.0f, gridAlpha * fade)));
        g.drawLine (cx + (float) i * 2.0f, horizon, bx, bottom, 1.4f);
    }

    const int hl = 14;
    for (int k = 0; k < hl; ++k)
    {
        const float t = ((float) k + scrollPhase) / (float) hl;
        const float yy = horizon + (bottom - horizon) * std::pow (t, 2.3f);
        g.setColour (theme.waveform.withAlpha (jlimit (0.0f, 1.0f, gridAlpha * (0.3f + t))));
        g.drawLine (x0, yy, x0 + w, yy, 0.8f + 2.0f * t);
    }
    g.restoreState();
    loadTheme();
}

//==============================================================================
void SynthVisualizerEditor::drawSpectrum (Graphics& g, Rectangle<float> r)
{
    // Theme background
    ColourGradient bg (
        theme.background.brighter (0.35f),
        r.getX(), r.getY(),
        theme.background.darker (0.25f),
        r.getX(), r.getBottom(),
        false
    );

    g.setGradientFill (bg);
    g.fillRect (r);

    const float baseY = r.getY() + r.getHeight() * 0.80f;
    const float maxH = r.getHeight() * 0.72f;
    const float bw = r.getWidth() / (float) numBands;

    for (int i = 0; i < numBands; ++i)
    {
        const float x = r.getX() + (float) i * bw;
        const float bh = jmax (2.0f, bands[(size_t) i] * maxH);

        const float position = (float) i / (float) numBands;

        // Theme-based bar colours
        const Colour top = theme.waveform.interpolatedWith (
            theme.accent, position
        );

        const Colour bot = theme.waveform2.interpolatedWith (
            theme.sweep, position
        );

        // Main spectrum bars
        g.setGradientFill (
            ColourGradient (top, x, baseY - bh, bot, x, baseY, false)
        );

        g.fillRoundedRectangle (
            x + bw * 0.12f,
            baseY - bh,
            bw * 0.76f,
            bh,
            2.0f
        );

        // Reflection
        g.setGradientFill (
            ColourGradient (
                bot.withAlpha (0.35f),
                x, baseY,
                bot.withAlpha (0.0f),
                x, baseY + bh * 0.45f,
                false
            )
        );

        g.fillRect (
            x + bw * 0.12f,
            baseY + 2.0f,
            bw * 0.76f,
            bh * 0.45f
        );

        // Peak indicator
        g.setColour (theme.text.withAlpha (0.85f));

        g.fillRect (
            x + bw * 0.12f,
            baseY - peaks[(size_t) i] * maxH - 4.0f,
            bw * 0.76f,
            2.0f
        );
    }
}
//==============================================================================
void SynthVisualizerEditor::drawScope (Graphics& g, Rectangle<float> r)
{
    // Theme background
    g.setColour (theme.background);
    g.fillRect (r);

    const float midY = r.getCentreY();

    // Theme grid
    g.setColour (theme.grid);
    for (int i = 1; i < 8; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 8.0f;
        g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
    }

    for (int i = 1; i < 4; ++i)
    {
        const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
    }

    // Trigger: erster aufsteigender Nulldurchgang
    int start = 0;
    for (int i = 1; i < fftSize / 2; ++i)
    {
        if (mono[(size_t) (i - 1)] <= 0.0f && mono[(size_t) i] > 0.0f)
        {
            start = i - 1;
            break;
        }
    }

    const int span = 1024;
    const int n = jmax (16, (int) (r.getWidth() / 3.0f));

    auto build = [&] (const std::array<float, fftSize>& src)
    {
        Path p;
        for (int i = 0; i <= n; ++i)
        {
            const float u = (float) i / (float) n;
            const int idx = start + (int) (u * (float) (span - 1));
            const float s = jlimit (-1.0f, 1.0f, src[(size_t) idx] * gainValue);
            const float x = r.getX() + u * r.getWidth();
            const float y = midY - s * r.getHeight() * 0.42f;

            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        return p;
    };

    // Theme waveform colours
    glowStroke (g, build (scopeL), theme.waveform, 1.6f);
    glowStroke (g, build (scopeR), theme.waveform2, 1.6f);
}

//==============================================================================
void SynthVisualizerEditor::drawRadial (Graphics& g, Rectangle<float> r)
{
    ColourGradient bg (
        theme.background.brighter (0.35f),
        r.getCentreX(), r.getCentreY(),
        theme.background.darker (0.35f),
        r.getX(), r.getY(),
        true
    );

    g.setGradientFill (bg);
    g.fillRect (r);

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float m = jmin (r.getWidth(), r.getHeight());
    const float baseR = m * 0.18f * (1.0f + 0.2f * bass + 0.15f * beat);
    const int segs = numBands * 2;

    for (int s = 0; s < segs; ++s)
    {
        const int idx = s < numBands ? s : segs - 1 - s;
        const float ang = rotation + (float) s / (float) segs * MathConstants<float>::twoPi;
        const float len = bands[(size_t) idx] * m * 0.28f + 2.0f;
        const float dx = std::cos (ang);
        const float dy = std::sin (ang);

        const float position = (float) s / (float) segs;

        const Colour c = theme.waveform.interpolatedWith (
            theme.accent, position
        );

        glowLine (
            g,
            Line<float> (
                cx + dx * baseR,
                cy + dy * baseR,
                cx + dx * (baseR + len),
                cy + dy * (baseR + len)
            ),
            c,
            2.5f
        );
    }

    Path ring;
    ring.addEllipse (cx - baseR, cy - baseR, baseR * 2.0f, baseR * 2.0f);

    glowStroke (g, ring, theme.waveform, 1.5f);

    const float core = baseR * (0.35f + 0.5f * level);

    g.setGradientFill (
        ColourGradient (
            theme.sweep.withAlpha (0.9f),
            cx, cy,
            theme.sweep.withAlpha (0.0f),
            cx + core, cy,
            true
        )
    );

    g.fillEllipse (cx - core, cy - core, core * 2.0f, core * 2.0f);
}    
//==============================================================================
void SynthVisualizerEditor::drawParticles (Graphics& g, Rectangle<float> r)
{
    // Theme background
    g.setColour (theme.background);
    g.fillRect (r);

    const float cx = r.getCentreX(), cy = r.getCentreY();

    for (const auto& p : particles)
    {
        const float size = 2.0f + 6.0f * p.life * (0.5f + bass);

        const float colourPosition = jlimit (0.0f, 1.0f, p.hue);

        const Colour particleColour =
            theme.waveform.interpolatedWith (
                theme.accent, colourPosition
            );

        g.setColour (
            particleColour.withAlpha (
                jlimit (0.0f, 1.0f, p.life)
            )
        );

        g.fillEllipse (
            p.x - size * 0.5f,
            p.y - size * 0.5f,
            size,
            size
        );
    }

    // Central glowing orb
    const float orb = 30.0f + level * 70.0f + beat * 40.0f;

    g.setGradientFill (
        ColourGradient (
            theme.sweep.withAlpha (0.9f),
            cx, cy,
            theme.accent.withAlpha (0.0f),
            cx + orb, cy,
            true
        )
    );

    g.fillEllipse (
        cx - orb,
        cy - orb,
        orb * 2.0f,
        orb * 2.0f
    );
}
void SynthVisualizerEditor::applyTheme (int themeId)
{
    switch (themeId)
    {
        case 1: // Neon Green
            theme.background = Colour (0xff07100d);
            theme.accent     = Colour (0xff00ff99);
            theme.waveform   = Colour (0xff00ff99);
            theme.waveform2  = Colour (0xffaaffcc);
            theme.grid       = Colour (0xff183d2d);
            theme.sweep      = Colour (0xffb8ffd2);
            theme.text       = Colours::white;
            break;

        case 2: // Electric Blue
            theme.background = Colour (0xff050b18);
            theme.accent     = Colour (0xff00aaff);
            theme.waveform   = Colour (0xff00e5ff);
            theme.waveform2  = Colour (0xff5577ff);
            theme.grid       = Colour (0xff172b50);
            theme.sweep      = Colour (0xffaaddff);
            theme.text       = Colours::white;
            break;

        case 3: // Cyberpunk Purple
            theme.background = Colour (0xff10051b);
            theme.accent     = Colour (0xffc000ff);
            theme.waveform   = Colour (0xffe040ff);
            theme.waveform2  = Colour (0xffff55bb);
            theme.grid       = Colour (0xff35154a);
            theme.sweep      = Colour (0xffffaaff);
            theme.text       = Colours::white;
            break;

        case 4: // Neon Red
            theme.background = Colour (0xff160707);
            theme.accent     = Colour (0xffff3344);
            theme.waveform   = Colour (0xffff3344);
            theme.waveform2  = Colour (0xffff8866);
            theme.grid       = Colour (0xff451818);
            theme.sweep      = Colour (0xffffaaaa);
            theme.text       = Colours::white;
            break;

        case 5: // Sunset Orange
            theme.background = Colour (0xff160b08);
            theme.accent     = Colour (0xffff8833);
            theme.waveform   = Colour (0xffffaa44);
            theme.waveform2  = Colour (0xffff5577);
            theme.grid       = Colour (0xff49301e);
            theme.sweep      = Colour (0xffffcc88);
            theme.text       = Colours::white;
            break;

        case 6: // Monochrome
            theme.background = Colour (0xff080808);
            theme.accent     = Colour (0xffdddddd);
            theme.waveform   = Colour (0xffffffff);
            theme.waveform2  = Colour (0xffaaaaaa);
            theme.grid       = Colour (0xff383838);
            theme.sweep      = Colour (0xffeeeeee);
            theme.text       = Colours::white;
            break;

        default:
            return;
    }

    saveTheme();
    repaint();
}

void SynthVisualizerEditor::openColourPicker (int colourId)
{
    activeColourTarget = colourId;

    auto selector = std::make_unique<ColourSelector>(
        ColourSelector::showColourAtTop
        | ColourSelector::showSliders
        | ColourSelector::showColourspace);

    selector->setCurrentColour (
        colourId == 0 ? theme.background :
        colourId == 1 ? theme.waveform :
        colourId == 2 ? theme.accent :
        colourId == 3 ? theme.grid :
        colourId == 4 ? theme.sweep :
                        theme.text);

    selector->setSize (300, 350);
    selector->addChangeListener (this);

    CallOutBox::launchAsynchronously (
        std::move (selector),
        themeButton.getScreenBounds(),
        nullptr);
}

void SynthVisualizerEditor::changeListenerCallback (
    ChangeBroadcaster* source)
{
    auto* selector = dynamic_cast<ColourSelector*> (source);

    if (selector == nullptr)
        return;

    const auto colour = selector->getCurrentColour();

    switch (activeColourTarget)
    {
        case 0: theme.background = colour; break;
        case 1:
            theme.waveform = colour;
            break;
        case 2: theme.accent = colour; break;
        case 3: theme.grid = colour; break;
        case 4: theme.sweep = colour; break;
        case 5: theme.text = colour; break;
        default: return;
    }

    saveTheme();
    repaint();
}

void SynthVisualizerEditor::saveTheme()
{
    auto state = proc.apvts.state;

    state.setProperty ("themeBackground",
                       theme.background.toString(), nullptr);

    state.setProperty ("themeAccent",
                       theme.accent.toString(), nullptr);

    state.setProperty ("themeWaveform",
                       theme.waveform.toString(), nullptr);

    state.setProperty ("themeWaveform2",
                       theme.waveform2.toString(), nullptr);

    state.setProperty ("themeGrid",
                       theme.grid.toString(), nullptr);

    state.setProperty ("themeSweep",
                       theme.sweep.toString(), nullptr);

    state.setProperty ("themeText",
                       theme.text.toString(), nullptr);
}

void SynthVisualizerEditor::loadTheme()
{
    auto state = proc.apvts.state;

    auto loadColour = [&state] (const juce::Identifier& name,
                                juce::Colour fallback)
    {
        auto value = state.getProperty (name);

        if (value.isVoid())
            return fallback;

        return juce::Colour::fromString (value.toString());
    };

    theme.background = loadColour ("themeBackground", juce::Colour (0xff07050d));
    theme.accent     = loadColour ("themeAccent", juce::Colour (0xff00e5ff));
    theme.waveform   = loadColour ("themeWaveform", juce::Colour (0xff00e5ff));
    theme.waveform2  = loadColour ("themeWaveform2", juce::Colour (0xffff3da8));
    theme.grid       = loadColour ("themeGrid", juce::Colour (0xff1c1438));
    theme.sweep      = loadColour ("themeSweep", juce::Colour (0xffff3da8));
    theme.text       = loadColour ("themeText", juce::Colour (0xffffffff));

    repaint();
}
//idk
