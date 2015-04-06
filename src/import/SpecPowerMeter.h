/**********************************************************************

  Audacity: A Digital Audio Editor

  SpecPowerMeter.h

  Philipp Sibler

**********************************************************************/

#ifndef __AUDACITY_SPECPOWERMETER_H_
#define __AUDACITY_SPECPOWERMETER_H_

// #define SPECPOWER_SIGNAL_DEBUG 1

#ifdef SPECPOWER_SIGNAL_DEBUG

#include <cstdio>

class SpecDebugWriter
{
   FILE* mpFid;

public:
   SpecDebugWriter(const char* filename)
   {
      mpFid = fopen(filename, "wb");
   }

   ~SpecDebugWriter()
   {
      if (mpFid) fclose(mpFid);
   }
   
   void WriteSignal(float* buffer, size_t len)
   {
      WriteSignal(buffer, 4, len);
   }

   void WriteSignal(void* buffer, size_t size, size_t len)
   {
      fwrite(buffer, size, len, mpFid);
   }
};

#endif

class SpecPowerMeter
{
   int mSigLen;
   
   float* mSigI;
   float* mSigFR;
   float* mSigFI;
   
   int mEqEnabled;
   float* mEqMask;

#ifdef SPECPOWER_SIGNAL_DEBUG
   SpecDebugWriter* mpWriter;
#endif

   float CalcBinPower(float* sig_f_r, float* sig_f_i, int loBin, int hiBin);
   int Freq2Bin(float fc);
public:
   SpecPowerMeter(int sigLen);
   ~SpecPowerMeter();
   
   float CalcPower(float* sig, float fc, float bw);
   
   void SetEqualizer(float* eqmask, int masklen);
   void EnableEqualizer();
   void DisableEqualizer();
};

#endif

