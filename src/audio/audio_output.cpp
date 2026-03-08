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
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    resampler = Resampler(32000, outputSampleRate);

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = outputSampleRate;

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void AudioOutput::shutdown() {
    if (stream) {
        stop();
        SDL_ClearAudioStream(stream);
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
}

void AudioOutput::start() {
    if (stream) {
        running = true;
        SDL_ResumeAudioStreamDevice(stream);
    }
}

void AudioOutput::stop() {
    if (stream) {
        running = false;
        SDL_PauseAudioStreamDevice(stream);
    }
}

void AudioOutput::flush() {
    if (stream) {
        if (!sampleBuffer.empty()) {
            SDL_PutAudioStreamData(stream, sampleBuffer.data(), sampleBuffer.size() * sizeof(s16));
            sampleBuffer.clear();
        }
        SDL_FlushAudioStream(stream);
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
        SDL_PutAudioStreamData(stream, sampleBuffer.data(), sampleBuffer.size() * sizeof(s16));
        sampleBuffer.clear();
    }
}

bool AudioOutput::needsMoreSamples() const {
    return SDL_GetAudioStreamQueued(stream) < 16384;
}

size_t AudioOutput::samplesQueued() const {
    return SDL_GetAudioStreamQueued(stream) / 4;  // stereo s16 = 4 bytes per sample
}

void AudioOutput::setVolume(float vol) {
    volume = std::clamp(vol, 0.0f, 1.0f);
}
