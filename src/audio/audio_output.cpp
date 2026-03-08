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

#include "audio_output.hpp"
#include <iostream>
#include <algorithm>

AudioOutput::AudioOutput() : resampler(32000, 48000) {
    sampleBuffer.reserve(2048);
}

AudioOutput::~AudioOutput() {
    shutdown();
}

bool AudioOutput::init(int outputSampleRate) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    resampler = Resampler(32000, outputSampleRate);

    SDL_AudioSpec desired{};
    desired.freq = outputSampleRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = nullptr;  // Use SDL_QueueAudio instead

    deviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (deviceId == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void AudioOutput::shutdown() {
    if (deviceId != 0) {
        stop();
        SDL_CloseAudioDevice(deviceId);
        deviceId = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioOutput::start() {
    if (deviceId != 0) {
        running = true;
        SDL_PauseAudioDevice(deviceId, 0);
    }
}

void AudioOutput::stop() {
    if (deviceId != 0) {
        running = false;
        SDL_PauseAudioDevice(deviceId, 1);
        SDL_ClearQueuedAudio(deviceId);
    }
}

void AudioOutput::queueSample(s16 left, s16 right) {
    s16 outL[4], outR[4];
    int count = resampler.process(left, right, outL, outR, 4);

    for (int i = 0; i < count; ++i) {
        s32 l = outL[i];
        s32 r = outR[i];
        
        // Pseudo-surround: subtract center to widen stereo image
        if (surround) {
            s32 center = (l + r) / 2;
            l = l - center;
            r = r - center;
        }
        
        // Bass boost: simple low-pass filter added back (with headroom)
        if (bassBoost) {
            constexpr float alpha = 0.05f;  // Lower cutoff for deeper bass
            constexpr float boost = 0.5f;   // Moderate boost to avoid clipping
            bassL = bassL + alpha * (l - bassL);
            bassR = bassR + alpha * (r - bassR);
            l = static_cast<s32>(l * 0.8f + bassL * boost);  // Reduce dry, add bass
            r = static_cast<s32>(r * 0.8f + bassR * boost);
        }
        
        l = static_cast<s32>(l * volume);
        r = static_cast<s32>(r * volume);
        l = std::clamp(l, static_cast<s32>(-32768), static_cast<s32>(32767));
        r = std::clamp(r, static_cast<s32>(-32768), static_cast<s32>(32767));
        sampleBuffer.push_back(static_cast<s16>(l));
        sampleBuffer.push_back(static_cast<s16>(r));
    }

    // Flush buffer periodically
    if (sampleBuffer.size() >= 1024) {
        SDL_QueueAudio(deviceId, sampleBuffer.data(), sampleBuffer.size() * sizeof(s16));
        sampleBuffer.clear();
    }
}

bool AudioOutput::needsMoreSamples() const {
    return SDL_GetQueuedAudioSize(deviceId) < 16384;
}

size_t AudioOutput::samplesQueued() const {
    return SDL_GetQueuedAudioSize(deviceId) / 4;  // stereo s16 = 4 bytes per sample
}

void AudioOutput::setVolume(float vol) {
    volume = std::clamp(vol, 0.0f, 1.0f);
}
