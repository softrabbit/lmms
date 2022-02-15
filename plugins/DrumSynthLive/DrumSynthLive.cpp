/*
 * DrumSynth.cpp - DrumSynth DS file renderer
 *
 * Copyright (c) 1998-2000 Paul Kellett (mda-vst.com)
 * Copyright (c) 2007 Paul Giblock <drfaygo/at/gmail.com>
 * Some modifications by Raine M. Ekman <raine/at/iki/dot/fi>,
 * no copyright claimed.
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "DrumSynthLive.h"

#include <cstring>
#include <iostream>
#include <sstream>

#include <cmath>

#include <QDebug>
#include <QFile>
#include <QSettings>


#ifdef LMMS_BUILD_WIN32
#define powf pow
#endif

#ifdef DOUBLE_PRECISION
// #define powf pow
#endif


#ifdef _MSC_VER
// not #if LMMS_BUILD_WIN32 because we have strncasecmp in mingw
#define strcasecmp _stricmp
#endif

using namespace std;

int DrumSynthLive::LongestEnv(void) {
  long e, eon, p;
  FLOAT l = 0.f;

  for (e = 0; e < 6; e++) // The filter is excluded here, because... it's not a
                          // sound generator?
  {

    // adjust numbering, of course it's different between
    // envelopes and on/off switches for the sections :D
    eon = e;
    if (eon > 2)
      eon--;

    p = 0;
    while (envpts[e][0][p + 1] >= 0.f)
      p++;
    envData[e].last = envpts[e][0][p] * timestretch;
    if (chkOn[eon])
      l = max(l, envData[e].last);
  }
  // Round to even buffers, leave 1 empty at end
  return BUFFER_SIZE * (1 + ceil(l / BUFFER_SIZE));

}

FLOAT DrumSynthLive::LoudestLevel(void) {
  FLOAT loudest = 0.f;
  int i = 0;

  while (i < 5) // 2
  {
    if (chkOn[i])
      loudest = max(loudest, (FLOAT)Level[i]);
    i++;
  }
  return (loudest * loudest);
}

// Update envelope when reaching new point, return whether end is reached.
// TODO: This return value should eventually
// replace the checks in the generation loop
bool DrumSynthLive::UpdateEnv(int e, long t) {
  FLOAT endEnv, dT;
  // 0.2's added
  envData[e].next =
      envpts[e][0][envData[e].pointer + 1] * timestretch; // get next point
  if (envData[e].next < 0) {
    envData[e].next = 442000 * timestretch; // if end point, hold
  }
  envData[e].value = envpts[e][1][envData[e].pointer] * 0.01f; // this level
  endEnv = envpts[e][1][envData[e].pointer + 1] * 0.01f;       // next level
  dT = envData[e].next - (FLOAT)t;
  // Ensure step is always at least 1 sample in the future. 
  dT = max(dT, (FLOAT)1.0); // Must cast literal as dT could be float or double
  envData[e].delta = (endEnv - envData[e].value) / dT;
  envData[e].pointer++;
  return t < envData[e].last;
}

void DrumSynthLive::GetEnv(int env, const QString key) {
  // We get the string split on commas, in a QStringList.
  // i.e. "0,10 20,30" becomes {"0", "10 20", "30"}
  QStringList qsl = IniData->value(key, "0,0 100,0").toStringList();
  QString str = qsl.join(",");
  qsl = str.split(" ");

  // Now we should have {"0,10", "20, 30"}
  int n;
  for (n = 0; n < qsl.size() && n < 32; ++n) {
    QStringList pair = qsl.at(n).split(",");
    envpts[env][0][n] = pair.at(0).toFloat();
    envpts[env][1][n] = pair.at(1).toFloat();
  }
  envData[env].last = envpts[env][0][n - 1];
  // Put in the sentinel
  envpts[env][0][n] = -1;
}

// Only the overtones use this, main tone is always a sine
FLOAT DrumSynthLive::waveform(FLOAT ph, int form) {
  FLOAT w;

  switch (form) {
  case 0:
    w = (FLOAT)sin(fmod(ph, TwoPi)); // sine
    break;
  case 1:
    w = (FLOAT)fabs(2.0f * (FLOAT)sin(fmod(0.5f * ph, TwoPi))) - 1.f; // sine^2
    break;
  case 2:
    while (ph < TwoPi)
      ph += TwoPi;
    w = 0.6366197f * (FLOAT)fmod(ph, TwoPi) - 1.f; // tri
    if (w > 1.f)
      w = 2.f - w;
    break;
  case 3:
    w = ph - TwoPi * (FLOAT)(int)(ph / TwoPi); // saw
    w = (0.3183098f * w) - 1.f;
    break;
  default:
    w = (sin(fmod(ph, TwoPi)) > 0.0) ? 1.f : -1.f; // square
    break;
  }

  return w;
}

// .ini file handling, good thing there is a QSettings class that handles the
// format. NB: the ini section General is represented by nothing in the keys
// when calling the reader functions, i.e. the key "xxx" under [General] is just
// plain "xxx", while it would be "othersection/xxx" under [othersection]
bool DrumSynthLive::LoadFile(QString file) {
  IniData = new QSettings(file, QSettings::IniFormat);
  return true;
}

inline int DrumSynthLive::qsString(const QString key, const QString def,
                                   char *buffer, int size) {
  QString str;
  str = IniData->value(key, def).toString();

  strncpy(buffer, str.toLocal8Bit().data(), size);
  return str.length();
}

inline int DrumSynthLive::qsInt(const QString key, int def) {
  return IniData->value(key, def).toInt();
}

inline bool DrumSynthLive::qsBool(const QString key, int def) {
  return IniData->value(key, def).toBool();
}

inline FLOAT DrumSynthLive::qsFloat(const QString key, FLOAT def) {
  return IniData->value(key, def).toFloat();
}


bool DrumSynthLive::init(sample_rate_t s) {
  char ver[32];
  // char comment[256];
  // int commentLen=0;
  // try to read version from input file
  qsString("Version", "", ver, sizeof(ver));
  ver[9] = 0;
  if (strcasecmp(ver, "DrumSynth") != 0) {
    return 0;
  } // input fail
  if (ver[11] != '1' && ver[11] != '2') {
    return 0;
  } // version fail

  ////////////////////////////
  // read master parameters

  // Comment logic not needed, left for later.
  /* qsString("Comment","",comment,sizeof(comment));
  while((comment[commentLen]!=0) && (commentLen<254)) commentLen++;
  if(commentLen==0) {
          comment[0]=32;
          comment[1]=0;
          commentLen=1;
  }
  comment[commentLen+1]=0; commentLen++;
  if((commentLen % 2)==1) commentLen++;
        */

  // The stretch parameter adds time range at the cost of precision or vice
  // versa
  Fs = s;
  timestretch = .01f * qsFloat("Stretch", 100.0);
  timestretch = min(max(timestretch, (FLOAT)0.2),
                    (FLOAT)10.0); // TODO: C++17: clamp(timestretch, 0.2f, 10.f);
  // The unit of envelope lengths is a sample in 44100Hz sample rate,
  // so adjust it to fit the current sample rate
  timestretch *= Fs / 44100.f;

  // Which parts are on?
  NoiseOn = chkOn[ENV_NOISE] = qsBool("Noise/On", 0);
  ToneOn = chkOn[ENV_TONE] = qsBool("Tone/On", 0);
  OvertonesOn = chkOn[ENV_OVERTONE1] = qsBool("Overtones/On", 0);
  Band1On = chkOn[3] = qsBool("NoiseBand/On", 0);
  Band2On = chkOn[4] = qsBool("NoiseBand2/On", 0);
  DistOn = chkOn[5] = qsBool("Distortion/On", 0);

  // Read and prepare envelopes
  GetEnv(ENV_FILTER, "FilterEnv");
  GetEnv(ENV_NOISE, "Noise/Envelope");
  GetEnv(ENV_TONE, "Tone/Envelope");
  GetEnv(ENV_OVERTONE1, "Overtones/Envelope1");
  GetEnv(ENV_OVERTONE2, "Overtones/Envelope2");
  GetEnv(ENV_NOISEBAND, "NoiseBand/Envelope");
  GetEnv(ENV_NOISEBAND2, "NoiseBand2/Envelope");

  for (int i = 0; i < 7; i++) {
    envData[i].next = 0;
    envData[i].pointer = 0;
  }

  
  // Gain, tuning, filter
  DGain = (FLOAT)powf(10.0, 0.05 * qsFloat("Level", 0)); // -20 to 20 dB
  MasterTune = qsFloat("Tuning", 0.0);
  MasterTune = (FLOAT)powf(1.0594631f, MasterTune);

  // 2 = filter all, 1 = filter only overtones
  MainFilter = qsBool("Filter", 0) ? 2 : qsBool("Overtones/Filter", 0) ? 1 : 0;
  MFres = 0.0101f * qsFloat("Resonance", 0.0); // 0 to 99
  MFres = (FLOAT)powf(MFres, 0.5f);
  HighPass = qsInt("HighPass", 0);

  // Noise parameters
  Level[ENV_NOISE] = qsInt("Noise/Level", 0);   // 1 = -90.3 dB, 181  =0.0 dB
  NoiseSlope = qsInt("Noise/Slope", 0); // -100 = Red, -50 = Pink, 0 = White, 50 = Azure, 100 = Blue
  NoiseLevel = (FLOAT)(Level[ENV_NOISE] * Level[ENV_NOISE]);
  if (NoiseSlope < 0) {
    a = 1.f + (NoiseSlope / 105.f);
    d = -NoiseSlope / 105.f;
    g = (1.f + 0.0005f * NoiseSlope * NoiseSlope) * NoiseLevel;
  } else {
    a = 1.f;
    b = -NoiseSlope / 50.f;
    c = (FLOAT)fabs((FLOAT)NoiseSlope) / 100.f;
    g = NoiseLevel;
  }


  // Tone parameters
  Level[ENV_TONE] = qsInt("Tone/Level", 128); // 1 = -90.3 dB, 181  =0.0 dB
  ToneLevel = (FLOAT)(Level[ENV_TONE] * Level[ENV_TONE]);
  F1 = MasterTune * TwoPi * qsFloat("Tone/F1", 200.0) / Fs; // Phase increment
  F1 = max(F1, (FLOAT)0.001); // to prevent overtone ratio div0	
  F2 = MasterTune * TwoPi * qsFloat("Tone/F2", 120.0) / Fs;
  TDroopRate = qsFloat("Tone/Droop", 0.f); // 0-100, linear/slow/exp/fast
  if (TDroopRate > 0.f) {
    TDroopRate = (FLOAT)powf(10.0f, (TDroopRate - 20.0f) / 30.0f);
    TDroopRate = TDroopRate * -4.f / envData[ENV_TONE].last;
    TDroop = true;
    F2 = F1 +
         ((F2 - F1) / (1.f - (FLOAT)exp(TDroopRate * envData[ENV_TONE].last)));
    ddF = F1 - F2;
  } else {
    ddF = F2 - F1;
  }
  Tphi = qsFloat("Tone/Phase", 90.f) / 57.29578f; // degrees>radians


  // Overtone parameters
  Level[ENV_OVERTONE1] = qsInt("Overtones/Level", 128); // 1 = -90.3 dB, 181  =0.0 dB
  OL = (FLOAT)(Level[ENV_OVERTONE1] * Level[ENV_OVERTONE1]);
  OMode = qsInt("Overtones/Method", 2);
  OF1 = MasterTune * TwoPi * qsFloat("Overtones/F1", 200.0) / Fs;
  OF2 = MasterTune * TwoPi * qsFloat("Overtones/F2", 120.0) / Fs;
  OW1 = qsInt("Overtones/Wave1", 0); // 0=sine, 1=sine², 2=triangle, 3=saw, 4=square
  OW2 = qsInt("Overtones/Wave2", 0);
  OBal2 = (FLOAT)qsInt("Overtones/Param", 50); // "Mix", 0-100 (all A <-> all B)
  ODrive = (FLOAT)powf(OBal2, 3.0f) / (FLOAT)powf(50.0f, 3.0f);
  OBal2 *= 0.01f;
  OBal1 = 1.f - OBal2;
  Ophi1 = Tphi;
  Ophi2 = Tphi;
  if ((qsInt("Overtones/Track1", 0) == 1) && ToneOn) {
    OF1Sync = true;
    OF1 = OF1 / F1;
  }
  if ((qsInt("Overtones/Track2", 0) == 1) && ToneOn) {
    OF2Sync = true;
    OF2 = OF2 / F1;
  }  

  
  // Filter parameters, TODO: fix this (OcQ, OcF?)
  // to be sample rate agnostic
  OcQ = powf((0.28f + OBal1 * OBal1),2); // overtone cymbal mode
  OcF = (1.8f - 0.7f * OcQ) * 0.92f; // will be multiplied by envelope
  Ocf1 = TwoPi / OF1;
  Ocf2 = TwoPi / OF2;
  for (int i = 0; i < 6; i++) // This is part of tone generation, not filter
    Oc[i][0] = Oc[i][1] = Ocf1 + (Ocf2 - Ocf1) * 0.2f * (FLOAT)i;

  // Two noise bands, and the modulation between them
  Level[3] = qsInt("NoiseBand/Level", 128); // 1 = -90.3 dB, 181  =0.0 dB
  BL[0] = (FLOAT)(Level[3] * Level[3]);
  BF[0] = MasterTune * TwoPi * qsFloat("NoiseBand/F", 1000.0) / Fs;
  BFStep = qsInt("NoiseBand/dF", 50); // Width, 0-100
  BQ[0] = (FLOAT)BFStep;
  BQ[0] = BQ[0] * BQ[0] / (10000.f - 6600.f * ((FLOAT)sqrt(BF[0]) - 0.19f));
  BFStep = 1 + (int)((40.f - (BFStep / 2.5f)) / (BQ[0] + 1.f + (1.f * BF[0])));
	
  Level[4] = qsInt("NoiseBand2/Level", 128);
  BL[1] = (FLOAT)(Level[4] * Level[4]);
  BF[1] = MasterTune * TwoPi * qsFloat("NoiseBand2/F", 1000.0) / Fs;
  BFStep2 = qsInt("NoiseBand2/dF", 50);
  BQ[1] = (FLOAT)BFStep2;
  BQ[1] = BQ[1] * BQ[1] / (10000.f - 6600.f * ((FLOAT)sqrt(BF[1]) - 0.19f));
  BFStep2 = 1 + (int)((40 - (BFStep2 / 2.5)) / (BQ[1] + 1 + (1 * BF[1])));

  // Distortion parameters, these could probably made to work with oversampling
  // through simple multiplication!

  DStep = 1 + qsInt("Distortion/Rate", 0); // 1...7 = 11/7/5/4/3/2/1 kHz, 0 = none
  if (DStep == 7)
    DStep = 20;
  if (DStep == 6)
    DStep = 10;
  if (DStep == 5)
    DStep = 8;

  clippoint = 32700;
  DAtten = 1.0f;

  if (DistOn) {
    DAtten = DGain * (short)LoudestLevel();
    clippoint = (short)min((int)DAtten, 32700);
    DAtten = (FLOAT)powf(2.0, 2.0 * qsInt("Distortion/Bits", 0)); // 0 = none, otherwise 16 - 2*Bits, e.g. 1 = 14 bit, 7 = 2 bit.
    DGain = DAtten * DGain *
            (FLOAT)powf(10.0, 0.05 * qsInt("Distortion/Clipping", 0)); // -X dB, 0 to 60
  }


  // Always a fixed random number sequence for now, remember to enable this
  // option when done coding...
  // if(qsBool("Noise/FixedSeq",0))
  srand(1);

  // I'd like to call this earlier, but as LongestEnv() changes the envelopes,
  // it might break stuff in the preceding code... :|
  Length = LongestEnv();  
  
  return true;
}

