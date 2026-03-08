// spcplayer - SNES SPC700 music player
// Copyright (C) 2026 Eric Pearson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "util/types.hpp"
#include <array>

// Forward declaration
class SPC700;

// DSP register addresses (per-voice, multiply by 0x10 for voice N)
namespace DSPReg {
    constexpr u8 VxVOLL   = 0x00;  // Left volume
    constexpr u8 VxVOLR   = 0x01;  // Right volume
    constexpr u8 VxPITCHL = 0x02;  // Pitch (low)
    constexpr u8 VxPITCHH = 0x03;  // Pitch (high)
    constexpr u8 VxSRCN   = 0x04;  // Source number
    constexpr u8 VxADSR1  = 0x05;  // ADSR settings 1
    constexpr u8 VxADSR2  = 0x06;  // ADSR settings 2
    constexpr u8 VxGAIN   = 0x07;  // GAIN settings
    constexpr u8 VxENVX   = 0x08;  // Current envelope (read-only)
    constexpr u8 VxOUTX   = 0x09;  // Current sample (read-only)

    // Global registers
    constexpr u8 MVOLL    = 0x0C;  // Main volume left
    constexpr u8 MVOLR    = 0x1C;  // Main volume right
    constexpr u8 EVOLL    = 0x2C;  // Echo volume left
    constexpr u8 EVOLR    = 0x3C;  // Echo volume right
    constexpr u8 KON      = 0x4C;  // Key on
    constexpr u8 KOFF     = 0x5C;  // Key off
    constexpr u8 FLG      = 0x6C;  // Flags (reset, mute, echo write, noise clock)
    constexpr u8 ENDX     = 0x7C;  // Voice end flags (read-only)

    constexpr u8 EFB      = 0x0D;  // Echo feedback
    constexpr u8 PMON     = 0x2D;  // Pitch modulation enable
    constexpr u8 NON      = 0x3D;  // Noise enable
    constexpr u8 EON      = 0x4D;  // Echo enable
    constexpr u8 DIR      = 0x5D;  // Sample directory page
    constexpr u8 ESA      = 0x6D;  // Echo buffer page
    constexpr u8 EDL      = 0x7D;  // Echo delay

    // FIR filter coefficients
    constexpr u8 FIR0     = 0x0F;
    constexpr u8 FIR1     = 0x1F;
    constexpr u8 FIR2     = 0x2F;
    constexpr u8 FIR3     = 0x3F;
    constexpr u8 FIR4     = 0x4F;
    constexpr u8 FIR5     = 0x5F;
    constexpr u8 FIR6     = 0x6F;
    constexpr u8 FIR7     = 0x7F;
}

// Envelope state
enum class EnvelopeState {
    Attack,
    Decay,
    Sustain,
    Release
};

// Voice state structure
struct Voice {
    // BRR decoder state
    s16 samples[4] = {};        // Last 4 samples for Gaussian interpolation
    u16 brrAddr = 0;            // Current BRR block address
    u8 brrHeader = 0;           // Current block header
    int brrSampleIdx = 0;       // Current sample within block (0-15)
    bool brrEnd = false;        // End flag encountered
    s16 prevSample1 = 0;        // Previous sample for BRR filter
    s16 prevSample2 = 0;        // Previous-previous sample for BRR filter

    // Pitch counter (16-bit fractional)
    u16 pitchCounter = 0;

    // Envelope state
    EnvelopeState envState = EnvelopeState::Release;
    s32 envelope = 0;           // Current envelope level (0-0x7FF)
    int envDelay = 0;           // Envelope counter

    // Output
    s16 output = 0;             // Current sample output

    // Key state
    bool keyOn = false;
    bool keyOff = false;
    int konDelay = 0;  // 5-sample delay after KON before actual output

    void reset() {
        std::fill(std::begin(samples), std::end(samples), 0);
        brrAddr = 0;
        brrHeader = 0;
        brrSampleIdx = 0;
        brrEnd = false;
        prevSample1 = 0;
        prevSample2 = 0;
        pitchCounter = 0;
        envState = EnvelopeState::Release;
        envelope = 0;
        envDelay = 0;
        output = 0;
        keyOn = false;
        keyOff = false;
        konDelay = 0;
    }
};

class DSP {
public:
    DSP();

    void reset();
    void setSPC700(SPC700* cpu) { spc = cpu; }

    // Load initial DSP register state
    void loadRegisters(const u8* regs);

    // Register access from CPU
    u8 read(u8 addr) const;
    void write(u8 addr, u8 value);

    // Generate one stereo sample (called every 32 CPU cycles)
    void tick();

    // Get output samples
    s16 getOutputL() const { return outputL; }
    s16 getOutputR() const { return outputR; }

    // Check if sample is ready
    bool sampleReady() const { return sampleGenerated; }
    void clearSampleFlag() { sampleGenerated = false; }

private:
    SPC700* spc = nullptr;

    // 128 DSP registers
    std::array<u8, 128> regs{};

    // 8 voices
    std::array<Voice, 8> voices{};

    // Echo buffer state
    u16 echoOffset = 0;
    u16 echoLength = 0;
    std::array<s16, 8> firBufferL{};
    std::array<s16, 8> firBufferR{};
    int firIndex = 0;

    // Output
    s16 outputL = 0;
    s16 outputR = 0;
    bool sampleGenerated = false;

    // Noise generator (15-bit LFSR)
    s16 noise = 0x4000;

    // Global counter for envelope/noise rate timing
    int counter = 0;
    static constexpr int COUNTER_RANGE = 30720;  // 2048 * 5 * 3

    // KON/KOFF latching
    u8 konLatch = 0;
    u8 koffLatch = 0;
    bool everyOtherSample = false;

    // Internal processing
    void processVoice(int v);
    s16 decodeBRRSample(int v);
    s16 interpolate(int v);
    void runEnvelope(int v);
    void processEcho(s32& outL, s32& outR);
    void runCounter();
    bool readCounter(int rate);

    // Memory access through SPC700
    u8 readRAM(u16 addr) const;

    // Helper to get sample directory address
    u16 getSampleAddr(int srcn) const;
    u16 getSampleLoopAddr(int srcn) const;
};
