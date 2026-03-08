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

#include "dsp/dsp.hpp"
#include "cpu/spc700.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>

// Gaussian interpolation table (512 entries)
static const s16 gaussTable[512] = {
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
       1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,
       2,    2,    3,    3,    3,    3,    3,    4,    4,    4,    4,    4,    5,    5,    5,    5,
       6,    6,    6,    6,    7,    7,    7,    8,    8,    8,    9,    9,    9,   10,   10,   10,
      11,   11,   11,   12,   12,   13,   13,   14,   14,   15,   15,   15,   16,   16,   17,   17,
      18,   19,   19,   20,   20,   21,   21,   22,   23,   23,   24,   24,   25,   26,   27,   27,
      28,   29,   29,   30,   31,   32,   32,   33,   34,   35,   36,   36,   37,   38,   39,   40,
      41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54,   55,   56,
      58,   59,   60,   61,   62,   64,   65,   66,   67,   69,   70,   71,   73,   74,   76,   77,
      78,   80,   81,   83,   84,   86,   87,   89,   90,   92,   94,   95,   97,   99,  100,  102,
     104,  106,  107,  109,  111,  113,  115,  117,  118,  120,  122,  124,  126,  128,  130,  132,
     134,  137,  139,  141,  143,  145,  147,  150,  152,  154,  156,  159,  161,  163,  166,  168,
     171,  173,  175,  178,  180,  183,  186,  188,  191,  193,  196,  199,  201,  204,  207,  210,
     212,  215,  218,  221,  224,  227,  230,  233,  236,  239,  242,  245,  248,  251,  254,  257,
     260,  263,  267,  270,  273,  276,  280,  283,  286,  290,  293,  297,  300,  304,  307,  311,
     314,  318,  321,  325,  328,  332,  336,  339,  343,  347,  351,  354,  358,  362,  366,  370,
     374,  378,  381,  385,  389,  393,  397,  401,  405,  410,  414,  418,  422,  426,  430,  434,
     439,  443,  447,  451,  456,  460,  464,  469,  473,  477,  482,  486,  491,  495,  499,  504,
     508,  513,  517,  522,  527,  531,  536,  540,  545,  550,  554,  559,  563,  568,  573,  577,
     582,  587,  592,  596,  601,  606,  611,  615,  620,  625,  630,  635,  640,  644,  649,  654,
     659,  664,  669,  674,  678,  683,  688,  693,  698,  703,  708,  713,  718,  723,  728,  732,
     737,  742,  747,  752,  757,  762,  767,  772,  777,  782,  787,  792,  797,  802,  806,  811,
     816,  821,  826,  831,  836,  841,  846,  851,  855,  860,  865,  870,  875,  880,  884,  889,
     894,  899,  904,  908,  913,  918,  923,  927,  932,  937,  941,  946,  951,  955,  960,  965,
     969,  974,  978,  983,  988,  992,  997, 1001, 1005, 1010, 1014, 1019, 1023, 1027, 1032, 1036,
    1040, 1045, 1049, 1053, 1057, 1061, 1066, 1070, 1074, 1078, 1082, 1086, 1090, 1094, 1098, 1102,
    1106, 1109, 1113, 1117, 1121, 1125, 1128, 1132, 1136, 1139, 1143, 1146, 1150, 1153, 1157, 1160,
    1164, 1167, 1170, 1174, 1177, 1180, 1183, 1186, 1190, 1193, 1196, 1199, 1202, 1205, 1207, 1210,
    1213, 1216, 1219, 1221, 1224, 1227, 1229, 1232, 1234, 1237, 1239, 1241, 1244, 1246, 1248, 1251,
    1253, 1255, 1257, 1259, 1261, 1263, 1265, 1267, 1269, 1270, 1272, 1274, 1275, 1277, 1279, 1280,
    1282, 1283, 1284, 1286, 1287, 1288, 1290, 1291, 1292, 1293, 1294, 1295, 1296, 1297, 1297, 1298,
    1299, 1300, 1300, 1301, 1302, 1302, 1303, 1303, 1303, 1304, 1304, 1304, 1304, 1304, 1305, 1305,
};

