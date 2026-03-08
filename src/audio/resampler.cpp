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

#include "resampler.hpp"

Resampler::Resampler(int inputRate, int outputRate)
    : inputRate(inputRate), outputRate(outputRate) {
    // Phase increment in 16.16 fixed point
    // For each output sample, we advance by (inputRate / outputRate) input samples
    phaseIncrement = (static_cast<u32>(inputRate) << 16) / outputRate;
    reset();
}

void Resampler::reset() {
    prevL = prevR = 0;
    currL = currR = 0;
    phase = 0;
}

int Resampler::process(s16 inL, s16 inR, s16* outL, s16* outR, int maxOut) {
    prevL = currL;
    prevR = currR;
    currL = inL;
    currR = inR;

    int outCount = 0;

    // Generate output samples until phase exceeds 1.0 (0x10000)
    while (phase < 0x10000 && outCount < maxOut) {
        // Linear interpolation
        u32 frac = phase & 0xFFFF;
        s32 sampleL = prevL + (((currL - prevL) * static_cast<s32>(frac)) >> 16);
        s32 sampleR = prevR + (((currR - prevR) * static_cast<s32>(frac)) >> 16);

        outL[outCount] = static_cast<s16>(sampleL);
        outR[outCount] = static_cast<s16>(sampleR);
        outCount++;

        phase += phaseIncrement;
    }

    // Wrap phase
    if (phase >= 0x10000) {
        phase -= 0x10000;
    }

    return outCount;
}
