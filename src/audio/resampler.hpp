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

class Resampler {
public:
    Resampler(int inputRate = 32000, int outputRate = 48000);

    // Process one input sample, may produce 0 or more output samples
    // Returns number of samples written to output arrays
    int process(s16 inL, s16 inR, s16* outL, s16* outR, int maxOut);

    void reset();

private:
    int inputRate;
    int outputRate;

    // Interpolation state
    s16 prevL = 0;
    s16 prevR = 0;
    s16 currL = 0;
    s16 currR = 0;

    // Phase accumulator (fixed point 16.16)
    u32 phase = 0;
    u32 phaseIncrement;
};
