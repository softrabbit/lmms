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

#include <stdint.h>
#include <sstream>
#include "lmms_basics.h"
#include <QFile>
#include <QSettings>

class QString;
using namespace std;

class DrumSynthLive {
    public:
        DrumSynthLive() {};
        int GetSamples(int16_t *&wave, int channels, sample_rate_t Fs);
	bool LoadFile(QString file);
    private:
	const float   TwoPi =  6.2831853f;
	
	struct envstatus {
		float last;          // Time of last envelope point
		float value;         // Envelope value
		float pointer;       // Current point (index), should be made int
		float delta;         // Delta to add to envelope value at each sample
		float next;          // Timestamp of next point to go to
	};
        // Envelope indexes
	const int ENV_TONE = 1;
	const int ENV_NOISE = 2;
	const int ENV_OVERTONE1 = 3;
	const int ENV_OVERTONE2 = 4;
	const int ENV_NOISEBAND = 5;
	const int ENV_NOISEBAND2 = 6;
	const int ENV_FILTER = 7;

	const int BUFFER_SIZE = 1200;  // Identical results not promised if this is changed

	float timestretch;         // overall time scaling

	struct envstatus envData[8];       // envelope running status	
	bool  chkOn[8];            // section on/off 
	int   Level[8];            // and level
	
        float LoudestEnv(void);
        int   LongestEnv(void);
        void  UpdateEnv(int e, long t);
        void  GetEnv(int env, const QString key);

        float waveform(float ph, int form);

        int qsString(const QString key, const QString def, char *buffer, int size);
        int qsInt(const QString key, int def);
	bool qsBool(const QString key, int def);
        float qsFloat(const QString key, float def);

	QSettings *IniData;
};

#endif
