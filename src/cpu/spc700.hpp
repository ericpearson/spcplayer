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
#include <array>
#include <functional>

class DSP;

struct SPC700State {
    u8 A = 0;       // Accumulator
    u8 X = 0;       // Index X
    u8 Y = 0;       // Index Y
    u8 SP = 0xEF;   // Stack pointer
    u16 PC = 0xFFC0;// Program counter
    u8 PSW = 0;     // Processor status word

    // PSW flags: N V P B H I Z C (bits 7-0)
    bool N() const { return PSW & 0x80; }
    bool V() const { return PSW & 0x40; }
    bool P() const { return PSW & 0x20; }
    bool B() const { return PSW & 0x10; }
    bool H() const { return PSW & 0x08; }
    bool I() const { return PSW & 0x04; }
    bool Z() const { return PSW & 0x02; }
    bool C() const { return PSW & 0x01; }

    void setN(bool v) { PSW = (PSW & ~0x80) | (v ? 0x80 : 0); }
    void setV(bool v) { PSW = (PSW & ~0x40) | (v ? 0x40 : 0); }
    void setP(bool v) { PSW = (PSW & ~0x20) | (v ? 0x20 : 0); }
    void setB(bool v) { PSW = (PSW & ~0x10) | (v ? 0x10 : 0); }
    void setH(bool v) { PSW = (PSW & ~0x08) | (v ? 0x08 : 0); }
    void setI(bool v) { PSW = (PSW & ~0x04) | (v ? 0x04 : 0); }
    void setZ(bool v) { PSW = (PSW & ~0x02) | (v ? 0x02 : 0); }
    void setC(bool v) { PSW = (PSW & ~0x01) | (v ? 0x01 : 0); }

    void setNZ(u8 value) {
        setN(value & 0x80);
        setZ(value == 0);
    }
};

struct TimerState {
    u8 target = 0;      // Target value (divisor)
    u8 counter = 0;     // 4-bit output counter
    u8 stage = 0;       // Internal stage
    bool enabled = false;
};

class SPC700 {
public:
    SPC700();

    void reset();
    void loadState(const u8* ramData, u16 pc, u8 a, u8 x, u8 y, u8 psw, u8 sp);

    // Run for target cycles, returns actual cycles executed
    int run(int targetCycles);

    // Execute single instruction
    int step();

    // Memory access
    u8 read(u16 addr);
    void write(u16 addr, u8 value);

    // Direct page helper
    u16 dp(u8 offset) const {
        return (state.P() ? 0x0100 : 0x0000) + offset;
    }

    // DSP connection
    void setDSP(DSP* d) { dsp = d; }

    // Tick timers (called each CPU cycle)
    void tickTimers();

    // Access state
    const SPC700State& getState() const { return state; }
    u8* getRAM() { return ram.data(); }
    const u8* getRAM() const { return ram.data(); }

private:
    SPC700State state;
    std::array<u8, 65536> ram{};

    // I/O registers
    u8 control = 0;         // $F1
    u8 dspAddr = 0;         // $F2
    std::array<u8, 4> cpuInput{};   // $F4-$F7 from main CPU
    std::array<u8, 4> cpuOutput{};  // $F4-$F7 to main CPU

    // Timers
    std::array<TimerState, 3> timers{};
    int timer01Prescaler = 0;
    int timer2Prescaler = 0;

    DSP* dsp = nullptr;
    int cyclesExecuted = 0;

    // IPL ROM (64 bytes)
    static const std::array<u8, 64> iplRom;

    // Instruction helpers
    u8 fetch();
    u16 fetch16();
    void push(u8 value);
    u8 pop();
    void push16(u16 value);
    u16 pop16();

    // Addressing mode helpers
    u8 readDp(u8 offset);
    void writeDp(u8 offset, u8 value);
    u16 readDpWord(u8 offset);
    void writeDpWord(u8 offset, u16 value);

    // ALU operations
    u8 adc(u8 a, u8 b);
    u8 sbc(u8 a, u8 b);
    void cmp(u8 a, u8 b);
    u8 doAnd(u8 a, u8 b);
    u8 doOr(u8 a, u8 b);
    u8 doEor(u8 a, u8 b);
    u8 asl(u8 value);
    u8 lsr(u8 value);
    u8 rol(u8 value);
    u8 ror(u8 value);

    // Execute opcode
    int executeOpcode(u8 opcode);
};
