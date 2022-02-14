/*
 * DrumSynth.h - DrumSynth DS file renderer
 *
 * Copyright (c) 1998-2000 Paul Kellett (mda-vst.com)
 * Copyright (c) 2007 Paul Giblock <drfaygo/at/gmail.com>
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

#ifndef _DRUMSYNTHLIVE_H__
#define _DRUMSYNTHLIVE_H__

#include "lmms_basics.h"
#include <QFile>
#include <QSettings>
#include <sstream>
#include <stdint.h>

class QString;
using namespace std;

#ifdef DOUBLE_PRECISION
typedef double FLOAT;
#else
typedef float FLOAT;
#endif

class DrumSynthLive {
public:
  DrumSynthLive(){};
  int GetSamples(int16_t *&wave, int channels, sample_rate_t Fs);
  bool LoadFile(QString file);

private:
  const FLOAT TwoPi = 6.2831853f;

  FLOAT envpts[8][2][32] = {0}; // envelope/time-level/point

  struct envstatus {
    FLOAT last;  // Time of last envelope point
    FLOAT value; // Envelope value
    FLOAT delta; // Delta to add to envelope value at each sample
    FLOAT next;  // Timestamp of next point to go to
    int pointer; // Index of current point
  };
  // Indexes for envelopes and section on/off switches
  const int ENV_TONE = 0;
  const int ENV_NOISE = 1;
  const int ENV_OVERTONE1 = 2;
  const int ENV_OVERTONE2 = 3;
  const int ENV_NOISEBAND = 4;
  const int ENV_NOISEBAND2 = 5;
  const int ENV_FILTER = 6;

  // Identical results not promised if this is changed - but why does this change
  // even things in position 0, which it shouldn't really do by all logic
  const int BUFFER_SIZE = 1200; 

  FLOAT timestretch; // overall time scaling

  struct envstatus envData[8]; // envelope running status
  bool chkOn[8];               // section on/off
  int Level[8];                // and level

  FLOAT LoudestLevel(void);
  int LongestEnv(void);
  bool UpdateEnv(int e, long t);
  void GetEnv(int env, const QString key);

  FLOAT waveform(FLOAT ph, int form);

  int qsString(const QString key, const QString def, char *buffer, int size);
  int qsInt(const QString key, int def);
  bool qsBool(const QString key, int def);
  FLOAT qsFloat(const QString key, FLOAT def);

  QSettings *IniData;
};

#endif
