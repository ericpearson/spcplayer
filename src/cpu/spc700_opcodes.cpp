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

// Cycle counts for each opcode
static const int cycleTable[256] = {
    2,8,4,5,3,4,3,6, 2,6,5,4,5,4,6,8,  // 0x
    2,8,4,5,4,5,5,6, 5,5,6,5,2,2,4,6,  // 1x
    2,8,4,5,3,4,3,6, 2,6,5,4,5,4,5,4,  // 2x
    2,8,4,5,4,5,5,6, 5,5,6,5,2,2,3,8,  // 3x
    2,8,4,5,3,4,3,6, 2,6,4,4,5,4,6,6,  // 4x
    2,8,4,5,4,5,5,6, 5,5,4,5,2,2,4,3,  // 5x
    2,8,4,5,3,4,3,6, 2,6,4,4,5,4,5,5,  // 6x
    2,8,4,5,4,5,5,6, 5,5,5,5,2,2,3,6,  // 7x
    2,8,4,5,3,4,3,6, 2,6,5,4,5,2,4,5,  // 8x
    2,8,4,5,4,5,5,6, 5,5,5,5,2,2,12,5, // 9x
    3,8,4,5,3,4,3,6, 2,6,4,4,5,2,4,4,  // Ax
    2,8,4,5,4,5,5,6, 5,5,5,5,2,2,3,4,  // Bx
    3,8,4,5,4,5,4,7, 2,5,6,4,5,2,4,9,  // Cx
    2,8,4,5,5,6,6,7, 4,5,4,5,2,2,6,3,  // Dx
    2,8,4,5,3,4,3,6, 2,4,5,3,4,3,4,3,  // Ex (E4 changed from 2 to 3)
    2,8,4,5,4,5,5,6, 3,4,5,4,2,2,4,3   // Fx
};

