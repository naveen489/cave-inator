#pragma once
#include <JuceHeader.h>
#include <cmath>

//==============================================================================
// A single independent slow-drifting random walker.
//
// Each instance picks a new random target in [-1, 1] every minSec–maxSec
// seconds and smoothly interpolates toward it with a per-block exponential
// filter (time constant = tcSec). Because every drifter uses its own seeded
// juce::Random, they never move in sync even if they share the same period.
//==============================================================================
struct SlowDrifter
{
    float current           = 0.0f;
    float target            = 0.0f;
    int   samplesUntilChange = 0;

    float tcSec;    // smoothing time constant (seconds)
    float minSec;   // minimum seconds between picking a new target
    float maxSec;   // maximum seconds between picking a new target

    // Each drifter gets a unique seed so they are never synchronized
    juce::Random rng { juce::Time::currentTimeMillis() + (juce::int64)(intptr_t)this };

    SlowDrifter (float tcSec, float minSec, float maxSec)
        : tcSec (tcSec), minSec (minSec), maxSec (maxSec) {}

    void tick (int blockSize, double sampleRate)
    {
        samplesUntilChange -= blockSize;
        if (samplesUntilChange <= 0)
        {
            target = rng.nextFloat() * 2.0f - 1.0f;
            int minS = (int)(minSec * sampleRate);
            int maxS = (int)(maxSec * sampleRate);
            samplesUntilChange = minS + rng.nextInt (juce::jmax (1, maxS - minS));
        }
        // Exponential smoothing toward target (per block)
        float coeff = 1.0f - std::exp (-(float)blockSize / (tcSec * (float)sampleRate));
        current += (target - current) * coeff;
    }

    float get() const { return current; }
};

//==============================================================================
// BatmanizeModulator
//
// Owns seven completely independent SlowDrifters — one for each modulated
// parameter. Their time constants and change periods are deliberately
// staggered so nothing ever feels synchronized.
//
// Usage in processBlock:
//   batmanize.tick(blockSize);
//   float instability = BatmanizeModulator::mod(rawInstability,
//                                               batmanize.instability.get(),
//                                               0.20f, batmanLevel);
//
// Modulation depths at batmanLevel = 1.0:
//   Instability  ±0.20   (meaningful cave-life drift)
//   Density      ±0.10   (gentle breathing)
//   Mutation     ±0.25   (dark harmonic shifts)
//   Ghost        +0.35   (ghost only increases — more bats, never fewer)
//   Width        ±0.15   (space drifts)
//   Darkness     ±0.10   (geological tonal darkening)
//   Decay        ±0.08   (subtle tail length shimmer)
//==============================================================================
class BatmanizeModulator
{
public:
    // Independent drifters — (smoothing tc, min change, max change) all in seconds
    SlowDrifter instability { 2.0f,  3.0f, 11.0f };   // medium-slow, noticeable drift
    SlowDrifter density     { 3.5f,  5.0f, 17.0f };   // slow breathing
    SlowDrifter mutation    { 1.5f,  2.0f,  9.0f };   // faster, event-like
    SlowDrifter ghost       { 1.0f,  1.0f,  6.0f };   // fast, probabilistic bursts
    SlowDrifter width       { 4.5f,  7.0f, 22.0f };   // very slow spatial drift
    SlowDrifter darkness    { 6.0f, 10.0f, 28.0f };   // geological — almost imperceptible motion
    SlowDrifter decay       { 3.0f,  4.0f, 13.0f };   // slow tail shimmer

    void prepare (double sr) { sampleRate = sr; }

    void tick (int blockSize)
    {
        instability .tick (blockSize, sampleRate);
        density     .tick (blockSize, sampleRate);
        mutation    .tick (blockSize, sampleRate);
        ghost       .tick (blockSize, sampleRate);
        width       .tick (blockSize, sampleRate);
        darkness    .tick (blockSize, sampleRate);
        decay       .tick (blockSize, sampleRate);
    }

    // Modulate a parameter around its anchor value.
    // driftVal in [-1, 1], depth = max offset at batman = 1.0, result clamped [0, 1].
    static float mod (float anchor, float driftVal, float depth, float batman)
    {
        return juce::jlimit (0.0f, 1.0f, anchor + driftVal * depth * batman);
    }

    // Ghost only ever gets boosted — more bats, never fewer.
    static float modGhost (float anchor, float driftVal, float batman)
    {
        return juce::jlimit (0.0f, 1.0f, anchor + std::abs (driftVal) * 0.35f * batman);
    }

private:
    double sampleRate = 44100.0;
};
