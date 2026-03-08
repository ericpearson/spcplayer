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

#include "player/spc_player.hpp"

SPCPlayer::SPCPlayer() {
    // Connect CPU and DSP
    cpu.setDSP(&dsp);
    dsp.setSPC700(&cpu);
}

SPCPlayer::~SPCPlayer() {
    stop();
}

bool SPCPlayer::load(const std::filesystem::path& path) {
    stop();

    if (!spcFile.load(path)) {
        return false;
    }

    // Load CPU state from SPC file
    const auto& header = spcFile.getHeader();
    cpu.loadState(
        spcFile.getRAM().data(),
        header.pc,
        header.a,
        header.x,
        header.y,
        header.psw,
        header.sp
    );

    // Load DSP registers
    dsp.loadRegisters(spcFile.getDSPRegs().data());

    // Set song length from ID666 metadata
    samplesGenerated = 0;
    const auto& id666 = spcFile.getID666();
    if (id666 && id666->playSeconds > 0) {
        songLengthSamples = static_cast<u64>(id666->playSeconds) * DSP_SAMPLE_RATE;
        fadeLengthSamples = static_cast<u64>(id666->fadeMilliseconds) * DSP_SAMPLE_RATE / 1000;
    } else {
        songLengthSamples = 0;  // No limit
        fadeLengthSamples = 0;
    }

    loaded = true;
    return true;
}

bool SPCPlayer::initAudio(int sampleRate) {
    return audio.init(sampleRate);
}

void SPCPlayer::play() {
    if (!loaded) return;

    playing = true;
    audio.start();
}

void SPCPlayer::pause() {
    playing = false;
    audio.stop();
}

void SPCPlayer::stop() {
    playing = false;
    audio.stop();
}

void SPCPlayer::generateSamples(int count) {
    if (!playing || !loaded) return;

    for (int i = 0; i < count; i++) {
        // Check if song is finished
        if (songLengthSamples > 0 && samplesGenerated >= songLengthSamples + fadeLengthSamples) {
            playing = false;
            return;
        }

        // Run CPU for 32 cycles (one DSP sample period)
        cpu.run(CYCLES_PER_SAMPLE);

        // Generate one DSP sample
        dsp.tick();

        // Apply fade out
        s16 outL = dsp.getOutputL();
        s16 outR = dsp.getOutputR();
        if (songLengthSamples > 0 && samplesGenerated >= songLengthSamples) {
            u64 fadePos = samplesGenerated - songLengthSamples;
            float fadeVol = 1.0f - static_cast<float>(fadePos) / fadeLengthSamples;
            outL = static_cast<s16>(outL * fadeVol);
            outR = static_cast<s16>(outR * fadeVol);
        }

        // Queue the sample for output
        audio.queueSample(outL, outR);
        samplesGenerated++;
    }
}

bool SPCPlayer::isFinished() const {
    if (songLengthSamples == 0) return false;
    return samplesGenerated >= songLengthSamples + fadeLengthSamples;
}

void SPCPlayer::setVolume(float vol) {
    audio.setVolume(vol);
}