int SPC700::executeOpcode(u8 opcode) {
    int cycles = cycleTable[opcode];

    switch (opcode) {
        // 0x00: NOP
        case 0x00: break;

        // 0x01: TCALL 0
        case 0x01: {
            push16(state.PC);
            u16 addr = read(0xFFDE) | (read(0xFFDF) << 8);
            state.PC = addr;
            break;
        }

        // 0x02: SET1 d.0
        case 0x02: { u8 d = fetch(); writeDp(d, readDp(d) | 0x01); break; }

        // 0x03: BBS d.0, r
        case 0x03: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x01) state.PC += r;
            break;
        }

        // 0x04: OR A, d
        case 0x04: { u8 d = fetch(); state.A = doOr(state.A, readDp(d)); break; }

        // 0x05: OR A, !a
        case 0x05: { u16 a = fetch16(); state.A = doOr(state.A, read(a)); break; }

        // 0x06: OR A, (X)
        case 0x06: { state.A = doOr(state.A, read(dp(state.X))); break; }

        // 0x07: OR A, [d+X]
        case 0x07: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            state.A = doOr(state.A, read(addr));
            break;
        }

        // 0x08: OR A, #i
        case 0x08: { u8 i = fetch(); state.A = doOr(state.A, i); break; }

        // 0x09: OR dd, ds
        case 0x09: {
            u8 ds = fetch();
            u8 dd = fetch();
            writeDp(dd, doOr(readDp(dd), readDp(ds)));
            break;
        }

        // 0x0A: OR1 C, m.b
        case 0x0A: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            bool result = state.C() | ((read(addr) >> bit) & 1);
            state.setC(result);
            break;
        }

        // 0x0B: ASL d
        case 0x0B: { u8 d = fetch(); writeDp(d, asl(readDp(d))); break; }

        // 0x0C: ASL !a
        case 0x0C: { u16 a = fetch16(); write(a, asl(read(a))); break; }

        // 0x0D: PUSH PSW
        case 0x0D: push(state.PSW); break;

        // 0x0E: TSET1 !a
        case 0x0E: {
            u16 a = fetch16();
            u8 val = read(a);
            state.setNZ(state.A - val);
            write(a, val | state.A);
            break;
        }

        // 0x0F: BRK
        case 0x0F: {
            push16(state.PC);
            push(state.PSW);
            state.setB(true);
            state.setI(false);
            state.PC = read(0xFFDE) | (read(0xFFDF) << 8);
            break;
        }

        // 0x10: BPL r
        case 0x10: { s8 r = static_cast<s8>(fetch()); if (!state.N()) { state.PC += r; cycles += 2; } break; }

        // 0x11: TCALL 1
        case 0x11: { push16(state.PC); state.PC = read(0xFFDC) | (read(0xFFDD) << 8); break; }

        // 0x12: CLR1 d.0
        case 0x12: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x01); break; }

        // 0x13: BBC d.0, r
        case 0x13: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x01)) state.PC += r;
            break;
        }

        // 0x14: OR A, d+X
        case 0x14: { u8 d = fetch(); state.A = doOr(state.A, readDp(d + state.X)); break; }

        // 0x15: OR A, !a+X
        case 0x15: { u16 a = fetch16(); state.A = doOr(state.A, read(a + state.X)); break; }

        // 0x16: OR A, !a+Y
        case 0x16: { u16 a = fetch16(); state.A = doOr(state.A, read(a + state.Y)); break; }

        // 0x17: OR A, [d]+Y
        case 0x17: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            state.A = doOr(state.A, read(addr + state.Y));
            break;
        }

        // 0x18: OR d, #i
        case 0x18: {
            u8 i = fetch();
            u8 d = fetch();
            writeDp(d, doOr(readDp(d), i));
            break;
        }

        // 0x19: OR (X), (Y)
        case 0x19: {
            u8 y = read(dp(state.Y));
            u8 x = readDp(state.X);
            writeDp(state.X, doOr(x, y));
            break;
        }

        // 0x1A: DECW d
        case 0x1A: {
            u8 d = fetch();
            u16 val = readDpWord(d) - 1;
            writeDpWord(d, val);
            state.setN(val & 0x8000);
            state.setZ(val == 0);
            break;
        }

        // 0x1B: ASL d+X
        case 0x1B: { u8 d = fetch(); writeDp(d + state.X, asl(readDp(d + state.X))); break; }

        // 0x1C: ASL A
        case 0x1C: state.A = asl(state.A); break;

        // 0x1D: DEC X
        case 0x1D: state.X--; state.setNZ(state.X); break;

        // 0x1E: CMP X, !a
        case 0x1E: { u16 a = fetch16(); cmp(state.X, read(a)); break; }

        // 0x1F: JMP [!a+X]
        case 0x1F: {
            u16 a = fetch16() + state.X;
            state.PC = read(a) | (read(a + 1) << 8);
            break;
        }

        // 0x20: CLRP
        case 0x20: state.setP(false); break;

        // 0x21: TCALL 2
        case 0x21: { push16(state.PC); state.PC = read(0xFFDA) | (read(0xFFDB) << 8); break; }

        // 0x22: SET1 d.1
        case 0x22: { u8 d = fetch(); writeDp(d, readDp(d) | 0x02); break; }

        // 0x23: BBS d.1, r
        case 0x23: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x02) state.PC += r;
            break;
        }

        // 0x24: AND A, d
        case 0x24: { u8 d = fetch(); state.A = doAnd(state.A, readDp(d)); break; }

        // 0x25: AND A, !a
        case 0x25: { u16 a = fetch16(); state.A = doAnd(state.A, read(a)); break; }

        // 0x26: AND A, (X)
        case 0x26: { state.A = doAnd(state.A, read(dp(state.X))); break; }

        // 0x27: AND A, [d+X]
        case 0x27: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            state.A = doAnd(state.A, read(addr));
            break;
        }

        // 0x28: AND A, #i
        case 0x28: { u8 i = fetch(); state.A = doAnd(state.A, i); break; }

        // 0x29: AND dd, ds
        case 0x29: {
            u8 ds = fetch();
            u8 dd = fetch();
            writeDp(dd, doAnd(readDp(dd), readDp(ds)));
            break;
        }

        // 0x2A: OR1 C, /m.b
        case 0x2A: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            bool result = state.C() | !((read(addr) >> bit) & 1);
            state.setC(result);
            break;
        }

        // 0x2B: ROL d
        case 0x2B: { u8 d = fetch(); writeDp(d, rol(readDp(d))); break; }

        // 0x2C: ROL !a
        case 0x2C: { u16 a = fetch16(); write(a, rol(read(a))); break; }

        // 0x2D: PUSH A
        case 0x2D: push(state.A); break;

        // 0x2E: CBNE d, r
        case 0x2E: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (state.A != readDp(d)) { state.PC += r; cycles += 2; }
            break;
        }

        // 0x2F: BRA r
        case 0x2F: { s8 r = static_cast<s8>(fetch()); state.PC += r; break; }

        // 0x30: BMI r
        case 0x30: { s8 r = static_cast<s8>(fetch()); if (state.N()) { state.PC += r; cycles += 2; } break; }

        // 0x31: TCALL 3
        case 0x31: { push16(state.PC); state.PC = read(0xFFD8) | (read(0xFFD9) << 8); break; }

        // 0x32: CLR1 d.1
        case 0x32: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x02); break; }

        // 0x33: BBC d.1, r
        case 0x33: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x02)) state.PC += r;
            break;
        }

        // 0x34: AND A, d+X
        case 0x34: { u8 d = fetch(); state.A = doAnd(state.A, readDp(d + state.X)); break; }

        // 0x35: AND A, !a+X
        case 0x35: { u16 a = fetch16(); state.A = doAnd(state.A, read(a + state.X)); break; }

        // 0x36: AND A, !a+Y
        case 0x36: { u16 a = fetch16(); state.A = doAnd(state.A, read(a + state.Y)); break; }

        // 0x37: AND A, [d]+Y
        case 0x37: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            state.A = doAnd(state.A, read(addr + state.Y));
            break;
        }

        // 0x38: AND d, #i
        case 0x38: {
            u8 i = fetch();
            u8 d = fetch();
            writeDp(d, doAnd(readDp(d), i));
            break;
        }

        // 0x39: AND (X), (Y)
        case 0x39: {
            u8 y = read(dp(state.Y));
            u8 x = readDp(state.X);
            writeDp(state.X, doAnd(x, y));
            break;
        }

        // 0x3A: INCW d
        case 0x3A: {
            u8 d = fetch();
            u16 val = readDpWord(d) + 1;
            writeDpWord(d, val);
            state.setN(val & 0x8000);
            state.setZ(val == 0);
            break;
        }

        // 0x3B: ROL d+X
        case 0x3B: { u8 d = fetch(); writeDp(d + state.X, rol(readDp(d + state.X))); break; }

        // 0x3C: ROL A
        case 0x3C: state.A = rol(state.A); break;

        // 0x3D: INC X
        case 0x3D: state.X++; state.setNZ(state.X); break;

        // 0x3E: CMP X, d
        case 0x3E: { u8 d = fetch(); cmp(state.X, readDp(d)); break; }

        // 0x3F: CALL !a
        case 0x3F: {
            u16 a = fetch16();
            push16(state.PC);
            state.PC = a;
            break;
        }

        // 0x40: SETP
        case 0x40: state.setP(true); break;

        // 0x41: TCALL 4
        case 0x41: { push16(state.PC); state.PC = read(0xFFD6) | (read(0xFFD7) << 8); break; }

        // 0x42: SET1 d.2
        case 0x42: { u8 d = fetch(); writeDp(d, readDp(d) | 0x04); break; }

        // 0x43: BBS d.2, r
        case 0x43: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x04) state.PC += r;
            break;
        }

        // 0x44: EOR A, d
        case 0x44: { u8 d = fetch(); state.A = doEor(state.A, readDp(d)); break; }

        // 0x45: EOR A, !a
        case 0x45: { u16 a = fetch16(); state.A = doEor(state.A, read(a)); break; }

        // 0x46: EOR A, (X)
        case 0x46: { state.A = doEor(state.A, read(dp(state.X))); break; }

        // 0x47: EOR A, [d+X]
        case 0x47: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            state.A = doEor(state.A, read(addr));
            break;
        }

        // 0x48: EOR A, #i
        case 0x48: { u8 i = fetch(); state.A = doEor(state.A, i); break; }

        // 0x49: EOR dd, ds
        case 0x49: {
            u8 ds = fetch();
            u8 dd = fetch();
            writeDp(dd, doEor(readDp(dd), readDp(ds)));
            break;
        }

        // 0x4A: AND1 C, m.b
        case 0x4A: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            bool result = state.C() & ((read(addr) >> bit) & 1);
            state.setC(result);
            break;
        }

        // 0x4B: LSR d
        case 0x4B: { u8 d = fetch(); writeDp(d, lsr(readDp(d))); break; }

        // 0x4C: LSR !a
        case 0x4C: { u16 a = fetch16(); write(a, lsr(read(a))); break; }

        // 0x4D: PUSH X
        case 0x4D: push(state.X); break;

        // 0x4E: TCLR1 !a
        case 0x4E: {
            u16 a = fetch16();
            u8 val = read(a);
            state.setNZ(state.A - val);
            write(a, val & ~state.A);
            break;
        }

        // 0x4F: PCALL u
        case 0x4F: {
            u8 u = fetch();
            push16(state.PC);
            state.PC = 0xFF00 | u;
            break;
        }

        // 0x50: BVC r
        case 0x50: { s8 r = static_cast<s8>(fetch()); if (!state.V()) { state.PC += r; cycles += 2; } break; }

        // 0x51: TCALL 5
        case 0x51: { push16(state.PC); state.PC = read(0xFFD4) | (read(0xFFD5) << 8); break; }

        // 0x52: CLR1 d.2
        case 0x52: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x04); break; }

        // 0x53: BBC d.2, r
        case 0x53: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x04)) state.PC += r;
            break;
        }

        // 0x54: EOR A, d+X
        case 0x54: { u8 d = fetch(); state.A = doEor(state.A, readDp(d + state.X)); break; }

        // 0x55: EOR A, !a+X
        case 0x55: { u16 a = fetch16(); state.A = doEor(state.A, read(a + state.X)); break; }

        // 0x56: EOR A, !a+Y
        case 0x56: { u16 a = fetch16(); state.A = doEor(state.A, read(a + state.Y)); break; }

        // 0x57: EOR A, [d]+Y
        case 0x57: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            state.A = doEor(state.A, read(addr + state.Y));
            break;
        }

        // 0x58: EOR d, #i
        case 0x58: {
            u8 i = fetch();
            u8 d = fetch();
            writeDp(d, doEor(readDp(d), i));
            break;
        }

        // 0x59: EOR (X), (Y)
        case 0x59: {
            u8 y = read(dp(state.Y));
            u8 x = readDp(state.X);
            writeDp(state.X, doEor(x, y));
            break;
        }

        // 0x5A: CMPW YA, d
        case 0x5A: {
            u8 d = fetch();
            u16 ya = state.A | (state.Y << 8);
            u16 val = readDpWord(d);
            s32 result = ya - val;
            state.setC(result >= 0);
            state.setN(result & 0x8000);
            state.setZ((result & 0xFFFF) == 0);
            break;
        }

        // 0x5B: LSR d+X
        case 0x5B: { u8 d = fetch(); writeDp(d + state.X, lsr(readDp(d + state.X))); break; }

        // 0x5C: LSR A
        case 0x5C: state.A = lsr(state.A); break;

        // 0x5D: MOV X, A
        case 0x5D: state.X = state.A; state.setNZ(state.X); break;

        // 0x5E: CMP Y, !a
        case 0x5E: { u16 a = fetch16(); cmp(state.Y, read(a)); break; }

        // 0x5F: JMP !a
        case 0x5F: state.PC = fetch16(); break;

        // 0x60: CLRC
        case 0x60: state.setC(false); break;

        // 0x61: TCALL 6
        case 0x61: { push16(state.PC); state.PC = read(0xFFD2) | (read(0xFFD3) << 8); break; }

        // 0x62: SET1 d.3
        case 0x62: { u8 d = fetch(); writeDp(d, readDp(d) | 0x08); break; }

        // 0x63: BBS d.3, r
        case 0x63: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x08) state.PC += r;
            break;
        }

        // 0x64: CMP A, d
        case 0x64: { u8 d = fetch(); cmp(state.A, readDp(d)); break; }

        // 0x65: CMP A, !a
        case 0x65: { u16 a = fetch16(); cmp(state.A, read(a)); break; }

        // 0x66: CMP A, (X)
        case 0x66: { cmp(state.A, read(dp(state.X))); break; }

        // 0x67: CMP A, [d+X]
        case 0x67: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            cmp(state.A, read(addr));
            break;
        }

        // 0x68: CMP A, #i
        case 0x68: { u8 i = fetch(); cmp(state.A, i); break; }

        // 0x69: CMP dd, ds
        case 0x69: {
            u8 ds = fetch();
            u8 dd = fetch();
            cmp(readDp(dd), readDp(ds));
            break;
        }

        // 0x6A: AND1 C, /m.b
        case 0x6A: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            bool result = state.C() & !((read(addr) >> bit) & 1);
            state.setC(result);
            break;
        }

        // 0x6B: ROR d
        case 0x6B: { u8 d = fetch(); writeDp(d, ror(readDp(d))); break; }

        // 0x6C: ROR !a
        case 0x6C: { u16 a = fetch16(); write(a, ror(read(a))); break; }

        // 0x6D: PUSH Y
        case 0x6D: push(state.Y); break;

        // 0x6E: DBNZ d, r
        case 0x6E: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            u8 val = readDp(d) - 1;
            writeDp(d, val);
            if (val != 0) { state.PC += r; cycles += 2; }
            break;
        }

        // 0x6F: RET
        case 0x6F: state.PC = pop16(); break;

        // 0x70: BVS r
        case 0x70: { s8 r = static_cast<s8>(fetch()); if (state.V()) { state.PC += r; cycles += 2; } break; }

        // 0x71: TCALL 7
        case 0x71: { push16(state.PC); state.PC = read(0xFFD0) | (read(0xFFD1) << 8); break; }

        // 0x72: CLR1 d.3
        case 0x72: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x08); break; }

        // 0x73: BBC d.3, r
        case 0x73: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x08)) state.PC += r;
            break;
        }

        // 0x74: CMP A, d+X
        case 0x74: { u8 d = fetch(); cmp(state.A, readDp(d + state.X)); break; }

        // 0x75: CMP A, !a+X
        case 0x75: { u16 a = fetch16(); cmp(state.A, read(a + state.X)); break; }

        // 0x76: CMP A, !a+Y
        case 0x76: { u16 a = fetch16(); cmp(state.A, read(a + state.Y)); break; }

        // 0x77: CMP A, [d]+Y
        case 0x77: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            cmp(state.A, read(addr + state.Y));
            break;
        }

        // 0x78: CMP d, #i
        case 0x78: {
            u8 i = fetch();
            u8 d = fetch();
            cmp(readDp(d), i);
            break;
        }

        // 0x79: CMP (X), (Y)
        case 0x79: {
            cmp(readDp(state.X), read(dp(state.Y)));
            break;
        }

        // 0x7A: ADDW YA, d
        case 0x7A: {
            u8 d = fetch();
            u16 ya = state.A | (state.Y << 8);
            u16 val = readDpWord(d);
            u32 result = ya + val;

            state.setC(result > 0xFFFF);
            state.setV(((ya ^ result) & (val ^ result) & 0x8000) != 0);
            state.setH(((ya & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF);

            state.A = result & 0xFF;
            state.Y = (result >> 8) & 0xFF;
            state.setN(result & 0x8000);
            state.setZ((result & 0xFFFF) == 0);
            break;
        }

        // 0x7B: ROR d+X
        case 0x7B: { u8 d = fetch(); writeDp(d + state.X, ror(readDp(d + state.X))); break; }

        // 0x7C: ROR A
        case 0x7C: state.A = ror(state.A); break;

        // 0x7D: MOV A, X
        case 0x7D: state.A = state.X; state.setNZ(state.A); break;

        // 0x7E: CMP Y, d
        case 0x7E: { u8 d = fetch(); cmp(state.Y, readDp(d)); break; }

        // 0x7F: RETI
        case 0x7F: {
            state.PSW = pop();
            state.PC = pop16();
            break;
        }

        // 0x80: SETC
        case 0x80: state.setC(true); break;

        // 0x81: TCALL 8
        case 0x81: { push16(state.PC); state.PC = read(0xFFCE) | (read(0xFFCF) << 8); break; }

        // 0x82: SET1 d.4
        case 0x82: { u8 d = fetch(); writeDp(d, readDp(d) | 0x10); break; }

        // 0x83: BBS d.4, r
        case 0x83: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x10) state.PC += r;
            break;
        }

        // 0x84: ADC A, d
        case 0x84: { u8 d = fetch(); state.A = adc(state.A, readDp(d)); break; }

        // 0x85: ADC A, !a
        case 0x85: { u16 a = fetch16(); state.A = adc(state.A, read(a)); break; }

        // 0x86: ADC A, (X)
        case 0x86: { state.A = adc(state.A, read(dp(state.X))); break; }

        // 0x87: ADC A, [d+X]
        case 0x87: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            state.A = adc(state.A, read(addr));
            break;
        }

        // 0x88: ADC A, #i
        case 0x88: { u8 i = fetch(); state.A = adc(state.A, i); break; }

        // 0x89: ADC dd, ds
        case 0x89: {
            u8 ds = fetch();
            u8 dd = fetch();
            writeDp(dd, adc(readDp(dd), readDp(ds)));
            break;
        }

        // 0x8A: EOR1 C, m.b
        case 0x8A: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            bool result = state.C() ^ ((read(addr) >> bit) & 1);
            state.setC(result);
            break;
        }

        // 0x8B: DEC d
        case 0x8B: { u8 d = fetch(); u8 val = readDp(d) - 1; writeDp(d, val); state.setNZ(val); break; }

        // 0x8C: DEC !a
        case 0x8C: { u16 a = fetch16(); u8 val = read(a) - 1; write(a, val); state.setNZ(val); break; }

        // 0x8D: MOV Y, #i
        case 0x8D: state.Y = fetch(); state.setNZ(state.Y); break;

        // 0x8E: POP PSW
        case 0x8E: state.PSW = pop(); break;

        // 0x8F: MOV d, #i
        case 0x8F: {
            u8 i = fetch();
            u8 d = fetch();
            writeDp(d, i);
            break;
        }

        // 0x90: BCC r
        case 0x90: { s8 r = static_cast<s8>(fetch()); if (!state.C()) { state.PC += r; cycles += 2; } break; }

        // 0x91: TCALL 9
        case 0x91: { push16(state.PC); state.PC = read(0xFFCC) | (read(0xFFCD) << 8); break; }

        // 0x92: CLR1 d.4
        case 0x92: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x10); break; }

        // 0x93: BBC d.4, r
        case 0x93: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x10)) state.PC += r;
            break;
        }

        // 0x94: ADC A, d+X
        case 0x94: { u8 d = fetch(); state.A = adc(state.A, readDp(d + state.X)); break; }

        // 0x95: ADC A, !a+X
        case 0x95: { u16 a = fetch16(); state.A = adc(state.A, read(a + state.X)); break; }

        // 0x96: ADC A, !a+Y
        case 0x96: { u16 a = fetch16(); state.A = adc(state.A, read(a + state.Y)); break; }

        // 0x97: ADC A, [d]+Y
        case 0x97: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            state.A = adc(state.A, read(addr + state.Y));
            break;
        }

        // 0x98: ADC d, #i
        case 0x98: {
            u8 i = fetch();
            u8 d = fetch();
            writeDp(d, adc(readDp(d), i));
            break;
        }

        // 0x99: ADC (X), (Y)
        case 0x99: {
            u8 y = read(dp(state.Y));
            u8 x = readDp(state.X);
            writeDp(state.X, adc(x, y));
            break;
        }

        // 0x9A: SUBW YA, d
        case 0x9A: {
            u8 d = fetch();
            u16 ya = state.A | (state.Y << 8);
            u16 val = readDpWord(d);
            s32 result = ya - val;

            state.setC(result >= 0);
            state.setV(((ya ^ val) & (ya ^ result) & 0x8000) != 0);
            state.setH(((ya & 0x0FFF) - (val & 0x0FFF)) >= 0);

            state.A = result & 0xFF;
            state.Y = (result >> 8) & 0xFF;
            state.setN(result & 0x8000);
            state.setZ((result & 0xFFFF) == 0);
            break;
        }

        // 0x9B: DEC d+X
        case 0x9B: { u8 d = fetch(); u8 val = readDp(d + state.X) - 1; writeDp(d + state.X, val); state.setNZ(val); break; }

        // 0x9C: DEC A
        case 0x9C: state.A--; state.setNZ(state.A); break;

        // 0x9D: MOV X, SP
        case 0x9D: state.X = state.SP; state.setNZ(state.X); break;

        // 0x9E: DIV YA, X
        case 0x9E: {
            u16 ya = state.A | (state.Y << 8);
            state.setV(state.Y >= state.X);
            state.setH((state.Y & 0x0F) >= (state.X & 0x0F));

            if (state.Y < (state.X << 1)) {
                state.A = ya / state.X;
                state.Y = ya % state.X;
            } else {
                state.A = 255 - (ya - (state.X << 9)) / (256 - state.X);
                state.Y = state.X + (ya - (state.X << 9)) % (256 - state.X);
            }
            state.setNZ(state.A);
            break;
        }

        // 0x9F: XCN A
        case 0x9F: state.A = (state.A >> 4) | (state.A << 4); state.setNZ(state.A); break;

        // 0xA0: EI
        case 0xA0: state.setI(true); break;

        // 0xA1: TCALL 10
        case 0xA1: { push16(state.PC); state.PC = read(0xFFCA) | (read(0xFFCB) << 8); break; }

        // 0xA2: SET1 d.5
        case 0xA2: { u8 d = fetch(); writeDp(d, readDp(d) | 0x20); break; }

        // 0xA3: BBS d.5, r
        case 0xA3: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x20) state.PC += r;
            break;
        }

        // 0xA4: SBC A, d
        case 0xA4: { u8 d = fetch(); state.A = sbc(state.A, readDp(d)); break; }

        // 0xA5: SBC A, !a
        case 0xA5: { u16 a = fetch16(); state.A = sbc(state.A, read(a)); break; }

        // 0xA6: SBC A, (X)
        case 0xA6: { state.A = sbc(state.A, read(dp(state.X))); break; }

        // 0xA7: SBC A, [d+X]
        case 0xA7: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            state.A = sbc(state.A, read(addr));
            break;
        }

        // 0xA8: SBC A, #i
        case 0xA8: { u8 i = fetch(); state.A = sbc(state.A, i); break; }

        // 0xA9: SBC dd, ds
        case 0xA9: {
            u8 ds = fetch();
            u8 dd = fetch();
            writeDp(dd, sbc(readDp(dd), readDp(ds)));
            break;
        }

        // 0xAA: MOV1 C, m.b
        case 0xAA: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            state.setC((read(addr) >> bit) & 1);
            break;
        }

        // 0xAB: INC d
        case 0xAB: { u8 d = fetch(); u8 val = readDp(d) + 1; writeDp(d, val); state.setNZ(val); break; }

        // 0xAC: INC !a
        case 0xAC: { u16 a = fetch16(); u8 val = read(a) + 1; write(a, val); state.setNZ(val); break; }

        // 0xAD: CMP Y, #i
        case 0xAD: { u8 i = fetch(); cmp(state.Y, i); break; }

        // 0xAE: POP A
        case 0xAE: state.A = pop(); break;

        // 0xAF: MOV (X)+, A
        case 0xAF: write(dp(state.X++), state.A); break;

        // 0xB0: BCS r
        case 0xB0: { s8 r = static_cast<s8>(fetch()); if (state.C()) { state.PC += r; cycles += 2; } break; }

        // 0xB1: TCALL 11
        case 0xB1: { push16(state.PC); state.PC = read(0xFFC8) | (read(0xFFC9) << 8); break; }

        // 0xB2: CLR1 d.5
        case 0xB2: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x20); break; }

        // 0xB3: BBC d.5, r
        case 0xB3: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x20)) state.PC += r;
            break;
        }

        // 0xB4: SBC A, d+X
        case 0xB4: { u8 d = fetch(); state.A = sbc(state.A, readDp(d + state.X)); break; }

        // 0xB5: SBC A, !a+X
        case 0xB5: { u16 a = fetch16(); state.A = sbc(state.A, read(a + state.X)); break; }

        // 0xB6: SBC A, !a+Y
        case 0xB6: { u16 a = fetch16(); state.A = sbc(state.A, read(a + state.Y)); break; }

        // 0xB7: SBC A, [d]+Y
        case 0xB7: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            state.A = sbc(state.A, read(addr + state.Y));
            break;
        }

        // 0xB8: SBC d, #i
        case 0xB8: {
            u8 i = fetch();
            u8 d = fetch();
            writeDp(d, sbc(readDp(d), i));
            break;
        }

        // 0xB9: SBC (X), (Y)
        case 0xB9: {
            u8 y = read(dp(state.Y));
            u8 x = readDp(state.X);
            writeDp(state.X, sbc(x, y));
            break;
        }

        // 0xBA: MOVW YA, d
        case 0xBA: {
            u8 d = fetch();
            u16 val = readDpWord(d);
            state.A = val & 0xFF;
            state.Y = val >> 8;
            state.setN(val & 0x8000);
            state.setZ(val == 0);
            break;
        }

        // 0xBB: INC d+X
        case 0xBB: { u8 d = fetch(); u8 val = readDp(d + state.X) + 1; writeDp(d + state.X, val); state.setNZ(val); break; }

        // 0xBC: INC A
        case 0xBC: state.A++; state.setNZ(state.A); break;

        // 0xBD: MOV SP, X
        case 0xBD: state.SP = state.X; break;

        // 0xBE: DAS A
        case 0xBE: {
            if (!state.C() || state.A > 0x99) {
                state.A -= 0x60;
                state.setC(false);
            }
            if (!state.H() || (state.A & 0x0F) > 0x09) {
                state.A -= 0x06;
            }
            state.setNZ(state.A);
            break;
        }

        // 0xBF: MOV A, (X)+
        case 0xBF: state.A = read(dp(state.X++)); state.setNZ(state.A); break;

        // 0xC0: DI
        case 0xC0: state.setI(false); break;

        // 0xC1: TCALL 12
        case 0xC1: { push16(state.PC); state.PC = read(0xFFC6) | (read(0xFFC7) << 8); break; }

        // 0xC2: SET1 d.6
        case 0xC2: { u8 d = fetch(); writeDp(d, readDp(d) | 0x40); break; }

        // 0xC3: BBS d.6, r
        case 0xC3: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x40) state.PC += r;
            break;
        }

        // 0xC4: MOV d, A
        case 0xC4: { u8 d = fetch(); writeDp(d, state.A); break; }

        // 0xC5: MOV !a, A
        case 0xC5: { u16 a = fetch16(); write(a, state.A); break; }

        // 0xC6: MOV (X), A
        case 0xC6: write(dp(state.X), state.A); break;

        // 0xC7: MOV [d+X], A
        case 0xC7: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            write(addr, state.A);
            break;
        }

        // 0xC8: CMP X, #i
        case 0xC8: { u8 i = fetch(); cmp(state.X, i); break; }

        // 0xC9: MOV !a, X
        case 0xC9: { u16 a = fetch16(); write(a, state.X); break; }

        // 0xCA: MOV1 m.b, C
        case 0xCA: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            u8 val = read(addr);
            if (state.C()) val |= (1 << bit);
            else val &= ~(1 << bit);
            write(addr, val);
            break;
        }

        // 0xCB: MOV d, Y
        case 0xCB: { u8 d = fetch(); writeDp(d, state.Y); break; }

        // 0xCC: MOV !a, Y
        case 0xCC: { u16 a = fetch16(); write(a, state.Y); break; }

        // 0xCD: MOV X, #i
        case 0xCD: state.X = fetch(); state.setNZ(state.X); break;

        // 0xCE: POP X
        case 0xCE: state.X = pop(); break;

        // 0xCF: MUL YA
        case 0xCF: {
            u16 result = state.Y * state.A;
            state.A = result & 0xFF;
            state.Y = result >> 8;
            state.setNZ(state.Y);
            break;
        }

        // 0xD0: BNE r
        case 0xD0: { s8 r = static_cast<s8>(fetch()); if (!state.Z()) { state.PC += r; cycles += 2; } break; }

        // 0xD1: TCALL 13
        case 0xD1: { push16(state.PC); state.PC = read(0xFFC4) | (read(0xFFC5) << 8); break; }

        // 0xD2: CLR1 d.6
        case 0xD2: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x40); break; }

        // 0xD3: BBC d.6, r
        case 0xD3: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x40)) state.PC += r;
            break;
        }

        // 0xD4: MOV d+X, A
        case 0xD4: { u8 d = fetch(); writeDp(d + state.X, state.A); break; }

        // 0xD5: MOV !a+X, A
        case 0xD5: { u16 a = fetch16(); write(a + state.X, state.A); break; }

        // 0xD6: MOV !a+Y, A
        case 0xD6: { u16 a = fetch16(); write(a + state.Y, state.A); break; }

        // 0xD7: MOV [d]+Y, A
        case 0xD7: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            write(addr + state.Y, state.A);
            break;
        }

        // 0xD8: MOV d, X
        case 0xD8: { u8 d = fetch(); writeDp(d, state.X); break; }

        // 0xD9: MOV d+Y, X
        case 0xD9: { u8 d = fetch(); writeDp(d + state.Y, state.X); break; }

        // 0xDA: MOVW d, YA
        case 0xDA: {
            u8 d = fetch();
            writeDpWord(d, state.A | (state.Y << 8));
            break;
        }

        // 0xDB: MOV d+X, Y
        case 0xDB: { u8 d = fetch(); writeDp(d + state.X, state.Y); break; }

        // 0xDC: DEC Y
        case 0xDC: state.Y--; state.setNZ(state.Y); break;

        // 0xDD: MOV A, Y
        case 0xDD: state.A = state.Y; state.setNZ(state.A); break;

        // 0xDE: CBNE d+X, r
        case 0xDE: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (state.A != readDp(d + state.X)) { state.PC += r; cycles += 2; }
            break;
        }

        // 0xDF: DAA A
        case 0xDF: {
            if (state.C() || state.A > 0x99) {
                state.A += 0x60;
                state.setC(true);
            }
            if (state.H() || (state.A & 0x0F) > 0x09) {
                state.A += 0x06;
            }
            state.setNZ(state.A);
            break;
        }

        // 0xE0: CLRV
        case 0xE0: state.setV(false); state.setH(false); break;

        // 0xE1: TCALL 14
        case 0xE1: { push16(state.PC); state.PC = read(0xFFC2) | (read(0xFFC3) << 8); break; }

        // 0xE2: SET1 d.7
        case 0xE2: { u8 d = fetch(); writeDp(d, readDp(d) | 0x80); break; }

        // 0xE3: BBS d.7, r
        case 0xE3: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (readDp(d) & 0x80) state.PC += r;
            break;
        }

        // 0xE4: MOV A, d
        case 0xE4: { u8 d = fetch(); state.A = readDp(d); state.setNZ(state.A); break; }

        // 0xE5: MOV A, !a
        case 0xE5: { u16 a = fetch16(); state.A = read(a); state.setNZ(state.A); break; }

        // 0xE6: MOV A, (X)
        case 0xE6: state.A = read(dp(state.X)); state.setNZ(state.A); break;

        // 0xE7: MOV A, [d+X]
        case 0xE7: {
            u8 d = fetch();
            u16 addr = readDpWord(d + state.X);
            state.A = read(addr);
            state.setNZ(state.A);
            break;
        }

        // 0xE8: MOV A, #i
        case 0xE8: state.A = fetch(); state.setNZ(state.A); break;

        // 0xE9: MOV X, !a
        case 0xE9: { u16 a = fetch16(); state.X = read(a); state.setNZ(state.X); break; }

        // 0xEA: NOT1 m.b
        case 0xEA: {
            u16 addr = fetch16();
            u8 bit = addr >> 13;
            addr &= 0x1FFF;
            write(addr, read(addr) ^ (1 << bit));
            break;
        }

        // 0xEB: MOV Y, d
        case 0xEB: { u8 d = fetch(); state.Y = readDp(d); state.setNZ(state.Y); break; }

        // 0xEC: MOV Y, !a
        case 0xEC: { u16 a = fetch16(); state.Y = read(a); state.setNZ(state.Y); break; }

        // 0xED: NOTC
        case 0xED: state.setC(!state.C()); break;

        // 0xEE: POP Y
        case 0xEE: state.Y = pop(); break;

        // 0xEF: SLEEP
        case 0xEF: state.PC--; break; // Infinite loop

        // 0xF0: BEQ r
        case 0xF0: { s8 r = static_cast<s8>(fetch()); if (state.Z()) { state.PC += r; cycles += 2; } break; }

        // 0xF1: TCALL 15
        case 0xF1: { push16(state.PC); state.PC = read(0xFFC0) | (read(0xFFC1) << 8); break; }

        // 0xF2: CLR1 d.7
        case 0xF2: { u8 d = fetch(); writeDp(d, readDp(d) & ~0x80); break; }

        // 0xF3: BBC d.7, r
        case 0xF3: {
            u8 d = fetch();
            s8 r = static_cast<s8>(fetch());
            if (!(readDp(d) & 0x80)) state.PC += r;
            break;
        }

        // 0xF4: MOV A, d+X
        case 0xF4: { u8 d = fetch(); state.A = readDp(d + state.X); state.setNZ(state.A); break; }

        // 0xF5: MOV A, !a+X
        case 0xF5: { u16 a = fetch16(); state.A = read(a + state.X); state.setNZ(state.A); break; }

        // 0xF6: MOV A, !a+Y
        case 0xF6: { u16 a = fetch16(); state.A = read(a + state.Y); state.setNZ(state.A); break; }

        // 0xF7: MOV A, [d]+Y
        case 0xF7: {
            u8 d = fetch();
            u16 addr = readDpWord(d);
            state.A = read(addr + state.Y);
            state.setNZ(state.A);
            break;
        }

        // 0xF8: MOV X, d
        case 0xF8: { u8 d = fetch(); state.X = readDp(d); state.setNZ(state.X); break; }

        // 0xF9: MOV X, d+Y
        case 0xF9: { u8 d = fetch(); state.X = readDp(d + state.Y); state.setNZ(state.X); break; }

        // 0xFA: MOV dd, ds
        case 0xFA: {
            u8 ds = fetch();
            u8 dd = fetch();
            writeDp(dd, readDp(ds));
            break;
        }

        // 0xFB: MOV Y, d+X
        case 0xFB: { u8 d = fetch(); state.Y = readDp(d + state.X); state.setNZ(state.Y); break; }

        // 0xFC: INC Y
        case 0xFC: state.Y++; state.setNZ(state.Y); break;

        // 0xFD: MOV Y, A
        case 0xFD: state.Y = state.A; state.setNZ(state.Y); break;

        // 0xFE: DBNZ Y, r
        case 0xFE: {
            s8 r = static_cast<s8>(fetch());
            state.Y--;
            if (state.Y != 0) { state.PC += r; cycles += 2; }
            break;
        }

        // 0xFF: STOP
        case 0xFF: state.PC--; break; // Infinite loop

        default:
            break;
    }

    return cycles;
}
