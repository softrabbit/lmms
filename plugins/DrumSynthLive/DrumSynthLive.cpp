/*
 * DrumSynth.cpp - DrumSynth DS file renderer
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


#include "DrumSynthLive.h"

#include <sstream>
#include <iostream>
#include <cstring>

#include <cmath>

#include <QFile>

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

// const int     Fs    =  44100;
const float   TwoPi =  6.2831853f;
const int     MAX   =  0;
const int     ENV   =  1;
const int     PNT   =  2;
const int     dENV  =  3;
const int     NEXTT =  4;

// Envelope indexes
const int ENV_TONE = 1;
const int ENV_NOISE = 2;
const int ENV_OVERTONE1 = 3;
const int ENV_OVERTONE2 = 4;
const int ENV_NOISEBAND = 5;
const int ENV_NOISEBAND2 = 6;
const int ENV_FILTER = 7;

// Bah, I'll move these into the class once I sepearate DrumsynthFile from DrumSynth
// llama
float envpts[8][3][32];    //envelope/time-level/point
float envData[8][6];       //envelope running status
int   chkOn[8], sliLev[8]; //section on/off and level
float timestretch;         //overall time scaling
short DD[1200], clippoint;
float DF[1200];
float phi[1200];

long  wavewords, wavemode=0;
float mem_t=1.0f, mem_o=1.0f, mem_n=1.0f, mem_b=1.0f, mem_tune=1.0f, mem_time=1.0f;


int DrumSynth::LongestEnv(void)
{
  long e, eon, p;
  float l=0.f;

  for(e=1; e<7; e++) // The filter is excluded here, because... it's not a sound generator?
  {
    eon = e - 1;
    if(eon>2) eon=eon-1;

    p = 0;
    while (envpts[e][0][p + 1] >= 0.f) p++;
    envData[e][MAX] = envpts[e][0][p] * timestretch;
    if(chkOn[eon]==1) l = max(l, envData[e][MAX]);
  }
  //l *= timestretch;

  return 2400 + (1200 * (int)(l / 1200));
}


float DrumSynth::LoudestEnv(void)
{
  float loudest=0.f;
  int i=0;

  while (i<5) //2
  {
	  if(chkOn[i]==1) loudest = max(loudest, (float)sliLev[i]);
	  i++;
  }
  return (loudest * loudest);
}


void DrumSynth::UpdateEnv(int e, long t)
{
  float endEnv, dT;
                                                             //0.2's added
  envData[e][NEXTT] = envpts[e][0][(long)(envData[e][PNT] + 1.f)] * timestretch; //get next point
  if(envData[e][NEXTT] < 0) {
	  envData[e][NEXTT] = 442000 * timestretch; //if end point, hold
  }
  envData[e][ENV] = envpts[e][1][(long)(envData[e][PNT] + 0.f)] * 0.01f; //this level
  endEnv = envpts[e][1][(long)(envData[e][PNT] + 1.f)] * 0.01f;          //next level
  dT = envData[e][NEXTT] - (float)t;
  dT = max(dT, 1.0f);
  envData[e][dENV] = (endEnv - envData[e][ENV]) / dT;
  envData[e][PNT] = envData[e][PNT] + 1.0f;
}


void DrumSynth::GetEnv(int env, const char *sec, const char *key, QString ini)
{
  char en[256], s[8];
  int i=0, o=0, ep=0;
  GetPrivateProfileString(sec, key, "0,0 100,0", en, sizeof(en), ini);

  //be safe!
  en[255]=0;
  s[0]=0;

  while(en[i]!=0)
  {
    if(en[i] == ',')
    {
      if(sscanf(s, "%f", &envpts[env][0][ep])==0) envpts[env][0][ep] = 0.f;
      o=0;
    }
    else if(en[i] == ' ')
    {
      if(sscanf(s, "%f", &envpts[env][1][ep])==0) envpts[env][1][ep] = 0.f;
      o=0; ep++;
    }
    else { s[o]=en[i]; o++; s[o]=0; }
    i++;
  }
  if(sscanf(s, "%f", &envpts[env][1][ep])==0) envpts[env][1][ep] = 0.f;
  envpts[env][0][ep + 1] = -1;

  envData[env][MAX] = envpts[env][0][ep];
}


float DrumSynth::waveform(float ph, int form)
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


QByteArray DrumSynth::LoadFile(QString file)
{
    // Use QFile to handle unicode file name on Windows
    QFile f(file);
    f.open(QIODevice::ReadOnly);
    return f.readAll().constData();
}


int DrumSynth::GetPrivateProfileString(const char *sec, const char *key, const char *def, char *buffer, int size, QString file)
{
    bool inSection = false;
    char *line;
    char *k, *b;
    int len = 0;

    // This up from 200, because envelopes were getting cut off
    const int lineLength = 400;
    line = (char*)malloc(lineLength);

    if(dat == nullptr) {
            dat = LoadFile(file);
            is.str(string(dat.constData(), dat.size()));
    }
    
    is.seekg(0, ios_base::beg);
    while (is.good()) {
        if (!inSection) {
            is.ignore( numeric_limits<streamsize>::max(), '[');

            if (!is.eof()) {
                is.getline(line, lineLength, ']');
                if (strcasecmp(line, sec)==0) {
                    inSection = true;
                }
            }
        }
        else if (!is.eof()) {
            is.getline(line, lineLength);
            if (line[0] == '[')
                break;

            k = strtok(line, " \t=");
            b = strtok(NULL, "\n\r\0");

            if (k != 0 && strcasecmp(k, key)==0) {
                if (b==0) {
                    len = 0;
                    buffer[0] = 0;
                }
                else {
                    k = (char *)(b + strlen(b)-1);
                    while ( (k>=b) && (*k==' ' || *k=='\t') )
                        --k;
                    *(k+1) = '\0';

                    len = strlen(b);
                    if (len > size-1) len = size-1;
                    strncpy(buffer, b, len+1);
                }
                break;
            }
        }
    }

    if (len == 0) {
        len = strlen(def);
        strncpy(buffer, def, size);
    }

    free(line);
    return len;
}

int DrumSynth::GetPrivateProfileInt(const char *sec, const char *key, int def, QString file)
{
  char tmp[16];
  int i=0;

  GetPrivateProfileString(sec, key, "", tmp, sizeof(tmp), file);
  sscanf(tmp, "%d", &i); if(tmp[0]==0) i=def;

  return i;
}

float DrumSynth::GetPrivateProfileFloat(const char *sec, const char *key, float def, QString file)
{
    char tmp[16];
    float f=0.f;

    GetPrivateProfileString(sec, key, "", tmp, sizeof(tmp), file);
    sscanf(tmp, "%f", &f); if(tmp[0]==0) f=def;

    return f;
}


//  Constantly opening and scanning each file for the setting really sucks.
//  But, the original did this, and we know it works.  Will store file into
//  an associative array or something once we have a datastructure to load in to.
//  llama

int DrumSynth::GetDSFileSamples(QString dsfile, int16_t *&wave, int channels, sample_rate_t Fs)
{
  //input file
  char sec[32];
  char ver[32];
  char comment[256];
  int commentLen=0;

  //generation
  long  Length, tpos=0, tplus, totmp, t, i, j;
  float x[3] = {0.f, 0.f, 0.f};
  float MasterTune, randmax, randmax2;
  int   MainFilter, HighPass;

  long  NON, NT, TON, DiON, TDroop=0, DStep;
  float a, b=0.f, c=0.f, d=0.f, g, TT=0.f, TL, NL, F1, F2;
  float TphiStart=0.f, Tphi, TDroopRate, ddF, DAtten, DGain;

  long  BON, BON2, BFStep, BFStep2, botmp;
  float BdF=0.f, BdF2=0.f, BPhi, BPhi2, BF, BF2, BQ, BQ2, BL, BL2;

  long  OON, OF1Sync=0, OF2Sync=0, OMode, OW1, OW2;
  float Ophi1, Ophi2, OF1, OF2, OL, Ot=0 /*PG: init */, OBal1, OBal2, ODrive;
  float Ocf1, Ocf2, OcF, OcQ, OcA, Oc[6][2];  //overtone cymbal mode
  float Oc0=0.0f, Oc1=0.0f, Oc2=0.0f;

  float MFfb, MFtmp, MFres, MFin=0.f, MFout=0.f;
  float DownAve;
  long  DownStart, DownEnd, jj;


  if(wavemode==0) //semi-real-time adjustments if working in memory!!
  {
    mem_t = 1.0f;
    mem_o = 1.0f;
    mem_n = 1.0f;
    mem_b = 1.0f;
    mem_tune = 0.0f;
    mem_time = 1.0f;
  }

  //try to read version from input file
  strcpy(sec, "General");
  GetPrivateProfileString(sec,"Version","",ver,sizeof(ver),dsfile);
  ver[9]=0;
  if(strcasecmp(ver, "DrumSynth") != 0) {return 0;} //input fail
  if(ver[11] != '1' && ver[11] != '2') {return 0;} //version fail


  //read master parameters
  GetPrivateProfileString(sec,"Comment","",comment,sizeof(comment),dsfile);
  while((comment[commentLen]!=0) && (commentLen<254)) commentLen++;
  if(commentLen==0) {
	  comment[0]=32;
	  comment[1]=0;
	  commentLen=1;
  }
  comment[commentLen+1]=0; commentLen++;
  if((commentLen % 2)==1) commentLen++;


  timestretch = .01f * mem_time * GetPrivateProfileFloat(sec,"Stretch",100.0,dsfile);
  if(timestretch<0.2f) timestretch=0.2f;
  if(timestretch>10.f) timestretch=10.f;
  // the unit of envelope lengths is a sample in 44100Hz sample rate, so correct it
  timestretch *= Fs / 44100.f;

  DGain = 1.0f; //leave this here!
  DGain = (float)powf(10.0, 0.05 * GetPrivateProfileFloat(sec,"Level",0,dsfile));

  MasterTune = GetPrivateProfileFloat(sec,"Tuning",0.0,dsfile);
  MasterTune = (float)powf(1.0594631f, MasterTune + mem_tune);
  MainFilter = 2 * GetPrivateProfileInt(sec,"Filter",0,dsfile);
  MFres = 0.0101f * GetPrivateProfileFloat(sec,"Resonance",0.0,dsfile);
  MFres = (float)powf(MFres, 0.5f);

  HighPass = GetPrivateProfileInt(sec,"HighPass",0,dsfile);
  GetEnv(ENV_FILTER, sec, "FilterEnv", dsfile);


  //read noise parameters
  strcpy(sec, "Noise");
  chkOn[1] = GetPrivateProfileInt(sec,"On",0,dsfile);
  sliLev[1] = GetPrivateProfileInt(sec,"Level",0,dsfile);
  NT =  GetPrivateProfileInt(sec,"Slope",0,dsfile);
  GetEnv(ENV_NOISE, sec, "Envelope", dsfile);
  NON = chkOn[1];
  NL = (float)(sliLev[1] * sliLev[1]) * mem_n;
  if(NT<0)
  {
	  a = 1.f + (NT / 105.f);
	  d = -NT / 105.f;
	  g = (1.f + 0.0005f * NT * NT) * NL;
  } else {
	  a = 1.f;
	  b = -NT / 50.f;
	  c = (float)fabs((float)NT) / 100.f;
	  g = NL;
  }

  // Always a fixed random number sequence for now, remember to enable this option
  // when done coding...
  //if(GetPrivateProfileInt(sec,"FixedSeq",0,dsfile)!=0)
  srand(1); //

   //read tone parameters
  strcpy(sec, "Tone");
  chkOn[0] = GetPrivateProfileInt(sec,"On",0,dsfile); TON = chkOn[0];
  sliLev[0] = GetPrivateProfileInt(sec,"Level",128,dsfile);
  TL = (float)(sliLev[0] * sliLev[0]) * mem_t;
  GetEnv(ENV_TONE, sec, "Envelope", dsfile);
  F1 = MasterTune * TwoPi * GetPrivateProfileFloat(sec,"F1",200.0,dsfile) / Fs;
  F1 = max(F1,0.001f); //to prevent overtone ratio div0
  F2 = MasterTune * TwoPi * GetPrivateProfileFloat(sec,"F2",120.0,dsfile) / Fs;
  TDroopRate = GetPrivateProfileFloat(sec,"Droop",0.f,dsfile);
  if(TDroopRate>0.f)
  {
    TDroopRate = (float)powf(10.0f, (TDroopRate - 20.0f) / 30.0f);
    TDroopRate = TDroopRate * -4.f / envData[ENV_TONE][MAX];
    TDroop = 1;
    F2 = F1+((F2-F1)/(1.f-(float)exp(TDroopRate * envData[ENV_TONE][MAX])));
    ddF = F1 - F2;
  }
  else ddF = F2-F1;

  Tphi = GetPrivateProfileFloat(sec,"Phase",90.f,dsfile) / 57.29578f; //degrees>radians

  //read overtone parameters
  strcpy(sec, "Overtones");
  chkOn[2] = GetPrivateProfileInt(sec,"On",0,dsfile); OON = chkOn[2];
  sliLev[2] = GetPrivateProfileInt(sec,"Level",128,dsfile);
  OL = (float)(sliLev[2] * sliLev[2]) * mem_o;
  GetEnv(ENV_OVERTONE1, sec, "Envelope1", dsfile);
  GetEnv(ENV_OVERTONE2, sec, "Envelope2", dsfile);
  OMode = GetPrivateProfileInt(sec,"Method",2,dsfile);
  OF1 = MasterTune * TwoPi * GetPrivateProfileFloat(sec,"F1",200.0,dsfile) / Fs;
  OF2 = MasterTune * TwoPi * GetPrivateProfileFloat(sec,"F2",120.0,dsfile) / Fs;
  OW1 = GetPrivateProfileInt(sec,"Wave1",0,dsfile);
  OW2 = GetPrivateProfileInt(sec,"Wave2",0,dsfile);
  OBal2 = (float)GetPrivateProfileInt(sec,"Param",50,dsfile);
  ODrive = (float)powf(OBal2, 3.0f) / (float)powf(50.0f, 3.0f);
  OBal2 *= 0.01f;
  OBal1 = 1.f - OBal2;
  Ophi1 = Tphi;
  Ophi2 = Tphi;
  if(MainFilter==0)
    MainFilter = GetPrivateProfileInt(sec,"Filter",0,dsfile);
  if((GetPrivateProfileInt(sec,"Track1",0,dsfile)==1) && (TON==1))
  { OF1Sync = 1;  OF1 = OF1 / F1; }
  if((GetPrivateProfileInt(sec,"Track2",0,dsfile)==1) && (TON==1))
  { OF2Sync = 1;  OF2 = OF2 / F1; }

  OcA = 0.28f + OBal1 * OBal1;  //overtone cymbal mode
  OcQ = OcA * OcA;
  OcF = (1.8f - 0.7f * OcQ) * 0.92f; //multiply by env 2
  OcA *= 1.0f + 4.0f * OBal1;  //level is a compromise!
  Ocf1 = TwoPi / OF1;
  Ocf2 = TwoPi / OF2;
  for(i=0; i<6; i++) Oc[i][0] = Oc[i][1] = Ocf1 + (Ocf2 - Ocf1) * 0.2f * (float)i;

  //read noise band parameters
  strcpy(sec, "NoiseBand");
  chkOn[3] = GetPrivateProfileInt(sec,"On",0,dsfile); BON = chkOn[3];
  sliLev[3] = GetPrivateProfileInt(sec,"Level",128,dsfile);
  BL = (float)(sliLev[3] * sliLev[3]) * mem_b;
  BF = MasterTune * TwoPi * GetPrivateProfileFloat(sec,"F",1000.0,dsfile) / Fs;
  BPhi = TwoPi / 8.f;
  GetEnv(ENV_NOISEBAND, sec, "Envelope", dsfile);
  BFStep = GetPrivateProfileInt(sec,"dF",50,dsfile);
  BQ = (float)BFStep;
  BQ = BQ * BQ / (10000.f-6600.f*((float)sqrt(BF)-0.19f));
  BFStep = 1 + (int)((40.f - (BFStep / 2.5f)) / (BQ + 1.f + (1.f * BF)));

  strcpy(sec, "NoiseBand2");
  chkOn[4] = GetPrivateProfileInt(sec,"On",0,dsfile); BON2 = chkOn[4];
  sliLev[4] = GetPrivateProfileInt(sec,"Level",128,dsfile);
  BL2 = (float)(sliLev[4] * sliLev[4]) * mem_b;
  BF2 = MasterTune * TwoPi * GetPrivateProfileFloat(sec,"F",1000.0,dsfile) / Fs;
  BPhi2 = TwoPi / 8.f;
  GetEnv(ENV_NOISEBAND2, sec, "Envelope", dsfile);
  BFStep2 = GetPrivateProfileInt(sec,"dF",50,dsfile);
  BQ2 = (float)BFStep2;
  BQ2 = BQ2 * BQ2 / (10000.f-6600.f*((float)sqrt(BF2)-0.19f));
  BFStep2 = 1 + (int)((40 - (BFStep2 / 2.5)) / (BQ2 + 1 + (1 * BF2)));

  //read distortion parameters
  strcpy(sec, "Distortion");
  chkOn[5] = GetPrivateProfileInt(sec,"On",0,dsfile); DiON = chkOn[5];
  DStep = 1 + GetPrivateProfileInt(sec,"Rate",0,dsfile);
  if(DStep==7) DStep=20;
  if(DStep==6) DStep=10;
  if(DStep==5) DStep=8;

  clippoint = 32700;
  DAtten = 1.0f;

  if(DiON==1)
  {
    DAtten = DGain * (short)LoudestEnv();
    if(DAtten>32700) clippoint=32700; else clippoint=(short)DAtten;
    DAtten = (float)powf(2.0, 2.0 * GetPrivateProfileInt(sec,"Bits",0,dsfile));
    DGain = DAtten * DGain * (float)powf(10.0, 0.05 * GetPrivateProfileInt(sec,"Clipping",0,dsfile));
  }

  //prepare envelopes
  randmax = 1.f / RAND_MAX; randmax2 = 2.f * randmax;
  for (i=1;i<8;i++) {
	  envData[i][NEXTT]=0;
	  envData[i][PNT]=0;
  }
  Length = LongestEnv();

  //allocate the buffer
  //if(wave!=NULL) free(wave);
  //wave = new int16_t[channels * (Length + 1280)]; //wave memory buffer
  wave = new int16_t[channels * Length]; //wave memory buffer
  if(wave==NULL) {return 0;}
  wavewords = 0;

  /////////////////////////////////////////////
  //generate
  tpos = 0;
  while(tpos<Length)
  {
    tplus = tpos + 1199;

    if(NON==1) //noise
    {
      for(t=tpos; t<=tplus; t++)
      {
        if(t < envData[ENV_NOISE][NEXTT]) envData[ENV_NOISE][ENV] = envData[ENV_NOISE][ENV] + envData[ENV_NOISE][dENV];
        else UpdateEnv(ENV_NOISE, t);
        x[2] = x[1];
        x[1] = x[0];
        x[0] = (randmax2 * (float)rand()) - 1.f;
        TT = a * x[0] + b * x[1] + c * x[2] + d * TT;
        DF[t - tpos] = TT * g * envData[ENV_NOISE][ENV];
      }
      if(t>=envData[ENV_NOISE][MAX]) NON=0;
    }
    else {
        for(j=0; j<1200; j++) DF[j]=0.f;
    }

    if(TON==1) //tone
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
          phi[t - tpos] = F1 + (t / envData[ENV_TONE][MAX]) * ddF;
      }
      for(t=tpos; t<=tplus; t++)
      {
        totmp = t - tpos;
        if(t < envData[ENV_TONE][NEXTT])
          envData[ENV_TONE][ENV] = envData[ENV_TONE][ENV] + envData[ENV_TONE][dENV];
        else UpdateEnv(ENV_TONE, t);
        Tphi = Tphi + phi[totmp];
        DF[totmp] += TL * envData[ENV_TONE][ENV] * (float)sin(fmod(Tphi,TwoPi));//overflow?
      }
      if(t>=envData[ENV_TONE][MAX]) TON=0;
    }
    else for(j=0; j<1200; j++) phi[j]=F2; //for overtone sync

    if(BON==1) //noise band 1
    {
      for(t=tpos; t<=tplus; t++)
      {
        if(t < envData[ENV_NOISEBAND][NEXTT])
          envData[ENV_NOISEBAND][ENV] = envData[ENV_NOISEBAND][ENV] + envData[ENV_NOISEBAND][dENV];
        else UpdateEnv(ENV_NOISEBAND, t);
        if((t % BFStep) == 0) BdF = randmax * (float)rand() - 0.5f;
        BPhi = BPhi + BF + BQ * BdF;
        botmp = t - tpos;
        DF[botmp] = DF[botmp] + (float)cos(fmod(BPhi,TwoPi)) * envData[ENV_NOISEBAND][ENV] * BL;
      }
      if(t>=envData[ENV_NOISEBAND][MAX]) BON=0;
    }

    if(BON2==1) //noise band 2
    {
      for(t=tpos; t<=tplus; t++)
      {
        if(t < envData[ENV_NOISEBAND2][NEXTT])
          envData[ENV_NOISEBAND2][ENV] = envData[ENV_NOISEBAND2][ENV] + envData[ENV_NOISEBAND2][dENV];
        else UpdateEnv(ENV_NOISEBAND2, t);
        if((t % BFStep2) == 0) BdF2 = randmax * (float)rand() - 0.5f;
        BPhi2 = BPhi2 + BF2 + BQ2 * BdF2;
        botmp = t - tpos;
        DF[botmp] = DF[botmp] + (float)cos(fmod(BPhi2,TwoPi)) * envData[ENV_NOISEBAND2][ENV] * BL2;
      }
      if(t>=envData[ENV_NOISEBAND2][MAX]) BON2=0;
    }


    for (t=tpos; t<=tplus; t++)
    {
      if(OON==1) //overtones
      {
        if(t<envData[ENV_OVERTONE1][NEXTT])
          envData[ENV_OVERTONE1][ENV] = envData[ENV_OVERTONE1][ENV] + envData[ENV_OVERTONE1][dENV];
        else
        {
          if(t>=envData[ENV_OVERTONE1][MAX]) //wait for OT2
          {
            envData[ENV_OVERTONE1][ENV] = 0;
            envData[ENV_OVERTONE1][dENV] = 0;
            envData[ENV_OVERTONE1][NEXTT] = 999999;
          }
          else UpdateEnv(ENV_OVERTONE1, t);
        }
        //
        if(t<envData[ENV_OVERTONE2][NEXTT])
          envData[ENV_OVERTONE2][ENV] = envData[ENV_OVERTONE2][ENV] + envData[ENV_OVERTONE2][dENV];
        else
        {
          if(t>=envData[ENV_OVERTONE2][MAX]) //wait for OT1
          {
            envData[ENV_OVERTONE2][ENV] = 0;
            envData[ENV_OVERTONE2][dENV] = 0;
            envData[ENV_OVERTONE2][NEXTT] = 999999;
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
            Ot = OBal1 * envData[ENV_OVERTONE1][ENV] * waveform(Ophi1, OW1);
            Ot = OL * (Ot + OBal2 * envData[ENV_OVERTONE2][ENV] * waveform(Ophi2, OW2));
            break;

          case 1: //FM
            Ot = ODrive * envData[ENV_OVERTONE2][ENV] * waveform(Ophi2, OW2);
            Ot = OL * envData[ENV_OVERTONE1][ENV] * waveform(Ophi1 + Ot, OW1);
            break;

          case 2: //RM
            Ot = (1 - ODrive / 8) + (((ODrive / 8) * envData[ENV_OVERTONE2][ENV]) * waveform(Ophi2, OW2));
            Ot = OL * envData[ENV_OVERTONE1][ENV] * waveform(Ophi1, OW1) * Ot;
            break;

          case 3: //808 Cymbal
            for(j=0; j<6; j++)
            {
              Oc[j][0] += 1.0f;

              if(Oc[j][0]>Oc[j][1])
              {
                Oc[j][0] -= Oc[j][1];
                Ot = OL * envData[ENV_OVERTONE1][ENV];
              }
            }
            Ocf1 = envData[ENV_OVERTONE2][ENV] * OcF;  //filter freq
            Oc0 += Ocf1 * Oc1;
            Oc1 += Ocf1 * (Ot + Oc2 - OcQ * Oc1 - Oc0);  //bpf
            Oc2 = Ot;
            Ot = Oc1;
            break;
        }
      }

      if(MainFilter==1) //filter overtones
      {
        if(t<envData[ENV_FILTER][NEXTT])
          envData[ENV_FILTER][ENV] = envData[ENV_FILTER][ENV] + envData[ENV_FILTER][dENV];
        else UpdateEnv(ENV_FILTER, t);

        MFtmp = envData[ENV_FILTER][ENV];
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
        if(t<envData[ENV_FILTER][NEXTT])
          envData[ENV_FILTER][ENV] = envData[ENV_FILTER][ENV] + envData[ENV_FILTER][dENV];
        else UpdateEnv(ENV_FILTER, t);

        MFtmp = envData[ENV_FILTER][ENV];
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

    if(DiON==1) //bit resolution
    {
      for(j=0; j<1200; j++)
        DF[j] = DGain * (int)(DF[j] / DAtten);

      for(j=0; j<1200; j+=DStep) //downsampling
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
    else for(j=0; j<1200; j++) DF[j] *= DGain;

    for(j = 0; j<1200; j++) //clipping + output
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

    tpos = tpos + 1200;
  }

  return Length;
}

