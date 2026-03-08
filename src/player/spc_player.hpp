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

#include "cpu/spc700.hpp"
#include "dsp/dsp.hpp"
#include "loader/spc_file.hpp"
#include "audio/audio_output.hpp"
#include <atomic>
#include <memory>

class SPCPlayer {
public:
    SPCPlayer();
    ~SPCPlayer();

    // Load an SPC file
    bool load(const std::filesystem::path& path);

    // Initialize audio output
    bool initAudio(int sampleRate = 48000);

    // Start/stop playback
    void play();
    void pause();
    void stop();

    bool isPlaying() const { return playing; }
    bool isFinished() const;
    bool needsMoreSamples() const { return audio.needsMoreSamples(); }

    // Run emulation for the audio callback
    // Generates samples until buffer is satisfied
    void generateSamples(int count);

    // Get metadata
    const SPCFile& getSPCFile() const { return spcFile; }

    // Volume control (0.0 - 1.0)
    void setVolume(float vol);
    
    // Audio effects
    void setSurround(bool enabled) { audio.setSurround(enabled); }
    void setBassBoost(bool enabled) { audio.setBassBoost(enabled); }

private:
    SPCFile spcFile;
    SPC700 cpu;
    DSP dsp;
    AudioOutput audio;

    std::atomic<bool> playing{false};
    bool loaded = false;

    // Timing
    u64 samplesGenerated = 0;
    u64 songLengthSamples = 0;  // 0 = no limit
    u64 fadeLengthSamples = 0;
    static constexpr int DSP_SAMPLE_RATE = 32000;

    // CPU cycles per DSP sample (32kHz = every 32 cycles at 1.024MHz)
    static constexpr int CYCLES_PER_SAMPLE = 32;
};