DSP::DSP() {
    reset();
}

void DSP::reset() {
    regs.fill(0);
    regs[DSPReg::FLG] = 0xE0;  // Reset, mute, echo write disabled

    for (auto& v : voices) {
        v.reset();
    }

    echoOffset = 0;
    echoLength = 0;
    firBufferL.fill(0);
    firBufferR.fill(0);
    firIndex = 0;

    outputL = 0;
    outputR = 0;
    sampleGenerated = false;

    noise = 0x4000;
    counter = 0;

    konLatch = 0;
    koffLatch = 0;
    everyOtherSample = false;
}

void DSP::loadRegisters(const u8* data) {
    std::memcpy(regs.data(), data, 128);

    // Initialize echo length from EDL
    echoLength = (regs[DSPReg::EDL] & 0x0F) * 2048;
    if (echoLength == 0) echoLength = 4;  // Minimum delay

}

u8 DSP::read(u8 addr) const {
    addr &= 0x7F;
    return regs[addr];
}

void DSP::write(u8 addr, u8 value) {
    addr &= 0x7F;

    switch (addr) {
        case DSPReg::KON:
            konLatch = value;
            break;

        case DSPReg::KOFF:
            koffLatch = value;
            break;

        case DSPReg::ENDX:
            // Writing any value clears ENDX
            regs[addr] = 0;
            break;

        case DSPReg::EDL:
            echoLength = (value & 0x0F) * 2048;
            if (echoLength == 0) echoLength = 4;
            regs[addr] = value;
            break;

        default:
            regs[addr] = value;
            break;
    }
}

u8 DSP::readRAM(u16 addr) const {
    if (spc) {
        return spc->getRAM()[addr];
    }
    return 0;
}

u16 DSP::getSampleAddr(int srcn) const {
    u16 dir = regs[DSPReg::DIR] << 8;
    u16 entry = dir + (srcn * 4);
    return readRAM(entry) | (readRAM(entry + 1) << 8);
}

u16 DSP::getSampleLoopAddr(int srcn) const {
    u16 dir = regs[DSPReg::DIR] << 8;
    u16 entry = dir + (srcn * 4);
    return readRAM(entry + 2) | (readRAM(entry + 3) << 8);
}

// Counter rates for envelope/noise timing
static const unsigned counterRates[32] = {
    30721,  // Rate 0: never fires (COUNTER_RANGE + 1)
    2048, 1536, 1280, 1024, 768, 640, 512,
    384, 320, 256, 192, 160, 128, 96, 80,
    64, 48, 40, 32, 24, 20, 16, 12,
    10, 8, 6, 5, 4, 3, 2, 1
};

static const unsigned counterOffsets[32] = {
    1, 0, 1040, 536, 0, 1040, 536, 0, 1040,
    536, 0, 1040, 536, 0, 1040, 536, 0, 1040,
    536, 0, 1040, 536, 0, 1040, 536, 0, 1040,
    536, 0, 1040, 536, 0
};

void DSP::runCounter() {
    if (--counter < 0) {
        counter = COUNTER_RANGE - 1;
    }
}

bool DSP::readCounter(int rate) {
    return ((unsigned)(counter) + counterOffsets[rate]) % counterRates[rate] == 0;
}

