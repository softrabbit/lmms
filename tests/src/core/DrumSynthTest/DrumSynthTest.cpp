/*
 * DrumSynthTest.cpp
 *
 * Copyright (c) 2019 the LMMS project
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

#include "QTestSuite.h"

#include "ConfigManager.h"
#include "DrumSynthOld.h"
#include "DrumSynth.h"

#include <QDir>
#include <QDebug>

class DrumSynthTest : QTestSuite
{
	Q_OBJECT
private slots:
	void DrumSynthTests()
	{
		int_sample_t * buf = NULL;
		DrumSynthOld dso;
		DrumSynth ds;
		
		QFileInfo fi(ConfigManager::inst()->factorySamplesDir() + "/drumsynth/acoustic/Snare.ds");
                QVERIFY(fi.exists());
		// 
		qDebug() << ds.GetDSFileSamples(fi.absoluteFilePath(), buf, 2, 44100);
		QVERIFY(dso.GetDSFileSamples(fi.absoluteFilePath(), buf, 2, 44100) == 
			ds.GetDSFileSamples(fi.absoluteFilePath(), buf, 2, 44100) );

	}
} DrumSynthTests;

#include "DrumSynthTest.moc"
