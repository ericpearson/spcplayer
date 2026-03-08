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
#include "resampler.hpp"
#include <SDL.h>
#include <vector>

class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();

    bool init(int outputSampleRate = 48000);
    void shutdown();

    void start();
    void stop();

    // Queue a stereo sample (from DSP at 32kHz)
    void queueSample(s16 left, s16 right);

    bool needsMoreSamples() const;
    size_t samplesQueued() const;

    void setVolume(float vol);
    void setSurround(bool enabled) { surround = enabled; }
    void setBassBoost(bool enabled) { bassBoost = enabled; }

private:
    SDL_AudioDeviceID deviceId = 0;
    Resampler resampler;
    std::vector<s16> sampleBuffer;  // Batch samples before queuing
    float volume = 1.0f;
    bool running = false;
    bool surround = false;
    bool bassBoost = false;
    
    // Bass boost filter state (simple IIR low-pass)
    float bassL = 0.0f;
    float bassR = 0.0f;
};