void DSP::tick() {
    // Run global counter
    runCounter();

    // Toggle every-other-sample flag
    everyOtherSample = !everyOtherSample;

    // Process KON/KOFF latches (only every other sample)
    if (everyOtherSample) {
        for (int v = 0; v < 8; v++) {
            if (koffLatch & (1 << v)) {
                voices[v].keyOff = true;
            }
            if (konLatch & (1 << v)) {
                voices[v].keyOn = true;
            }
        }
        koffLatch = 0;
        konLatch = 0;
    }

    // Update noise generator: N=(N>>1)|(((N<<14)^(N<<13))&0x4000)
    int noiseRate = regs[DSPReg::FLG] & 0x1F;
    if (readCounter(noiseRate)) {
        int feedback = ((noise << 14) ^ (noise << 13)) & 0x4000;
        noise = (noise >> 1) | feedback;
    }

    // Mix accumulators
    s32 mainL = 0, mainR = 0;
    s32 echoL = 0, echoR = 0;

    // Process all 8 voices
    for (int v = 0; v < 8; v++) {
        processVoice(v);

        // Get voice output
        s16 sample = voices[v].output;

        // Apply per-voice volume (result is 16-bit)
        s8 volL = static_cast<s8>(regs[(v << 4) + DSPReg::VxVOLL]);
        s8 volR = static_cast<s8>(regs[(v << 4) + DSPReg::VxVOLR]);

        s16 voiceL = static_cast<s16>((sample * volL) >> 7);
        s16 voiceR = static_cast<s16>((sample * volR) >> 7);

        // Mix voices, clamping to 16 bits after each addition
        mainL += voiceL;
        mainL = std::clamp<s32>(mainL, -32768, 32767);
        mainR += voiceR;
        mainR = std::clamp<s32>(mainR, -32768, 32767);

        // Echo mixing with clamping
        if (regs[DSPReg::EON] & (1 << v)) {
            echoL += voiceL;
            echoL = std::clamp<s32>(echoL, -32768, 32767);
            echoR += voiceR;
            echoR = std::clamp<s32>(echoR, -32768, 32767);
        }
    }

    // Process echo
    processEcho(echoL, echoR);

    // Check mute flag
    bool mute = (regs[DSPReg::FLG] & 0x40) != 0;

    if (!mute) {
        // Apply main volume
        s8 mvolL = static_cast<s8>(regs[DSPReg::MVOLL]);
        s8 mvolR = static_cast<s8>(regs[DSPReg::MVOLR]);

        mainL = (mainL * mvolL) >> 7;
        mainR = (mainR * mvolR) >> 7;

        // Add echo output
        s8 evolL = static_cast<s8>(regs[DSPReg::EVOLL]);
        s8 evolR = static_cast<s8>(regs[DSPReg::EVOLR]);

        mainL += (echoL * evolL) >> 7;
        mainR += (echoR * evolR) >> 7;

        // Clamp to 16-bit
        outputL = static_cast<s16>(std::clamp(mainL, (s32)-32768, (s32)32767));
        outputR = static_cast<s16>(std::clamp(mainR, (s32)-32768, (s32)32767));
    } else {
        outputL = 0;
        outputR = 0;
    }

    sampleGenerated = true;
}