// Here we assume the file has been loaded and parsed in previously
int DrumSynthLive::GetSamples(int16_t *&wave, int channels) {

  FLOAT DF[BUFFER_SIZE];  // The buffer audio is rendered into
  FLOAT phi[BUFFER_SIZE]; // Phase buffer... something?
  long wavewords;         // Counter

  // generation
  long tpos = 0, tplus, totmp, t, j;







  // allocate the buffer
  // if(wave!=NULL) free(wave);
  wave = new int16_t[channels * Length]; // wave memory buffer
  if (wave == NULL) {
    return 0;
  }
  wavewords = 0;

  /////////////////////////////////////////////
  // Generate samples.
  tpos = 0;
  while (tpos < Length) {
    tplus = tpos + BUFFER_SIZE - 1; // Last index of buffer...

    // First up noise, if not enabled fill buffer with silence.
    if (NoiseOn) {
      for (t = tpos; t <= tplus; t++) {
        if (t < envData[ENV_NOISE].next) {
          envData[ENV_NOISE].value += envData[ENV_NOISE].delta;
        } else {
          NoiseOn = UpdateEnv(ENV_NOISE, t);
        }
	// TODO: fix filter to be sample rate agnostic, maybe see
	// https://www.firstpr.com.au/dsp/pink-noise/ for details?
	// https://www.musicdsp.org/en/latest/Filters/76-pink-noise-filter.html
	// 
        x[2] = x[1];
        x[1] = x[0];
        x[0] = (2.f * (FLOAT)rand() / RAND_MAX) - 1.f;
        TT = a * x[0] + b * x[1] + c * x[2] + d * TT;
        DF[t - tpos] = TT * g * envData[ENV_NOISE].value;
      }
    } else {
      std::fill(DF, DF + BUFFER_SIZE, 0.f);
    }

    // The main tone
    if (ToneOn) {
      TphiStart = Tphi;
      if (TDroop) {
        for (t = tpos; t <= tplus; t++) {
          phi[t - tpos] = F2 + (ddF * (FLOAT)exp(t * TDroopRate));
        }
      } else {
        for (t = tpos; t <= tplus; t++) {
          phi[t - tpos] = F1 + (t / envData[ENV_TONE].last) * ddF;
        }
      }
      for (t = tpos; t <= tplus; t++) {
        totmp = t - tpos;
        if (t < envData[ENV_TONE].next) {
          envData[ENV_TONE].value += envData[ENV_TONE].delta;
        } else {
          ToneOn = UpdateEnv(ENV_TONE, t);
        }
        Tphi = Tphi + phi[totmp];
        DF[totmp] += ToneLevel * envData[ENV_TONE].value *
                     (FLOAT)sin(fmod(Tphi, TwoPi)); // overflow?
      }
    } else {
						std::fill(phi, phi + BUFFER_SIZE, F2);
    }

    // Turning these 2 noise bands into one SIMD-friendlier loop might make
    // sense but as there is randomness involved it'll be hard to verify
    // correctness using any bit-exact methods. Anyway, it's a small victory.
    if (Band1On) // noise band 1
    {
      for (t = tpos; t <= tplus; t++) {
        if (t < envData[ENV_NOISEBAND].next) {
          envData[ENV_NOISEBAND].value =
              envData[ENV_NOISEBAND].value + envData[ENV_NOISEBAND].delta;
        } else {
          Band1On = UpdateEnv(ENV_NOISEBAND, t);
        }
        if ((t % BFStep) == 0) {
          BdF[0] = (FLOAT)rand() / RAND_MAX - 0.5f;
        }
        BPhi[0] = BPhi[0] + BF[0] + BQ[0] * BdF[0];
        botmp = t - tpos;
        DF[botmp] = DF[botmp] + (FLOAT)cos(fmod(BPhi[0], TwoPi)) *
                                    envData[ENV_NOISEBAND].value * BL[0];
      }
      // if (t >= envData[ENV_NOISEBAND].last)
	  //	  Band1On = false;
    }

    if (Band2On) // noise band 2
    {
      for (t = tpos; t <= tplus; t++) {
        if (t < envData[ENV_NOISEBAND2].next)
          envData[ENV_NOISEBAND2].value =
              envData[ENV_NOISEBAND2].value + envData[ENV_NOISEBAND2].delta;
        else
		Band2On = UpdateEnv(ENV_NOISEBAND2, t);
        if ((t % BFStep2) == 0)
          BdF[1] = (FLOAT)rand() / RAND_MAX - 0.5f;
        BPhi[1] = BPhi[1] + BF[1] + BQ[1] * BdF[1];
        botmp = t - tpos;
        DF[botmp] = DF[botmp] + (FLOAT)cos(fmod(BPhi[1], TwoPi)) *
                                    envData[ENV_NOISEBAND2].value * BL[1];
      }
      // if (t >= envData[ENV_NOISEBAND2].last)
	  // Band2On = false;
    }

    // Generate overtones and do filtering
    for (t = tpos; t <= tplus; t++) {
      if (OvertonesOn) // overtones
      {
        if (t < envData[ENV_OVERTONE1].next)
          envData[ENV_OVERTONE1].value =
              envData[ENV_OVERTONE1].value + envData[ENV_OVERTONE1].delta;
        else {
          if (t >= envData[ENV_OVERTONE1].last) // wait for OT2
          {
            envData[ENV_OVERTONE1].value = 0;
            envData[ENV_OVERTONE1].delta = 0;
            envData[ENV_OVERTONE1].next = 999999;
          } else
            UpdateEnv(ENV_OVERTONE1, t);
        }
        //
        if (t < envData[ENV_OVERTONE2].next)
          envData[ENV_OVERTONE2].value =
              envData[ENV_OVERTONE2].value + envData[ENV_OVERTONE2].delta;
        else {
          if (t >= envData[ENV_OVERTONE2].last) // wait for OT1
          {
            envData[ENV_OVERTONE2].value = 0;
            envData[ENV_OVERTONE2].delta = 0;
            envData[ENV_OVERTONE2].next = 999999;
          } else
            UpdateEnv(ENV_OVERTONE2, t);
        }
        //
        TphiStart = TphiStart + phi[t - tpos];
        if (OF1Sync)
          Ophi1 = TphiStart * OF1;
        else
          Ophi1 = Ophi1 + OF1;
        if (OF2Sync)
          Ophi2 = TphiStart * OF2;
        else
          Ophi2 = Ophi2 + OF2;
        Ot = 0.0f;
        switch (OMode) {
        case 0: // add
          Ot = OBal1 * envData[ENV_OVERTONE1].value * waveform(Ophi1, OW1);
          Ot = OL * (Ot + OBal2 * envData[ENV_OVERTONE2].value *
                              waveform(Ophi2, OW2));
          break;

        case 1: // FM
          Ot = ODrive * envData[ENV_OVERTONE2].value * waveform(Ophi2, OW2);
          Ot = OL * envData[ENV_OVERTONE1].value * waveform(Ophi1 + Ot, OW1);
          break;

        case 2: // RM
          Ot = (1 - ODrive / 8) +
               (((ODrive / 8) * envData[ENV_OVERTONE2].value) *
                waveform(Ophi2, OW2));
          Ot = OL * envData[ENV_OVERTONE1].value * waveform(Ophi1, OW1) * Ot;
          break;

        case 3: // 808 Cymbal
          for (j = 0; j < 6; j++) {
            Oc[j][0] += 1.0f;

            if (Oc[j][0] > Oc[j][1]) {
              Oc[j][0] -= Oc[j][1];
              Ot = OL * envData[ENV_OVERTONE1].value;
            }
          }
	  // TODO: fix filter to be sample rate agnostic
          Ocf1 = envData[ENV_OVERTONE2].value * OcF; // filter freq, should this be * timestretch?
          Oc0 += Ocf1 * Oc1;
          Oc1 += Ocf1 * (Ot + Oc2 - OcQ * Oc1 - Oc0); // bpf
          Oc2 = Ot;
          Ot = Oc1;
          break;
        }
      }

      if (MainFilter > 0) {
        if (t < envData[ENV_FILTER].next)
          envData[ENV_FILTER].value =
              envData[ENV_FILTER].value + envData[ENV_FILTER].delta;
        else
          UpdateEnv(ENV_FILTER, t);

	// TODO: fix filter to be sample rate agnostic - is this filter what is described in
	// http://www.martin-finke.de/blog/articles/audio-plugins-013-filter/
	// and https://www.musicdsp.org/en/latest/Filters/29-resonant-filter.html ?
        MFtmp = envData[ENV_FILTER].value;
        if (MFtmp > 0.2f)
          MFfb = 1.001f - (FLOAT)powf(10.0f, MFtmp - 1);
        else
          MFfb = 0.999f - 0.7824f * MFtmp;

        FLOAT filter_in =
            DF[t - tpos] * (MainFilter == 2) + Ot * (MainFilter > 0);
        FLOAT HP = filter_in * HighPass;

        MFtmp = filter_in + MFres * (1.f + (1.f / MFfb)) * (MFin - MFout);
        MFin = MFfb * (MFin - MFtmp) + MFtmp;
        MFout = MFfb * (MFout - MFin) + MFin;

        DF[t - tpos] =
            (MFout - HP) +                   // Filter to output
            DF[t - tpos] * (MainFilter < 2); // Main to output if needed
      } else {
        DF[t - tpos] = DF[t - tpos] + Ot; // no filter
      }
    }

    if (DistOn) // bit resolution
    {
      for (j = 0; j < BUFFER_SIZE; j++) {
	DF[j] = DGain * (int)(DF[j] / DAtten);
      }

      for (j = 0; j < BUFFER_SIZE; j += DStep) // downsampling
      {
        DownAve = 0;
        DownStart = j;
        DownEnd = j + DStep - 1;
        for (jj = DownStart; jj <= DownEnd; jj++)
          DownAve = DownAve + DF[jj];
        DownAve = DownAve / DStep;
        for (jj = DownStart; jj <= DownEnd; jj++)
          DF[jj] = DownAve;
      }
    } else {
      for (j = 0; j < BUFFER_SIZE; j++) {
	DF[j] *= DGain;
      }
    }
    for (j = 0; j < BUFFER_SIZE; j++) // clipping + output
    {
      if (DF[j] > clippoint) {
        wave[wavewords++] = clippoint;
      } else if (DF[j] < -clippoint) {
	wave[wavewords++] = -clippoint;
      } else {
	wave[wavewords++] = (short)DF[j];
      }
			
      for (int c = 1; c < channels; c++) {
        wave[wavewords] = wave[wavewords - 1];
        wavewords++;
      }
    }

    tpos = tpos + BUFFER_SIZE;
  }

  return Length;
}
