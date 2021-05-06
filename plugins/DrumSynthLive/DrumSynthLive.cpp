/*
 * DrumSynth.cpp - DrumSynth DS file renderer
 *
 * Copyright (c) 1998-2000 Paul Kellett (mda-vst.com)
 * Copyright (c) 2007 Paul Giblock <drfaygo/at/gmail.com>
 * Some modifications by Raine M. Ekman <raine/at/iki/dot/fi>
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

#include <sstream>
#include <iostream>
#include <cstring>

#include <cmath>

#include <QFile>
#include <QDebug>
#include <QSettings>

#ifdef LMMS_BUILD_WIN32
#define powf pow
#endif

#ifdef _MSC_VER
//not #if LMMS_BUILD_WIN32 because we have strncasecmp in mingw
#define strcasecmp _stricmp
#endif


using namespace std;

#define WORD  __u16
#define DWORD __u32

float _envpts[8][3][32];    // envelope/time-level/point, this didn't agree with being moved into the .h file (?)

int DrumSynthLive::LongestEnv(void)
{
  long e, eon, p;
  float l=0.f;

  for(e=1; e<7; e++) // The filter is excluded here, because... it's not a sound generator?
  {

    eon = e - 1; 
    if(eon>2) eon=eon-1; // WTF?

    p = 0;
    while (_envpts[e][0][p + 1] >= 0.f) p++;
    envData[e].last = _envpts[e][0][p] * timestretch;
    if(chkOn[eon]) l = max(l, envData[e].last);
  }
  //l *= timestretch;


  return BUFFER_SIZE * (2 + (int)(l / BUFFER_SIZE));

}


float DrumSynthLive::LoudestEnv(void)
{
  float loudest=0.f;
  int i=0;

  while (i<5) //2
  {
	  if(chkOn[i]) loudest = max(loudest, (float)Level[i]);
	  i++;
  }
  return (loudest * loudest);
}


void DrumSynthLive::UpdateEnv(int e, long t)
{
  float endEnv, dT;
                                                             //0.2's added
  envData[e].next = _envpts[e][0][(long)(envData[e].pointer + 1.f)] * timestretch; //get next point
  if(envData[e].next < 0) {
	  envData[e].next = 442000 * timestretch; //if end point, hold
  }
  envData[e].value = _envpts[e][1][(long)(envData[e].pointer + 0.f)] * 0.01f; //this level
  endEnv = _envpts[e][1][(long)(envData[e].pointer + 1.f)] * 0.01f;          //next level
  dT = envData[e].next - (float)t;
  dT = max(dT, 1.0f);
  envData[e].delta = (endEnv - envData[e].value) / dT;
  envData[e].pointer = envData[e].pointer + 1.0f;
}


void DrumSynthLive::GetEnv(int env, const QString sec, const QString key, QString ini)
{
	// We get the string split on commas, in a QStringList.
	// i.e. "0,10 20,30" becomes {"0", "10 20", "30"}
	QStringList qsl;
	if(sec=="General") {
		qsl = IniData->value(key, "0,0 100,0").toStringList();
  } else {
    qsl = IniData->value(sec+"/"+key, "0,0 100,0").toStringList();
	}
	QString str = qsl.join(",");
	qsl = str.split(" ");

	// Now we should have {"0,10", "20, 30"}
	int n;
	for(n=0; n<qsl.size() && n<32; ++n) {
		QStringList pair = qsl.at(n).split(",");
		_envpts[env][0][n] = pair.at(0).toFloat();
		_envpts[env][1][n] = pair.at(1).toFloat();
	}
	envData[env].last = _envpts[env][0][n-1];
	// Fill the rest with negative values
	for( ; n<32; ++n) {
		_envpts[env][0][n] = -1.0;
		_envpts[env][1][n] = -1.0;
	}
}


float DrumSynthLive::waveform(float ph, int form)
{
  float w;

  switch (form)
  {
  case 0: w = (float)sin(fmod(ph,TwoPi));                            //sine
	  break; 
  case 1: w = (float)fabs(2.0f*(float)sin(fmod(0.5f*ph,TwoPi)))-1.f; //sine^2
	  break; 
  case 2: while(ph<TwoPi) ph+=TwoPi;
	  w = 0.6366197f * (float)fmod(ph,TwoPi) - 1.f;              //tri
	  if(w>1.f) w=2.f-w;
	  break;
  case 3: w = ph - TwoPi * (float)(int)(ph / TwoPi);                 //saw
	  w = (0.3183098f * w) - 1.f;
	  break;
  default: w = (sin(fmod(ph,TwoPi))>0.0)? 1.f: -1.f;                 //square
	  break; 
  }

  return w;
}


QByteArray DrumSynthLive::LoadFile(QString file)
{
    // Use QFile to handle unicode file name on Windows
    QFile f(file);
    f.open(QIODevice::ReadOnly);
    return f.readAll().constData();
}

bool DrumSynthLive::Parse(QString file)
{
	IniData = new QSettings(file, QSettings::IniFormat);

	/*QStringList keys = IniData->allKeys();
	for (int i = 0; i < keys.size(); ++i)
	qDebug() << keys.at(i) << " = " << IniData->value(keys.at(i)); */
	return true;
}