void DSP::processVoice(int v) {
    Voice& voice = voices[v];
    u8 vbase = v << 4;

    // Handle key-on
    if (voice.keyOn) {
        voice.keyOn = false;
        voice.keyOff = false;

        // Load sample start address
        u8 srcn = regs[vbase + DSPReg::VxSRCN];
        voice.brrAddr = getSampleAddr(srcn);
        voice.brrSampleIdx = 0;
        voice.brrEnd = false;
        voice.pitchCounter = 0;
        voice.prevSample1 = 0;
        voice.prevSample2 = 0;

        // Reset sample buffer to zeros
        std::fill(std::begin(voice.samples), std::end(voice.samples), 0);

        // Start envelope attack
        voice.envState = EnvelopeState::Attack;
        voice.envelope = 0;
        voice.envDelay = 0;

        // Set KON delay - 5 samples of silence before actual output
        voice.konDelay = 5;

        // Clear end flag for this voice
        regs[DSPReg::ENDX] &= ~(1 << v);

        // Read first BRR header
        voice.brrHeader = readRAM(voice.brrAddr);
        voice.brrEnd = (voice.brrHeader & 0x01) != 0;

        // Pre-decode BRR samples to fill the Gaussian interpolation buffer
        for (int i = 0; i < 4; i++) {
            voice.samples[i] = decodeBRRSample(v);
        }
    }

    // Handle key-off
    if (voice.keyOff) {
        voice.keyOff = false;
        voice.envState = EnvelopeState::Release;
    }

    // Handle KON delay - output 0 for 5 samples
    if (voice.konDelay > 0) {
        voice.konDelay--;
        voice.output = 0;
        regs[vbase + DSPReg::VxENVX] = 0;
        regs[vbase + DSPReg::VxOUTX] = 0;
        return;
    }

    // Skip if voice is silent (release state with zero envelope)
    if (voice.envState == EnvelopeState::Release && voice.envelope == 0) {
        voice.output = 0;
        regs[vbase + DSPReg::VxENVX] = 0;
        regs[vbase + DSPReg::VxOUTX] = 0;
        return;
    }

    // Run envelope
    runEnvelope(v);

    // Get pitch (14-bit)
    s32 pitch = regs[vbase + DSPReg::VxPITCHL] | ((regs[vbase + DSPReg::VxPITCHH] & 0x3F) << 8);

    // Apply pitch modulation from previous voice (signed operation)
    if ((regs[DSPReg::PMON] & (1 << v)) && v > 0) {
        // P = VxPITCH + (((OutX[x-1] >> 5) * VxPITCH) >> 10)
        pitch += ((voices[v - 1].output >> 5) * pitch) >> 10;
    }

    // Advance pitch counter and decode samples as needed
    voice.pitchCounter += pitch;

    // Cap interpolation index at 0x7FFF
    if (voice.pitchCounter > 0x7FFF) {
        voice.pitchCounter = 0x7FFF;
    }

    // When pitch counter overflows, advance to next sample
    while (voice.pitchCounter >= 0x1000) {
        voice.pitchCounter -= 0x1000;

        // Shift samples down
        voice.samples[0] = voice.samples[1];
        voice.samples[1] = voice.samples[2];
        voice.samples[2] = voice.samples[3];

        // Decode next sample
        voice.samples[3] = decodeBRRSample(v);
    }

    // Get interpolated sample
    s16 sample;
    if (regs[DSPReg::NON] & (1 << v)) {
        // Noise channel
        sample = noise;
    } else {
        // Gaussian interpolation
        sample = interpolate(v);
    }

    // Apply envelope
    sample = (sample * voice.envelope) >> 11;

    voice.output = sample;

    // Update read-only registers
    regs[vbase + DSPReg::VxENVX] = voice.envelope >> 4;
    regs[vbase + DSPReg::VxOUTX] = sample >> 8;
}

s16 DSP::decodeBRRSample(int v) {
    Voice& voice = voices[v];
    u8 vbase = v << 4;

    // Check if we need to load next block
    if (voice.brrSampleIdx >= 16) {
        voice.brrSampleIdx = 0;

        // Handle end of block
        if (voice.brrEnd) {
            u8 srcn = regs[vbase + DSPReg::VxSRCN];
            if (voice.brrHeader & 0x02) {
                // Loop
                voice.brrAddr = getSampleLoopAddr(srcn);
            } else {
                // End - release envelope
                voice.envState = EnvelopeState::Release;
                voice.envelope = 0;
                return 0;
            }
            regs[DSPReg::ENDX] |= (1 << v);
        } else {
            // Next block
            voice.brrAddr += 9;
        }

        // Read new header
        voice.brrHeader = readRAM(voice.brrAddr);
        voice.brrEnd = (voice.brrHeader & 0x01) != 0;
    }

    // Decode one sample
    int shift = voice.brrHeader >> 4;
    int filter = voice.brrHeader & 0x0C;  // 0, 4, 8, or 12

    // Get the byte containing this sample
    int byteIdx = voice.brrSampleIdx / 2;
    u8 data = readRAM(voice.brrAddr + 1 + byteIdx);

    // Get nibble and sign-extend (high nibble first)
    s32 s;
    if ((voice.brrSampleIdx & 1) == 0) {
        s = static_cast<s16>(data << 8) >> 12;  // Sign extend high nibble
    } else {
        s = static_cast<s16>(data << 12) >> 12;  // Sign extend low nibble
    }

    // Apply shift
    s = (s << shift) >> 1;
    if (shift >= 13) {
        // Invalid shift range
        s = (s >> 25) << 11;  // Same as: s < 0 ? -0x800 : 0
    }

    // Apply IIR filter (matching snes_spc exactly)
    s32 p1 = voice.prevSample1;
    s32 p2 = voice.prevSample2 >> 1;

    if (filter >= 8) {
        s += p1;
        s -= p2;
        if (filter == 8) {
            // Filter 2: s += p1 * 0.953125 - p2 * 0.46875
            s += p2 >> 4;
            s += (p1 * -3) >> 6;
        } else {
            // Filter 3: s += p1 * 0.8984375 - p2 * 0.40625
            s += (p1 * -13) >> 7;
            s += (p2 * 3) >> 4;
        }
    } else if (filter) {
        // Filter 1: s += p1 * 0.46875
        s += p1 >> 1;
        s += (-p1) >> 5;
    }

    // Clamp to 16 bits
    if (static_cast<s16>(s) != s) {
        s = (s >> 31) ^ 0x7FFF;
    }
    // Clip to 15 bits (shift left to make it 16-bit, LSB always 0)
    s = static_cast<s16>(s * 2);

    // Update previous samples
    voice.prevSample2 = voice.prevSample1;
    voice.prevSample1 = static_cast<s16>(s);

    voice.brrSampleIdx++;

    return static_cast<s16>(s);
}

