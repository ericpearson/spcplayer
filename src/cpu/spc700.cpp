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

#include "spc700.hpp"
#include "dsp/dsp.hpp"
#include <cstring>

// IPL ROM - boot code that runs on reset
const std::array<u8, 64> SPC700::iplRom = {
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0,
    0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4,
    0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB,
    0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD,
    0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

SPC700::SPC700() {
    reset();
}

void SPC700::reset() {
    state = SPC700State{};
    ram.fill(0);
    control = 0x80; // IPL ROM enabled
    dspAddr = 0;
    cpuInput.fill(0);
    cpuOutput.fill(0);
    timers = {};
    timer01Prescaler = 0;
    timer2Prescaler = 0;
    cyclesExecuted = 0;
}

void SPC700::loadState(const u8* ramData, u16 pc, u8 a, u8 x, u8 y, u8 psw, u8 sp) {
    std::memcpy(ram.data(), ramData, 65536);
    state.PC = pc;
    state.A = a;
    state.X = x;
    state.Y = y;
    state.PSW = psw;
    state.SP = sp;

    // Initialize control register from RAM snapshot
    // Keep timer enables (bits 0-2) and disable IPL ROM (bit 7)
    control = ram[0xF1] & 0x07;
    
    // Initialize timer targets from RAM
    timers[0].target = ram[0xFA];
    timers[1].target = ram[0xFB];
    timers[2].target = ram[0xFC];
    
    // Enable timers based on control register
    timers[0].enabled = (control & 0x01) != 0;
    timers[1].enabled = (control & 0x02) != 0;
    timers[2].enabled = (control & 0x04) != 0;
    
    // Initialize CPU input ports from RAM (ports 0xF4-0xF7)
    // These simulate values the SNES main CPU would have sent
    cpuInput[0] = ram[0xF4];
    cpuInput[1] = ram[0xF5];
    cpuInput[2] = ram[0xF6];
    cpuInput[3] = ram[0xF7];
    
}

u8 SPC700::read(u16 addr) {
    // Memory-mapped I/O ($F0-$FF)
    if (addr >= 0xF0 && addr <= 0xFF) {
        switch (addr) {
            case 0xF0: return 0; // TEST register (write-only)
            case 0xF1: return 0; // CONTROL (write-only)
            case 0xF2: return dspAddr;
            case 0xF3: return dsp ? dsp->read(dspAddr) : 0;
            case 0xF4: return cpuInput[0];
            case 0xF5: return cpuInput[1];
            case 0xF6: return cpuInput[2];
            case 0xF7: return cpuInput[3];
            case 0xF8: return ram[0xF8]; // Normal RAM
            case 0xF9: return ram[0xF9]; // Normal RAM
            case 0xFA: return 0; // Timer 0 target (write-only)
            case 0xFB: return 0; // Timer 1 target (write-only)
            case 0xFC: return 0; // Timer 2 target (write-only)
            case 0xFD: { // Timer 0 output
                u8 val = timers[0].counter;
                timers[0].counter = 0;
                return val;
            }
            case 0xFE: { // Timer 1 output
                u8 val = timers[1].counter;
                timers[1].counter = 0;
                return val;
            }
            case 0xFF: { // Timer 2 output
                u8 val = timers[2].counter;
                timers[2].counter = 0;
                return val;
            }
        }
    }

    // IPL ROM ($FFC0-$FFFF) when enabled
    if (addr >= 0xFFC0 && (control & 0x80)) {
        return iplRom[addr - 0xFFC0];
    }

    return ram[addr];
}

void SPC700::write(u16 addr, u8 value) {
    // Memory-mapped I/O
    if (addr >= 0xF0 && addr <= 0xFF) {
        switch (addr) {
            case 0xF0: break; // TEST register (ignore)
            case 0xF1: // CONTROL
                control = value;
                // Bit 0: Timer 0 enable
                // Bit 1: Timer 1 enable
                // Bit 2: Timer 2 enable
                timers[0].enabled = (value & 0x01);
                timers[1].enabled = (value & 0x02);
                timers[2].enabled = (value & 0x04);

                // Bit 4: Clear input ports 0,1
                if (value & 0x10) {
                    cpuInput[0] = cpuInput[1] = 0;
                }
                // Bit 5: Clear input ports 2,3
                if (value & 0x20) {
                    cpuInput[2] = cpuInput[3] = 0;
                }
                break;
            case 0xF2: dspAddr = value; break;
            case 0xF3:
                if (dsp) dsp->write(dspAddr, value);
                break;
            case 0xF4: cpuOutput[0] = value; break;
            case 0xF5: cpuOutput[1] = value; break;
            case 0xF6: cpuOutput[2] = value; break;
            case 0xF7: cpuOutput[3] = value; break;
            case 0xF8: ram[0xF8] = value; break;
            case 0xF9: ram[0xF9] = value; break;
            case 0xFA:
                timers[0].target = value;
                break;
            case 0xFB:
                timers[1].target = value;
                break;
            case 0xFC:
                timers[2].target = value;
                break;
            case 0xFD: case 0xFE: case 0xFF:
                break; // Timer outputs (read-only)
        }
        return;
    }

    ram[addr] = value;
}

void SPC700::tickTimers() {
    // Timer 0 and 1: 8000 Hz (every 128 CPU cycles at 1.024 MHz)
    timer01Prescaler++;
    if (timer01Prescaler >= 128) {
        timer01Prescaler = 0;

        for (int i = 0; i < 2; ++i) {
            if (timers[i].enabled) {
                timers[i].stage++;
                u16 target = timers[i].target ? timers[i].target : 256;
                if (timers[i].stage >= target) {
                    timers[i].stage = 0;
                    timers[i].counter = (timers[i].counter + 1) & 0x0F;
                }
            }
        }
    }

    // Timer 2: 64000 Hz (every 16 CPU cycles)
    timer2Prescaler++;
    if (timer2Prescaler >= 16) {
        timer2Prescaler = 0;

        if (timers[2].enabled) {
            timers[2].stage++;
            u16 target = timers[2].target ? timers[2].target : 256;
            if (timers[2].stage >= target) {
                timers[2].stage = 0;
                timers[2].counter = (timers[2].counter + 1) & 0x0F;
            }
        }
    }
}

int SPC700::run(int targetCycles) {
    cyclesExecuted = 0;
    while (cyclesExecuted < targetCycles) {
        cyclesExecuted += step();
    }
    return cyclesExecuted;
}

int SPC700::step() {
    u8 opcode = fetch();
    int cycles = executeOpcode(opcode);

    // Tick timers for each cycle
    for (int i = 0; i < cycles; ++i) {
        tickTimers();
    }

    return cycles;
}

u8 SPC700::fetch() {
    return read(state.PC++);
}

u16 SPC700::fetch16() {
    u8 lo = fetch();
    u8 hi = fetch();
    return lo | (hi << 8);
}

void SPC700::push(u8 value) {
    write(0x0100 | state.SP, value);
    state.SP--;
}

u8 SPC700::pop() {
    state.SP++;
    return read(0x0100 | state.SP);
}

void SPC700::push16(u16 value) {
    push(value >> 8);
    push(value & 0xFF);
}

u16 SPC700::pop16() {
    u8 lo = pop();
    u8 hi = pop();
    return lo | (hi << 8);
}

u8 SPC700::readDp(u8 offset) {
    return read(dp(offset));
}

void SPC700::writeDp(u8 offset, u8 value) {
    write(dp(offset), value);
}

u16 SPC700::readDpWord(u8 offset) {
    u8 lo = readDp(offset);
    u8 hi = readDp(offset + 1);
    return lo | (hi << 8);
}

void SPC700::writeDpWord(u8 offset, u16 value) {
    writeDp(offset, value & 0xFF);
    writeDp(offset + 1, value >> 8);
}

// ALU operations
u8 SPC700::adc(u8 a, u8 b) {
    int carry = state.C() ? 1 : 0;
    int result = a + b + carry;

    state.setC(result > 0xFF);
    state.setV(((a ^ result) & (b ^ result) & 0x80) != 0);
    state.setH(((a & 0x0F) + (b & 0x0F) + carry) > 0x0F);
    result &= 0xFF;
    state.setNZ(result);

    return result;
}

u8 SPC700::sbc(u8 a, u8 b) {
    int borrow = state.C() ? 0 : 1;
    int result = a - b - borrow;

    state.setC(result >= 0);
    state.setV(((a ^ b) & (a ^ result) & 0x80) != 0);
    state.setH(((a & 0x0F) - (b & 0x0F) - borrow) >= 0);
    result &= 0xFF;
    state.setNZ(result);

    return result;
}

void SPC700::cmp(u8 a, u8 b) {
    int result = a - b;
    state.setC(result >= 0);
    state.setNZ(result & 0xFF);
}

u8 SPC700::doAnd(u8 a, u8 b) {
    u8 result = a & b;
    state.setNZ(result);
    return result;
}

u8 SPC700::doOr(u8 a, u8 b) {
    u8 result = a | b;
    state.setNZ(result);
    return result;
}

u8 SPC700::doEor(u8 a, u8 b) {
    u8 result = a ^ b;
    state.setNZ(result);
    return result;
}

u8 SPC700::asl(u8 value) {
    state.setC(value & 0x80);
    value <<= 1;
    state.setNZ(value);
    return value;
}

u8 SPC700::lsr(u8 value) {
    state.setC(value & 0x01);
    value >>= 1;
    state.setNZ(value);
    return value;
}

u8 SPC700::rol(u8 value) {
    bool oldC = state.C();
    state.setC(value & 0x80);
    value = (value << 1) | (oldC ? 1 : 0);
    state.setNZ(value);
    return value;
}

u8 SPC700::ror(u8 value) {
    bool oldC = state.C();
    state.setC(value & 0x01);
    value = (value >> 1) | (oldC ? 0x80 : 0);
    state.setNZ(value);
    return value;
}
