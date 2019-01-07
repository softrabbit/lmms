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


#ifndef _DRUMSYNTH_H__
#define _DRUMSYNTH_H__

#include <stdint.h>
#include <sstream>
#include <QFile>
#include "lmms_basics.h"
using namespace std;

class QString;

class DrumSynth {
    public:
        DrumSynth() {};
        int GetDSFileSamples(QString dsfile, int16_t *&wave, int channels, sample_rate_t Fs);

    private:
        float LoudestEnv(void);
        int   LongestEnv(void);
        void  UpdateEnv(int e, long t);
        void  GetEnv(int env, const char *sec, const char *key, QString ini);

        float waveform(float ph, int form);

	QByteArray LoadFile(QString file);
        int GetPrivateProfileString(const char *sec, const char *key, const char *def, char *buffer, int size, QString file);
        int GetPrivateProfileInt(const char *sec, const char *key, int def, QString file);
        float GetPrivateProfileFloat(const char *sec, const char *key, float def, QString file);

	float envpts[8][3][32];    //envelope/time-level/point
	float envData[8][6];       //envelope running status
	int   chkOn[8], sliLev[8]; //section on/off and level
	float timestretch;         //overall time scaling
	short DD[1200], clippoint;
	float DF[1200];
	float phi[1200];

	long  wavewords, wavemode=0;
	float mem_t=1.0f, mem_o=1.0f, mem_n=1.0f, mem_b=1.0f, mem_tune=1.0f, mem_time=1.0f;

	QByteArray dat;
	stringstream is;
};

#endif