s16 DSP::interpolate(int v) {
    Voice& voice = voices[v];

    // Get interpolation position (12-bit fraction)
    int offset = (voice.pitchCounter >> 4) & 0xFF;

    // Gaussian interpolation using 4 samples
    // First 3 samples wrap at 16 bits signed
    s32 out = 0;
    out += (gaussTable[255 - offset] * voice.samples[0]) >> 11;
    out += (gaussTable[511 - offset] * voice.samples[1]) >> 11;
    out += (gaussTable[256 + offset] * voice.samples[2]) >> 11;

    // Clip to 16-bit signed (wrap)
    out = static_cast<s16>(out);

    // Add 4th sample and clamp to 16-bit signed
    out += (gaussTable[offset] * voice.samples[3]) >> 11;
    out = std::clamp<s32>(out, -32768, 32767);

    return static_cast<s16>(out);
}

void DSP::runEnvelope(int v) {
    Voice& voice = voices[v];
    u8 vbase = v << 4;

    int env = voice.envelope;

    // Release mode runs unconditionally (rate=31, every sample)
    if (voice.envState == EnvelopeState::Release) {
        env -= 8;
        if (env < 0) env = 0;
        voice.envelope = env;
        return;
    }

    u8 adsr1 = regs[vbase + DSPReg::VxADSR1];
    u8 adsr2 = regs[vbase + DSPReg::VxADSR2];
    int rate;
    int newEnv = env;

    if (adsr1 & 0x80) {
        // ADSR mode
        if (voice.envState == EnvelopeState::Attack) {
            rate = (adsr1 & 0x0F) * 2 + 1;
            newEnv += (rate < 31) ? 0x20 : 0x400;
        } else {
            // Decay or Sustain - exponential decrease
            newEnv--;
            newEnv -= newEnv >> 8;
            if (voice.envState == EnvelopeState::Decay) {
                rate = ((adsr1 >> 3) & 0x0E) + 0x10;
            } else {
                rate = adsr2 & 0x1F;
            }
        }
    } else {
        // GAIN mode
        u8 gain = regs[vbase + DSPReg::VxGAIN];
        int mode = gain >> 5;

        if (mode < 4) {
            // Direct gain - immediate, no rate checking
            voice.envelope = gain * 0x10;
            return;
        } else {
            rate = gain & 0x1F;
            if (mode == 4) {
                // Linear decrease
                newEnv -= 0x20;
            } else if (mode < 6) {
                // Exponential decrease
                newEnv--;
                newEnv -= newEnv >> 8;
            } else if (mode == 6) {
                // Linear increase
                newEnv += 0x20;
            } else {
                // Bent increase (mode 7)
                newEnv += (voice.envelope < 0x600) ? 0x20 : 0x08;
            }
        }
    }

    // Only update when counter fires
    if (!readCounter(rate)) {
        return;
    }

    // Clamp envelope to 11 bits
    if (newEnv < 0) {
        newEnv = 0;
    } else if (newEnv > 0x7FF) {
        newEnv = 0x7FF;
    }

    voice.envelope = newEnv;

    // Check state transitions AFTER updating envelope
    if (adsr1 & 0x80) {
        // Check Attack->Decay transition (when envelope exceeds 0x7FF before clamp)
        if (voice.envState == EnvelopeState::Attack && env + ((rate < 31) ? 0x20 : 0x400) > 0x7FF) {
            voice.envState = EnvelopeState::Decay;
        }
        // Check Decay->Sustain transition
        else if (voice.envState == EnvelopeState::Decay && (newEnv >> 8) == (adsr2 >> 5)) {
            voice.envState = EnvelopeState::Sustain;
        }
    }
}