int DrumSynthLive::GetPrivateProfileString(const QString sec, const QString key, const QString def, char *buffer, int size, QString file) {
  QString str;
	if(sec=="General") {
		str = IniData->value(key,def).toString();
  } else {
    str = IniData->value(sec+"/"+key,def).toString();
	}
	//qDebug() << str;
	strncpy(buffer, str.toLocal8Bit().data(), size);
	return str.length();
}
int DrumSynthLive::GetPrivateProfileInt(const QString sec, const QString key, int def, QString file)
{
  // "General" sections are special...
	if(sec=="General") {
		return IniData->value(key,def).toInt();
  }
  return IniData->value(sec+"/"+key,def).toInt();
}

bool DrumSynthLive::GetPrivateProfileBool(const QString sec, const QString key, int def, QString file)
{
				return GetPrivateProfileInt(sec, key, def, file) != 0;
}

float DrumSynthLive::GetPrivateProfileFloat(const QString sec, const QString key, float def, QString file)
{
	if(sec=="General") {
		return IniData->value(key,def).toFloat();
  }
  return IniData->value(sec+"/"+key,def).toFloat();

}


//  Constantly opening and scanning each file for the setting really sucks.
//  But, the original did this, and we know it works.  Will store file into
//  an associative array or something once we have a datastructure to load in to.
//  llama

