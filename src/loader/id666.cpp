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

#include "id666.hpp"
#include <cstring>
#include <algorithm>

namespace {

std::string extractString(const u8* data, size_t maxLen) {
    std::string result;
    result.reserve(maxLen);
    for (size_t i = 0; i < maxLen && data[i] != 0; ++i) {
        if (data[i] >= 0x20 && data[i] <= 0x7E) {
            result += static_cast<char>(data[i]);
        }
    }
    // Trim trailing spaces
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

u32 readLE32(const u8* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

u16 readLE16(const u8* data) {
    return data[0] | (data[1] << 8);
}

int parseIntString(const u8* data, size_t len) {
    std::string s = extractString(data, len);
    if (s.empty()) return 0;
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}

} // anonymous namespace

ID666Format detectID666Format(const u8* headerData) {
    // ID666 tag starts at offset 0x2E in SPC file
    // We receive data starting from that offset

    // Check the date field at offset 0x9E - 0x2E = 0x70 relative to ID666 start
    // Text format: 11 bytes "MM/DD/YYYY\0"
    // Binary format: 4 bytes YYYYMMDD as integer

    const u8* dateField = headerData + 0x70; // Date is at 0x9E in file, 0x70 in ID666

    // Text format heuristics:
    // 1. Look for slash characters in date
    // 2. Check if duration fields look like ASCII numbers

    bool hasSlashes = (dateField[2] == '/' || dateField[5] == '/');

    // Check duration field (0xA9 in file = 0x7B in ID666 for text format)
    // Text format: 3 bytes ASCII
    // Binary format: 3 bytes at different offset
    const u8* durationText = headerData + 0x7B;
    bool durationIsAscii = true;
    for (int i = 0; i < 3; ++i) {
        if (durationText[i] != 0 && (durationText[i] < '0' || durationText[i] > '9')) {
            durationIsAscii = false;
            break;
        }
    }

    if (hasSlashes || durationIsAscii) {
        return ID666Format::Text;
    }

    // Additional check: look at emulator byte position
    // Text format: 0xD1 in file = 0xA3 in ID666
    // Binary format: 0xD1 in file = 0xA3 in ID666 (same offset, different meaning)

    // Default to binary if text detection fails
    return ID666Format::Binary;
}

bool parseID666(const u8* headerData, ID666& out) {
    out.format = detectID666Format(headerData);

    if (out.format == ID666Format::Text) {
        // Text format offsets (relative to ID666 start at 0x2E)
        out.songTitle = extractString(headerData + 0x00, 32);      // 0x2E: Song title
        out.gameTitle = extractString(headerData + 0x20, 32);      // 0x4E: Game title
        out.dumperName = extractString(headerData + 0x40, 16);     // 0x6E: Dumper name
        out.comments = extractString(headerData + 0x50, 32);       // 0x7E: Comments
        out.date = extractString(headerData + 0x70, 11);           // 0x9E: Date
        out.playSeconds = parseIntString(headerData + 0x7B, 3);    // 0xA9: Play time (seconds)
        out.fadeMilliseconds = parseIntString(headerData + 0x7E, 5); // 0xAC: Fade time (ms)
        out.artist = extractString(headerData + 0x83, 32);         // 0xB1: Artist
        out.channelDisable = headerData[0xA3];                     // 0xD1: Channel disable
        out.emulator = static_cast<Emulator>(headerData[0xA4]);    // 0xD2: Emulator
    } else {
        // Binary format offsets
        out.songTitle = extractString(headerData + 0x00, 32);      // 0x2E: Song title
        out.gameTitle = extractString(headerData + 0x20, 32);      // 0x4E: Game title
        out.dumperName = extractString(headerData + 0x40, 16);     // 0x6E: Dumper name
        out.comments = extractString(headerData + 0x50, 32);       // 0x7E: Comments

        // Binary date: 4 bytes at 0x9E = YYYYMMDD
        u32 dateInt = readLE32(headerData + 0x70);
        if (dateInt > 0) {
            int year = dateInt / 10000;
            int month = (dateInt / 100) % 100;
            int day = dateInt % 100;
            char dateBuf[16];
            snprintf(dateBuf, sizeof(dateBuf), "%04d/%02d/%02d", year, month, day);
            out.date = dateBuf;
        }

        // Binary: duration at 0xA9 is 3 bytes (24-bit LE)
        out.playSeconds = headerData[0x7B] | (headerData[0x7C] << 8) | (headerData[0x7D] << 16);

        // Binary: fade at 0xAC is 4 bytes (32-bit LE)
        out.fadeMilliseconds = readLE32(headerData + 0x7E);

        out.artist = extractString(headerData + 0x82, 32);         // 0xB0: Artist (binary)
        out.channelDisable = headerData[0xA2];                     // 0xD0: Channel disable
        out.emulator = static_cast<Emulator>(headerData[0xA3]);    // 0xD1: Emulator
    }

    // Sanity check play time
    if (out.playSeconds > 3600 * 24) out.playSeconds = 0;
    if (out.fadeMilliseconds > 60000) out.fadeMilliseconds = 10000;

    return true;
}