void DSP::processEcho(s32& outL, s32& outR) {
    if (!spc) return;

    u8* ram = spc->getRAM();
    u16 esa = regs[DSPReg::ESA] << 8;

    // Read echo samples from buffer (left-aligned, right-shift by 1 to get 15-bit value)
    u16 addr = esa + echoOffset;
    s16 echoSampleL = static_cast<s16>(ram[addr] | (ram[addr + 1] << 8)) >> 1;
    s16 echoSampleR = static_cast<s16>(ram[addr + 2] | (ram[addr + 3] << 8)) >> 1;

    // Store in FIR buffer
    firBufferL[firIndex] = echoSampleL;
    firBufferR[firIndex] = echoSampleR;

    // Apply FIR filter
    // First 7 samples wrap at 16 bits (clip), last sample is clamped
    s32 firOutL = 0, firOutR = 0;
    for (int i = 0; i < 7; i++) {
        int idx = (firIndex + i + 1) & 7;
        s8 coef = static_cast<s8>(regs[DSPReg::FIR0 + (i << 4)]);
        firOutL += (firBufferL[idx] * coef) >> 6;
        firOutR += (firBufferR[idx] * coef) >> 6;
    }

    // Clip to 16 bits (wrap)
    firOutL = static_cast<s16>(firOutL);
    firOutR = static_cast<s16>(firOutR);

    // Add last sample (newest) with clamp
    s8 coef7 = static_cast<s8>(regs[DSPReg::FIR7]);
    firOutL += (firBufferL[firIndex] * coef7) >> 6;
    firOutR += (firBufferR[firIndex] * coef7) >> 6;

    // Clamp to 16 bits
    firOutL = std::clamp<s32>(firOutL, -32768, 32767);
    firOutR = std::clamp<s32>(firOutR, -32768, 32767);

    // Mask off LSB
    firOutL &= ~1;
    firOutR &= ~1;

    firIndex = (firIndex + 1) & 7;

    // Write new echo data (if enabled)
    bool echoWriteDisabled = (regs[DSPReg::FLG] & 0x20) != 0;
    if (!echoWriteDisabled) {
        s8 efb = static_cast<s8>(regs[DSPReg::EFB]);

        s32 newEchoL = outL + ((firOutL * efb) >> 7);
        s32 newEchoR = outR + ((firOutR * efb) >> 7);

        // Clamp to 16 bits
        newEchoL = std::clamp<s32>(newEchoL, -32768, 32767);
        newEchoR = std::clamp<s32>(newEchoR, -32768, 32767);

        // Write (left-aligned, so we write the 16-bit value directly)
        ram[addr] = newEchoL & 0xFF;
        ram[addr + 1] = (newEchoL >> 8) & 0xFF;
        ram[addr + 2] = newEchoR & 0xFF;
        ram[addr + 3] = (newEchoR >> 8) & 0xFF;
    }

    // Advance echo offset
    echoOffset += 4;
    if (echoOffset >= echoLength) {
        echoOffset = 0;
    }

    // Output is FIR filter output
    outL = firOutL;
    outR = firOutR;
}