int DrumSynthLive::GetDSFileSamples(QString dsfile, int16_t *&wave, int channels, sample_rate_t Fs)
{
	Parse(dsfile);
	
	float DF[BUFFER_SIZE];            // Buffer audio is rendered into
	float phi[BUFFER_SIZE];           // Phase buffer... something?
  long  wavewords;                  // Counter

  short clippoint;

  //input file
  char ver[32];
  //char comment[256];
  //int commentLen=0;

  //generation
  long  Length, tpos=0, tplus, totmp, t, i, j;
  float x[3] = {0.f, 0.f, 0.f};
  float MasterTune, randmax, randmax2;
  int   MainFilter, HighPass;

  bool  NoiseOn, ToneOn, DistOn;
  long  NoiseSlope, TDroop=0, DStep;
  float a, b=0.f, c=0.f, d=0.f, g, TT=0.f, ToneLevel, NoiseLevel, F1, F2;
  float TphiStart=0.f, Tphi, TDroopRate, ddF, DAtten, DGain;

  bool  Band1On, Band2On;
  long  BFStep, BFStep2, botmp;
  float BdF=0.f, BdF2=0.f, BPhi, BPhi2, BF, BF2, BQ, BQ2, BL, BL2;

  bool  OvertonesOn;
  long  OF1Sync=0, OF2Sync=0, OMode, OW1, OW2;
  float Ophi1, Ophi2, OF1, OF2, OL, Ot=0 /*PG: init */, OBal1, OBal2, ODrive;
  float Ocf1, Ocf2, OcF, OcQ, OcA, Oc[6][2];  //overtone cymbal mode
  float Oc0=0.0f, Oc1=0.0f, Oc2=0.0f;

  float MFfb, MFtmp, MFres, MFin=0.f, MFout=0.f;
  float DownAve;
  long  DownStart, DownEnd, jj;

  //try to read version from input file
  GetPrivateProfileString("General","Version","",ver,sizeof(ver),dsfile);
  ver[9]=0;
  if(strcasecmp(ver, "DrumSynth") != 0) {return 0;} //input fail
  if(ver[11] != '1' && ver[11] != '2') {return 0;} //version fail


  //read master parameters
	// Comment logic not needed, left for later.
  /* GetPrivateProfileString("General","Comment","",comment,sizeof(comment),dsfile);
  while((comment[commentLen]!=0) && (commentLen<254)) commentLen++;
  if(commentLen==0) {
	  comment[0]=32;
	  comment[1]=0;
	  commentLen=1;
  }
  comment[commentLen+1]=0; commentLen++;
  if((commentLen % 2)==1) commentLen++;
	*/

	timestretch = .01f * GetPrivateProfileFloat("General","Stretch",100.0,dsfile);
	timestretch = min(max(timestretch, 0.2f), 10.f); //C++17: clamp(timestretch, 0.2f, 10.f);
  // the unit of envelope lengths is a sample in 44100Hz sample rate, so correct it
  timestretch *= Fs / 44100.f;

  DGain = (float)powf(10.0, 0.05 * GetPrivateProfileFloat("General","Level",0,dsfile));

  MasterTune = GetPrivateProfileFloat("General","Tuning",0.0,dsfile);
  MasterTune = (float)powf(1.0594631f, MasterTune);
  MainFilter = 2 * GetPrivateProfileInt("General","Filter",0,dsfile);
  MFres = 0.0101f * GetPrivateProfileFloat("General","Resonance",0.0,dsfile);
  MFres = (float)powf(MFres, 0.5f);

  HighPass = GetPrivateProfileInt("General","HighPass",0,dsfile);
  GetEnv(ENV_FILTER, "General", "FilterEnv", dsfile);


  //read noise parameters
  NoiseOn = chkOn[1] = GetPrivateProfileBool("Noise","On",0,dsfile);
  Level[1] = GetPrivateProfileInt("Noise","Level",0,dsfile);
  NoiseSlope =  GetPrivateProfileInt("Noise","Slope",0,dsfile);
  GetEnv(ENV_NOISE, "Noise", "Envelope", dsfile);
  NoiseLevel = (float)(Level[1] * Level[1]);
  if(NoiseSlope<0)
  {
	  a = 1.f + (NoiseSlope / 105.f);
	  d = -NoiseSlope / 105.f;
	  g = (1.f + 0.0005f * NoiseSlope * NoiseSlope) * NoiseLevel;
  } else {
	  a = 1.f;
	  b = -NoiseSlope / 50.f;
	  c = (float)fabs((float)NoiseSlope) / 100.f;
	  g = NoiseLevel;
  }

  // Always a fixed random number sequence for now, remember to enable this option
  // when done coding...
  //if(GetPrivateProfileBool("Noise","FixedSeq",0,dsfile))
  srand(1); 

  //read tone parameters
  ToneOn = chkOn[0] = GetPrivateProfileBool("Tone","On",0,dsfile);
  Level[0] = GetPrivateProfileInt("Tone","Level",128,dsfile);
  ToneLevel = (float)(Level[0] * Level[0]);
  GetEnv(ENV_TONE, "Tone", "Envelope", dsfile);
  F1 = MasterTune * TwoPi * GetPrivateProfileFloat("Tone","F1",200.0,dsfile) / Fs;
  F1 = max(F1,0.001f); //to prevent overtone ratio div0
  F2 = MasterTune * TwoPi * GetPrivateProfileFloat("Tone","F2",120.0,dsfile) / Fs;
  TDroopRate = GetPrivateProfileFloat("Tone","Droop",0.f,dsfile);
  if(TDroopRate>0.f)
  {
    TDroopRate = (float)powf(10.0f, (TDroopRate - 20.0f) / 30.0f);
    TDroopRate = TDroopRate * -4.f / envData[ENV_TONE].last;
    TDroop = 1;
    F2 = F1+((F2-F1)/(1.f-(float)exp(TDroopRate * envData[ENV_TONE].last)));
    ddF = F1 - F2;
  }
  else ddF = F2-F1;

  Tphi = GetPrivateProfileFloat("Tone","Phase",90.f,dsfile) / 57.29578f; //degrees>radians

  //read overtone parameters
  OvertonesOn = chkOn[2] = GetPrivateProfileBool("Overtones","On",0,dsfile); 
  Level[2] = GetPrivateProfileInt("Overtones","Level",128,dsfile);
  OL = (float)(Level[2] * Level[2]);
  GetEnv(ENV_OVERTONE1, "Overtones", "Envelope1", dsfile);
  GetEnv(ENV_OVERTONE2, "Overtones", "Envelope2", dsfile);
  OMode = GetPrivateProfileInt("Overtones","Method",2,dsfile);
  OF1 = MasterTune * TwoPi * GetPrivateProfileFloat("Overtones","F1",200.0,dsfile) / Fs;
  OF2 = MasterTune * TwoPi * GetPrivateProfileFloat("Overtones","F2",120.0,dsfile) / Fs;
  OW1 = GetPrivateProfileInt("Overtones","Wave1",0,dsfile);
  OW2 = GetPrivateProfileInt("Overtones","Wave2",0,dsfile);
  OBal2 = (float)GetPrivateProfileInt("Overtones","Param",50,dsfile);
  ODrive = (float)powf(OBal2, 3.0f) / (float)powf(50.0f, 3.0f);
  OBal2 *= 0.01f;
  OBal1 = 1.f - OBal2;
  Ophi1 = Tphi;
  Ophi2 = Tphi;
  if(MainFilter==0)
    MainFilter = GetPrivateProfileInt("Overtones","Filter",0,dsfile);
  if((GetPrivateProfileInt("Overtones","Track1",0,dsfile)==1) && ToneOn)
  { OF1Sync = 1;  OF1 = OF1 / F1; }
  if((GetPrivateProfileInt("Overtones","Track2",0,dsfile)==1) && ToneOn)
  { OF2Sync = 1;  OF2 = OF2 / F1; }

  OcA = 0.28f + OBal1 * OBal1;  //overtone cymbal mode
  OcQ = OcA * OcA;
  OcF = (1.8f - 0.7f * OcQ) * 0.92f; //multiply by env 2
  OcA *= 1.0f + 4.0f * OBal1;  //level is a compromise!
  Ocf1 = TwoPi / OF1;
  Ocf2 = TwoPi / OF2;
  for(i=0; i<6; i++) Oc[i][0] = Oc[i][1] = Ocf1 + (Ocf2 - Ocf1) * 0.2f * (float)i;

  //read noise band parameters
  Band1On =  chkOn[3] = GetPrivateProfileBool("NoiseBand","On",0,dsfile); 
  Level[3] = GetPrivateProfileInt("NoiseBand","Level",128,dsfile);
  BL = (float)(Level[3] * Level[3]);
  BF = MasterTune * TwoPi * GetPrivateProfileFloat("NoiseBand","F",1000.0,dsfile) / Fs;
  BPhi = TwoPi / 8.f;
  GetEnv(ENV_NOISEBAND, "NoiseBand", "Envelope", dsfile);
  BFStep = GetPrivateProfileInt("NoiseBand","dF",50,dsfile);
  BQ = (float)BFStep;
  BQ = BQ * BQ / (10000.f-6600.f*((float)sqrt(BF)-0.19f));
  BFStep = 1 + (int)((40.f - (BFStep / 2.5f)) / (BQ + 1.f + (1.f * BF)));

  Band2On = chkOn[4] = GetPrivateProfileBool("NoiseBand2","On",0,dsfile); 
  Level[4] = GetPrivateProfileInt("NoiseBand2","Level",128,dsfile);
  BL2 = (float)(Level[4] * Level[4]);
  BF2 = MasterTune * TwoPi * GetPrivateProfileFloat("NoiseBand2","F",1000.0,dsfile) / Fs;
  BPhi2 = TwoPi / 8.f;
  GetEnv(ENV_NOISEBAND2, "NoiseBand2", "Envelope", dsfile);
  BFStep2 = GetPrivateProfileInt("NoiseBand2","dF",50,dsfile);
  BQ2 = (float)BFStep2;
  BQ2 = BQ2 * BQ2 / (10000.f-6600.f*((float)sqrt(BF2)-0.19f));
  BFStep2 = 1 + (int)((40 - (BFStep2 / 2.5)) / (BQ2 + 1 + (1 * BF2)));

  //read distortion parameters
  DistOn = chkOn[5] = GetPrivateProfileBool("Distortion","On",0,dsfile); 
  DStep = 1 + GetPrivateProfileInt("Distortion","Rate",0,dsfile);
  if(DStep==7) DStep=20;
  if(DStep==6) DStep=10;
  if(DStep==5) DStep=8;

  clippoint = 32700;
  DAtten = 1.0f;

  if(DistOn)
  {
    DAtten = DGain * (short)LoudestEnv();
    clippoint = (short)min((int)DAtten, 32700);
    DAtten = (float)powf(2.0, 2.0 * GetPrivateProfileInt("Distortion","Bits",0,dsfile));
    DGain = DAtten * DGain * (float)powf(10.0, 0.05 * GetPrivateProfileInt("Distortion","Clipping",0,dsfile));
  }

  randmax = 1.f / RAND_MAX; randmax2 = 2.f * randmax;

  //prepare envelopes
  for (i=1;i<8;i++) {
	  envData[i].next=0;
	  envData[i].pointer=0;
  }

  Length = LongestEnv();

  //allocate the buffer
  //if(wave!=NULL) free(wave);
  wave = new int16_t[channels * Length]; //wave memory buffer
  if(wave==NULL) {return 0;}
  wavewords = 0;

  /////////////////////////////////////////////
  //generate
  tpos = 0;
  while(tpos<Length)
  {
    tplus = tpos + BUFFER_SIZE -1; // Last index of buffer...

    if(NoiseOn) //noise
    {
      for(t=tpos; t<=tplus; t++)
      {
        if(t < envData[ENV_NOISE].next) envData[ENV_NOISE].value = envData[ENV_NOISE].value + envData[ENV_NOISE].delta;
        else UpdateEnv(ENV_NOISE, t);
        x[2] = x[1];
        x[1] = x[0];
        x[0] = (randmax2 * (float)rand()) - 1.f;
        TT = a * x[0] + b * x[1] + c * x[2] + d * TT;
        DF[t - tpos] = TT * g * envData[ENV_NOISE].value;
      }
      if(t>=envData[ENV_NOISE].last) NoiseOn=false;
    }
    else {
        for(j=0; j<BUFFER_SIZE; j++) DF[j]=0.f;
    }

    if(ToneOn)
    {
      TphiStart = Tphi;
      if(TDroop==1)
      {
        for(t=tpos; t<=tplus; t++)
          phi[t - tpos] = F2 + (ddF * (float)exp(t * TDroopRate));
      }
      else
      {
        for(t=tpos; t<=tplus; t++)
          phi[t - tpos] = F1 + (t / envData[ENV_TONE].last) * ddF;
      }
      for(t=tpos; t<=tplus; t++)
      {
        totmp = t - tpos;
        if(t < envData[ENV_TONE].next)
          envData[ENV_TONE].value = envData[ENV_TONE].value + envData[ENV_TONE].delta;
        else UpdateEnv(ENV_TONE, t);
        Tphi = Tphi + phi[totmp];
        DF[totmp] += ToneLevel * envData[ENV_TONE].value * (float)sin(fmod(Tphi,TwoPi));//overflow?
      }
      if(t>=envData[ENV_TONE].last) ToneOn=false;
    }
    else for(j=0; j<BUFFER_SIZE; j++) phi[j]=F2; //for overtone sync

    if(Band1On) //noise band 1
    {
      for(t=tpos; t<=tplus; t++)
      {
        if(t < envData[ENV_NOISEBAND].next)
          envData[ENV_NOISEBAND].value = envData[ENV_NOISEBAND].value + envData[ENV_NOISEBAND].delta;
        else UpdateEnv(ENV_NOISEBAND, t);
        if((t % BFStep) == 0) BdF = randmax * (float)rand() - 0.5f;
        BPhi = BPhi + BF + BQ * BdF;
        botmp = t - tpos;
        DF[botmp] = DF[botmp] + (float)cos(fmod(BPhi,TwoPi)) * envData[ENV_NOISEBAND].value * BL;
      }
      if(t>=envData[ENV_NOISEBAND].last) Band1On=false;
    }

    if(Band2On) //noise band 2
    {
      for(t=tpos; t<=tplus; t++)
      {
        if(t < envData[ENV_NOISEBAND2].next)
          envData[ENV_NOISEBAND2].value = envData[ENV_NOISEBAND2].value + envData[ENV_NOISEBAND2].delta;
        else UpdateEnv(ENV_NOISEBAND2, t);
        if((t % BFStep2) == 0) BdF2 = randmax * (float)rand() - 0.5f;
        BPhi2 = BPhi2 + BF2 + BQ2 * BdF2;
        botmp = t - tpos;
        DF[botmp] = DF[botmp] + (float)cos(fmod(BPhi2,TwoPi)) * envData[ENV_NOISEBAND2].value * BL2;
      }
      if(t>=envData[ENV_NOISEBAND2].last) Band2On=false;
    }


    for (t=tpos; t<=tplus; t++)
    {
      if(OvertonesOn) //overtones
      {
        if(t<envData[ENV_OVERTONE1].next)
          envData[ENV_OVERTONE1].value = envData[ENV_OVERTONE1].value + envData[ENV_OVERTONE1].delta;
        else
        {
          if(t>=envData[ENV_OVERTONE1].last) //wait for OT2
          {
            envData[ENV_OVERTONE1].value = 0;
            envData[ENV_OVERTONE1].delta = 0;
            envData[ENV_OVERTONE1].next = 999999;
          }
          else UpdateEnv(ENV_OVERTONE1, t);
        }
        //
        if(t<envData[ENV_OVERTONE2].next)
          envData[ENV_OVERTONE2].value = envData[ENV_OVERTONE2].value + envData[ENV_OVERTONE2].delta;
        else
        {
          if(t>=envData[ENV_OVERTONE2].last) //wait for OT1
          {
            envData[ENV_OVERTONE2].value = 0;
            envData[ENV_OVERTONE2].delta = 0;
            envData[ENV_OVERTONE2].next = 999999;
          }
          else UpdateEnv(ENV_OVERTONE2, t);
        }
        //
        TphiStart = TphiStart + phi[t - tpos];
        if(OF1Sync==1) Ophi1 = TphiStart * OF1; else Ophi1 = Ophi1 + OF1;
        if(OF2Sync==1) Ophi2 = TphiStart * OF2; else Ophi2 = Ophi2 + OF2;
        Ot=0.0f;
        switch (OMode)
        {
          case 0: //add
            Ot = OBal1 * envData[ENV_OVERTONE1].value * waveform(Ophi1, OW1);
            Ot = OL * (Ot + OBal2 * envData[ENV_OVERTONE2].value * waveform(Ophi2, OW2));
            break;

          case 1: //FM
            Ot = ODrive * envData[ENV_OVERTONE2].value * waveform(Ophi2, OW2);
            Ot = OL * envData[ENV_OVERTONE1].value * waveform(Ophi1 + Ot, OW1);
            break;

          case 2: //RM
            Ot = (1 - ODrive / 8) + (((ODrive / 8) * envData[ENV_OVERTONE2].value) * waveform(Ophi2, OW2));
            Ot = OL * envData[ENV_OVERTONE1].value * waveform(Ophi1, OW1) * Ot;
            break;

          case 3: //808 Cymbal
            for(j=0; j<6; j++)
            {
              Oc[j][0] += 1.0f;

              if(Oc[j][0]>Oc[j][1])
              {
                Oc[j][0] -= Oc[j][1];
                Ot = OL * envData[ENV_OVERTONE1].value;
              }
            }
            Ocf1 = envData[ENV_OVERTONE2].value * OcF;  //filter freq
            Oc0 += Ocf1 * Oc1;
            Oc1 += Ocf1 * (Ot + Oc2 - OcQ * Oc1 - Oc0);  //bpf
            Oc2 = Ot;
            Ot = Oc1;
            break;
        }
      }

      if(MainFilter==1) //filter overtones
      {
        if(t<envData[ENV_FILTER].next)
          envData[ENV_FILTER].value = envData[ENV_FILTER].value + envData[ENV_FILTER].delta;
        else UpdateEnv(ENV_FILTER, t);

        MFtmp = envData[ENV_FILTER].value;
        if(MFtmp >0.2f)
          MFfb = 1.001f - (float)powf(10.0f, MFtmp - 1);
        else
          MFfb = 0.999f - 0.7824f * MFtmp;

        MFtmp = Ot + MFres * (1.f + (1.f/MFfb)) * (MFin - MFout);
        MFin = MFfb * (MFin - MFtmp) + MFtmp;
        MFout = MFfb * (MFout - MFin) + MFin;

        DF[t - tpos] = DF[t - tpos] + (MFout - (HighPass * Ot));
      }
      else if(MainFilter==2) //filter all
      {
        if(t<envData[ENV_FILTER].next)
          envData[ENV_FILTER].value = envData[ENV_FILTER].value + envData[ENV_FILTER].delta;
        else UpdateEnv(ENV_FILTER, t);

        MFtmp = envData[ENV_FILTER].value;
        if(MFtmp >0.2f)
          MFfb = 1.001f - (float)powf(10.0f, MFtmp - 1);
        else
          MFfb = 0.999f - 0.7824f * MFtmp;

        MFtmp = DF[t - tpos] + Ot + MFres * (1.f + (1.f/MFfb)) * (MFin - MFout);
        MFin = MFfb * (MFin - MFtmp) + MFtmp;
        MFout = MFfb * (MFout - MFin) + MFin;

        DF[t - tpos] = MFout - (HighPass * (DF[t - tpos] + Ot));
      }
      // PG: Ot is uninitialized
      else DF[t - tpos] = DF[t - tpos] + Ot; //no filter
    }

    if(DistOn) //bit resolution
    {
      for(j=0; j<BUFFER_SIZE; j++)
        DF[j] = DGain * (int)(DF[j] / DAtten);

      for(j=0; j<BUFFER_SIZE; j+=DStep) //downsampling
      {
        DownAve = 0;
        DownStart = j;
        DownEnd = j + DStep - 1;
        for(jj = DownStart; jj<=DownEnd; jj++)
          DownAve = DownAve + DF[jj];
        DownAve = DownAve / DStep;
        for(jj = DownStart; jj<=DownEnd; jj++)
          DF[jj] = DownAve;
      }
    }
    else for(j=0; j<BUFFER_SIZE; j++) DF[j] *= DGain;

    for(j = 0; j<BUFFER_SIZE; j++) //clipping + output
    {
      if(DF[j] > clippoint)
        wave[wavewords++] = clippoint;
      else if(DF[j] < -clippoint)
          wave[wavewords++] = -clippoint;
      else
          wave[wavewords++] = (short)DF[j];

      for (int c = 1; c < channels; c++)  {
        wave[wavewords] = wave[wavewords-1];
        wavewords++;
      }
    }

    tpos = tpos + BUFFER_SIZE;
  }

  return Length;
}

