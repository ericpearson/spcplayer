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
#include <string>

enum class ID666Format {
    Unknown,
    Text,
    Binary
};

enum class Emulator : u8 {
    Unknown = 0,
    ZSNES = 1,
    Snes9x = 2,
    ZST2SPC = 3,
    Other = 4,
    SNEShout = 5,
    ZSNESw = 6,
    Snes9xpp = 7,
    SNESGT = 8
};

struct ID666 {
    std::string songTitle;
    std::string gameTitle;
    std::string dumperName;
    std::string comments;
    std::string artist;
    std::string date;

    int playSeconds = 0;
    int fadeMilliseconds = 0;
    u8 channelDisable = 0;
    Emulator emulator = Emulator::Unknown;
    ID666Format format = ID666Format::Unknown;
};

// Parse ID666 from SPC header data (starting at offset 0x2E)
bool parseID666(const u8* headerData, ID666& out);

// Detect if ID666 is text or binary format
ID666Format detectID666Format(const u8* headerData);
