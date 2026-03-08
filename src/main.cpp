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
#include <SDL.h>
#include <iostream>
#include <csignal>
#include <atomic>

// Global flag for graceful shutdown
static std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " [options] <file.spc>\n";
    std::cerr << "\nSNES SPC700 music player\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  -s, --surround  Enable pseudo-surround sound\n";
    std::cerr << "  -b, --bass      Enable bass boost\n";
    std::cerr << "\nControls:\n";
    std::cerr << "  Ctrl+C  - Quit\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Parse options
    bool surround = false;
    bool bassBoost = false;
    const char* spcFile = nullptr;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-s" || arg == "--surround") {
            surround = true;
        } else if (arg == "-b" || arg == "--bass") {
            bassBoost = true;
        } else if (arg[0] != '-') {
            spcFile = argv[i];
        }
    }
    
    if (!spcFile) {
        printUsage(argv[0]);
        return 1;
    }

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SPCPlayer player;

    // Initialize audio
    if (!player.initAudio(48000)) {
        std::cerr << "Failed to initialize audio\n";
        SDL_Quit();
        return 1;
    }

    // Enable audio effects if requested
    if (surround) {
        player.setSurround(true);
        std::cout << "Surround sound enabled\n";
    }
    if (bassBoost) {
        player.setBassBoost(true);
        std::cout << "Bass boost enabled\n";
    }

    // Load SPC file
    std::filesystem::path spcPath = spcFile;
    if (!player.load(spcPath)) {
        std::cerr << "Failed to load: " << spcPath << "\n";
        SDL_Quit();
        return 1;
    }

    // Print metadata
    player.getSPCFile().printMetadata();

    std::cout << "\nPlaying... (Ctrl+C to quit)\n\n";

    // Pre-fill audio buffer before starting
    player.generateSamples(8192);

    // Start playback
    player.play();

    // Main loop - generate samples to feed the audio buffer
    while (g_running && player.isPlaying()) {
        // Keep buffer well-filled
        while (player.needsMoreSamples() && g_running) {
            player.generateSamples(256);
        }
        // Only sleep when buffer is healthy
        SDL_Delay(1);
    }

    // Clean shutdown
    player.stop();
    SDL_Quit();

    std::cout << "\nPlayback stopped.\n";
    return 0;
}
