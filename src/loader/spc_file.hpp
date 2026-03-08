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
#include "id666.hpp"
#include <array>
#include <string>
#include <optional>
#include <filesystem>

struct SPCHeader {
    static constexpr size_t HEADER_SIZE = 0x100;
    static constexpr const char* SIGNATURE = "SNES-SPC700 Sound File Data v0.30";

    u16 pc;
    u8 a, x, y;
    u8 psw;
    u8 sp;
    bool hasID666;
};

class SPCFile {
public:
    SPCFile() = default;

    bool load(const std::filesystem::path& path);

    const SPCHeader& getHeader() const { return header; }
    const std::array<u8, 65536>& getRAM() const { return ram; }
    const std::array<u8, 128>& getDSPRegs() const { return dspRegs; }
    const std::optional<ID666>& getID666() const { return id666; }

    void printMetadata() const;

private:
    SPCHeader header{};
    std::array<u8, 65536> ram{};
    std::array<u8, 128> dspRegs{};
    std::optional<ID666> id666;

    bool parseHeader(const u8* data);
};
