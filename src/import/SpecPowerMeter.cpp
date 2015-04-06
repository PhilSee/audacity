/**********************************************************************

  Audacity: A Digital Audio Editor

  SpecPowerMeter.cpp

  Philipp Sibler

******************************************************************//**

\class SpecPowerMeter
\brief SpecPowerMeter is a simple spectral power level meter.

SpecPowerMeter operates in the Fourier domain and allows power level
measurements in subbands or in the entire signal band.  

*//*******************************************************************/

#include <cmath>
#include <cstdlib>
#include <wx/defs.h>

#include "../FFT.h"
#include "SpecPowerMeter.h"

SpecPowerMeter::SpecPowerMeter(int sigLen) :
   mEqEnabled(0)
{
   mSigLen = sigLen;
   
   // Init buffers
   mSigI = new float[sigLen];
   mSigFR = new float[sigLen];
   mSigFI = new float[sigLen];
   mEqMask = new float[sigLen];
   for (int n = 0; n < sigLen; n++)
   {
      mSigI[n] = 0.0f;
      mEqMask[n] = 1.0f;
   }
   
#ifdef SPECPOWER_SIGNAL_DEBUG
   // Build a debug writer
   mpWriter = new SpecDebugWriter("SpecPower.sig");
#endif

}

SpecPowerMeter::~SpecPowerMeter()
{
   delete[] mSigI;
   delete[] mSigFR;
   delete[] mSigFI;
   
#ifdef SPECPOWER_SIGNAL_DEBUG
   delete mpWriter;
#endif
}

float SpecPowerMeter::CalcPower(float* sig, float fc, float bw)
{
   float pwr;
   int loBin, hiBin;
   
   // Given the bandwidth bw, get the boundary bin numbers
   loBin = Freq2Bin(fc - (bw / 2.0f));
   hiBin = Freq2Bin(fc + (bw / 2.0f));
   if (loBin == hiBin)
   {
      hiBin = loBin + 1;
   }
   
   // Calc the FFT
   FFT(mSigLen, 0, sig, mSigI, mSigFR, mSigFI);
   
#ifdef SPECPOWER_SIGNAL_DEBUG
   mpWriter->WriteSignal(mSigFR, mSigLen);
   mpWriter->WriteSignal(mSigFI, mSigLen);
#endif
   
   // Calc the in-band power
   pwr = CalcBinPower(mSigFR, mSigFI, loBin, hiBin);
   
   return pwr;     
}

void SpecPowerMeter::SetEqualizer(float* eqmask, int masklen)
{
   // Copy mask into own buffer
   memcpy((void*) mEqMask, (void*) eqmask, masklen * sizeof(float));
}

void SpecPowerMeter::EnableEqualizer()
{
   mEqEnabled = 1;
}


void SpecPowerMeter::DisableEqualizer()
{
   mEqEnabled = 0;
}


float SpecPowerMeter::CalcBinPower(float* sig_f_r, float* sig_f_i, int loBin, int hiBin)
{
   float pwr = 0.0f;
   float re, im;
   
   if (mEqEnabled)
   {
      for (int n = loBin; n < hiBin; n++)
      {
         re = sig_f_r[n] * mEqMask[n];
         im = sig_f_i[n] * mEqMask[n];
         
         pwr += ((re*re)+(im*im));
      }
   }
   else
   {
      for (int n = loBin; n < hiBin; n++)
      {
         pwr += ((sig_f_r[n]*sig_f_r[n])+(sig_f_i[n]*sig_f_i[n]));
      }
   }
   
   
   return pwr;
}

int SpecPowerMeter::Freq2Bin(float fc)
{
   int bin;
   
   // There is no round() in (older) MSVSs ...
   bin = floor((double)fc * mSigLen);
   bin %= mSigLen;
   
   return bin;   
}

