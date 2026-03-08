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

#include "spc_file.hpp"
#include <fstream>
#include <iostream>
#include <cstring>

bool SPCFile::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << path << std::endl;
        return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Minimum SPC file size: 0x100 header + 0x10000 RAM + 0x80 DSP = 0x10180
    if (fileSize < 0x10180) {
        std::cerr << "Error: File too small to be a valid SPC file" << std::endl;
        return false;
    }

    // Read entire file
    std::vector<u8> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);

    if (!file) {
        std::cerr << "Error: Failed to read file" << std::endl;
        return false;
    }

    // Parse header
    if (!parseHeader(data.data())) {
        return false;
    }

    // Load RAM (offset 0x100, size 0x10000)
    std::memcpy(ram.data(), data.data() + 0x100, 65536);

    // Load DSP registers (offset 0x10100, size 0x80)
    std::memcpy(dspRegs.data(), data.data() + 0x10100, 128);

    // Load Extra RAM / IPL ROM replacement (offset 0x101C0, size 0x40)
    // This goes into RAM at 0xFFC0-0xFFFF
    if (fileSize >= 0x10200) {
        std::memcpy(ram.data() + 0xFFC0, data.data() + 0x101C0, 64);
    }

    // Parse ID666 if present
    if (header.hasID666) {
        ID666 tag;
        if (parseID666(data.data() + 0x2E, tag)) {
            id666 = tag;
        }
    }

    return true;
}

bool SPCFile::parseHeader(const u8* data) {
    // Check signature (33 bytes at offset 0)
    const char* sig = "SNES-SPC700 Sound File Data v0.30";
    if (std::memcmp(data, sig, 33) != 0) {
        std::cerr << "Error: Invalid SPC signature" << std::endl;
        return false;
    }

    // Check for 0x1A 0x1A at offset 0x21
    if (data[0x21] != 0x1A || data[0x22] != 0x1A) {
        std::cerr << "Error: Invalid SPC header marker" << std::endl;
        return false;
    }

    // ID666 tag flag at offset 0x23
    // 0x1A = has ID666, 0x1B = no ID666
    header.hasID666 = (data[0x23] == 0x1A);

    // Version minor at 0x24 (should be 30 for v0.30)
    // Not strictly validated

    // CPU registers at offset 0x25
    header.pc = data[0x25] | (data[0x26] << 8);
    header.a = data[0x27];
    header.x = data[0x28];
    header.y = data[0x29];
    header.psw = data[0x2A];
    header.sp = data[0x2B];

    return true;
}

void SPCFile::printMetadata() const {
    std::cout << "=== SPC File Information ===" << std::endl;
    std::cout << "PC: $" << std::hex << header.pc << std::dec << std::endl;
    std::cout << "A: $" << std::hex << (int)header.a << " X: $" << (int)header.x
              << " Y: $" << (int)header.y << std::dec << std::endl;
    std::cout << "PSW: $" << std::hex << (int)header.psw << " SP: $" << (int)header.sp
              << std::dec << std::endl;

    if (id666) {
        std::cout << std::endl;
        std::cout << "=== ID666 Metadata ===" << std::endl;
        if (!id666->songTitle.empty())
            std::cout << "Song:    " << id666->songTitle << std::endl;
        if (!id666->gameTitle.empty())
            std::cout << "Game:    " << id666->gameTitle << std::endl;
        if (!id666->artist.empty())
            std::cout << "Artist:  " << id666->artist << std::endl;
        if (!id666->dumperName.empty())
            std::cout << "Dumper:  " << id666->dumperName << std::endl;
        if (!id666->date.empty())
            std::cout << "Date:    " << id666->date << std::endl;
        if (!id666->comments.empty())
            std::cout << "Comment: " << id666->comments << std::endl;
        if (id666->playSeconds > 0)
            std::cout << "Length:  " << id666->playSeconds << " seconds" << std::endl;
        if (id666->fadeMilliseconds > 0)
            std::cout << "Fade:    " << id666->fadeMilliseconds << " ms" << std::endl;
    }
    std::cout << std::endl;
}
