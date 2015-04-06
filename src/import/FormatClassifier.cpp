/**********************************************************************

  Audacity: A Digital Audio Editor

  FormatClassifier.cpp

  Philipp Sibler

******************************************************************//**

\class FormatClassifier
\brief FormatClassifier classifies the sample format and endianness of
raw audio files.

The classifier operates in the frequency domain and exploits 
the low-pass-like spectral behaviour of natural audio signals 
for classification of the sample format and the used endianness.

*//*******************************************************************/
#define _USE_MATH_DEFINES
#include <stdint.h>
#include <cmath>
#include <cfloat>
#include <vector>
#include <cstdio>

#include <wx/defs.h>

#include "MultiFormatReader.h"
#include "SpecPowerMeter.h"
#include "sndfile.h"

#include "FormatClassifier.h"

FormatClassifier::FormatClassifier(const char* filename) :
   mFiltSiglen(cSiglen / cPolyTaps),
   mReader(filename),
   mMeter(mFiltSiglen)
{
   FormatClassT fClass;
   
   // Resize buffers to their working sizes
   mSigBuffer.resize(cSiglen);
   mAuxBuffer.resize(cSiglen);   
   mWinBuffer.resize(cSiglen);
   mEqBuffer.resize(mFiltSiglen);
   mRawBuffer.resize(cRawSiglen);

   // Define the classification classes
   fClass.endian = MachineEndianness::Little;
   fClass.format = MultiFormatReader::Int8;
   mClasses.push_back(fClass);
   fClass.format = MultiFormatReader::Int16;
   mClasses.push_back(fClass);
   fClass.format = MultiFormatReader::Uint8;
   mClasses.push_back(fClass);
   fClass.format = MultiFormatReader::Float;
   mClasses.push_back(fClass);   
   fClass.format = MultiFormatReader::Double;
   mClasses.push_back(fClass);

   fClass.endian = MachineEndianness::Big;
   fClass.format = MultiFormatReader::Int8;
   mClasses.push_back(fClass);
   fClass.format = MultiFormatReader::Int16;
   mClasses.push_back(fClass);
   fClass.format = MultiFormatReader::Uint8;
   mClasses.push_back(fClass);
   fClass.format = MultiFormatReader::Float;
   mClasses.push_back(fClass);   
   fClass.format = MultiFormatReader::Double;
   mClasses.push_back(fClass);

   // Find signal start offset
   mSignalStart = cHeaderSkip;
   FindSignalStart();

   // Build feature vectors
   mMonoFeat = new float[mClasses.size()];
   mStereoFeat = new float[mClasses.size()];
   
   // Calc window signal for polyphase input filter
   CalcSincwin(&mWinBuffer[0], cSiglen);
   
   // Calc equalizer mask and set it to power meter
   CalcEqualizerMask(&mEqBuffer[0], mFiltSiglen);
   mMeter.SetEqualizer(&mEqBuffer[0], mFiltSiglen);
   
#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
   // Build a debug writer
   char dfile [1024];
   sprintf(dfile, "%s.sig", filename);
   mpWriter = new DebugWriter(dfile);
#endif

   // Run it
   Run();   
   
#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
   for (unsigned int n = 0; n < mClasses.size(); n++)
   {
      printf("C[%i] M[%i]: Mono: %3.3f Stereo: %3.3f PLo: %3.3f PHiM: %3.3f dB PHiS: %3.3f dB\n", 
            mClasses[n].format, mClasses[n].endian, mMonoFeat[n], mStereoFeat[n], mPLo[n], 10.0f*log10f(mPHiM[n]), 10.0f*log10f(mPHiS[n]));
   }
#endif

}

FormatClassifier::~FormatClassifier()
{
   delete[] mMonoFeat;
   delete[] mStereoFeat;

#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
   delete mpWriter;
#endif
}

FormatClassifier::FormatClassT FormatClassifier::GetResultFormat()
{
   return mResultFormat;
}

int FormatClassifier::GetResultFormatLibSndfile()
{
   int format = SF_FORMAT_RAW;
   
   switch(mResultFormat.format)
   {
      case MultiFormatReader::Int8:
         format |= SF_FORMAT_PCM_S8;
         break;
      case MultiFormatReader::Int16:
         format |= SF_FORMAT_PCM_16;
         break;
      case MultiFormatReader::Int32:
         format |= SF_FORMAT_PCM_32;
         break;
      case MultiFormatReader::Uint8:
         format |= SF_FORMAT_PCM_U8;
         break;
      case MultiFormatReader::Float:
         format |= SF_FORMAT_FLOAT;
         break;
      case MultiFormatReader::Double:
         format |= SF_FORMAT_DOUBLE;
         break;
      default:
         format |= SF_FORMAT_PCM_16;
         break;
   }
   
   switch(mResultFormat.endian)
   {
      case MachineEndianness::Little:
         format |= SF_ENDIAN_LITTLE;
         break;
      case MachineEndianness::Big:
         format |= SF_ENDIAN_BIG;
         break;
   }
   
   return format;
}

int FormatClassifier::GetResultChannels()
{
   return mResultChannels;
}

void FormatClassifier::Run()
{
   mPLo.clear();
   mPHiM.clear();
   mPHiS.clear();

   // Calc the mono feature vector
   // Enable the dither noise equalizer
   mMeter.EnableEqualizer();
   
   for (unsigned int n = 0; n < mClasses.size(); n++)
   {
      // Read the signal
      ReadSignal(mClasses[n], 1);
#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
      mpWriter->WriteSignal(&mSigBuffer[0], cSiglen);
#endif

      // Do some preprocessing
      
      // Apply polyphase filtering
      FilterPolyphase(&mSigBuffer[0], &mAuxBuffer[0], &mWinBuffer[0], cPolyTaps, cSiglen);
      
      // Remove DC offset
      float smean = Mean(&mAuxBuffer[0], mFiltSiglen);
      Sub(&mAuxBuffer[0], smean, mFiltSiglen);
      
      // Normalize to a common RMS value
      float rms = Rms(&mAuxBuffer[0], mFiltSiglen);
      Div(&mAuxBuffer[0], rms, mFiltSiglen);
      
      // Now actually fill the feature vector
      // Low to high band power ratio
      mPLo.push_back(mMeter.CalcPower(&mAuxBuffer[0], 0.25f, 0.5f));
      mPHiM.push_back(mMeter.CalcPower(&mAuxBuffer[0], 0.45f, 0.1f)); 
      mMonoFeat[n] = mPLo[n] / mPHiM[n];
   }

   // Calc the stereo feature vector
   // Disable the dither noise equalizer
   mMeter.DisableEqualizer();   
   
   for (unsigned int n = 0; n < mClasses.size(); n++)
   {
      // Read the signal
      ReadSignal(mClasses[n], 2);
#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
      mpWriter->WriteSignal(mSigBuffer, cSiglen);
#endif

      // Do some preprocessing
            
      // Apply polyphase filtering
      FilterPolyphase(&mSigBuffer[0], &mAuxBuffer[0], &mWinBuffer[0], cPolyTaps, cSiglen);
      
      // Remove DC offset
      float smean = Mean(&mAuxBuffer[0], mFiltSiglen);
      Sub(&mAuxBuffer[0], smean, mFiltSiglen);
      
      // Normalize to a common RMS value
      float rms = Rms(&mAuxBuffer[0], mFiltSiglen);
      Div(&mAuxBuffer[0], rms, mFiltSiglen);
      
      // Now actually fill the feature vector
      // Low to high band power ratio
      // float pLo = mMeter.CalcPower(mAuxBuffer, 0.15f, 0.3f);
      mPHiS.push_back(mMeter.CalcPower(&mAuxBuffer[0], 0.40f, 0.2f));
      mStereoFeat[n] = mPLo[n] / mPHiS[n];
   }

   // Get the results
   size_t midx, sidx;
   float monoMax = Max(mMonoFeat, mClasses.size(), &midx);
   float stereoMax = Max(mStereoFeat, mClasses.size(), &sidx);

#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
   printf("monoMax is %f\n", monoMax);
   printf("stereoMax is %f\n", stereoMax);
#endif

   if (monoMax > stereoMax)
   {
      mResultChannels = 1;
      mResultFormat = mClasses[midx];
   }
   else
   {
      mResultChannels = 2;
      mResultFormat = mClasses[sidx];
   }

}



void FormatClassifier::ReadSignal(FormatClassT format, size_t stride)
{
   size_t actRead = 0;
   unsigned int n = 0;

   mReader.Reset(mSignalStart);

   do
   {
      actRead = mReader.ReadSamples(&mRawBuffer[0], cSiglen, stride, format.format, format.endian);

      if (n == 0)
      {
         ConvertSamples(&mRawBuffer[0], &mSigBuffer[0], format);
      }
      else
      {
         if (actRead == cSiglen)
         {
            ConvertSamples(&mRawBuffer[0], &mAuxBuffer[0], format);

            // Integrate signals
            Add(&mSigBuffer[0], &mAuxBuffer[0], cSiglen);

            // Do some dummy reads to break signal coherence
            mReader.ReadSamples(&mRawBuffer[0], n + 1, stride, format.format, format.endian);
         }
      }

      n++;

   } while ((n < cNumInts) && (actRead == cSiglen));
   
#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
   printf("ReadSignal: Number of integrated windows: %i\n", n);
#endif

}

void FormatClassifier::FindSignalStart()
{
   size_t actRead = 0;
   float rms = 0.0f;
   char signalFound = 0;
   size_t n;
   int i = 0;
   FormatClassT testFormat;
   
   testFormat.endian = MachineEndianness::Little;
   testFormat.format = MultiFormatReader::Uint8;
   
   // Do a dummy read of 1024 bytes to skip potential header information
   mReader.Reset();
   mReader.ReadSamples(&mRawBuffer[0], cHeaderSkip, MultiFormatReader::Uint8, MachineEndianness::Little);

   // Zero out raw buffer
   memset(&mRawBuffer[0], 0, cRawSiglen);
   
   // Do a first read
   actRead = mReader.ReadSamples(&mRawBuffer[0], cSiglen, testFormat.format, testFormat.endian);
   ConvertSamples(&mRawBuffer[0], &mSigBuffer[0], testFormat);

   while ((actRead == cSiglen) && !(rms != rms))
   {
      // Only do a RMS calculation based on the first few samples, that's enough
      rms = Rms(&mSigBuffer[0], 64);
         
      if (rms >= cMinRms)
      {
         signalFound = 1;
         break;
      }
      else
      {
         for (n = 0; n < cSigSearchGridSize; n++)
         {
            actRead = mReader.ReadSamples(&mRawBuffer[0], cSiglen, testFormat.format, testFormat.endian);
         }
         
         if (actRead == cSiglen)
         {
            ConvertSamples(&mRawBuffer[0], &mSigBuffer[0], testFormat);
            i++;
         }
      }
      
   }
   
   if (signalFound > 0)
   {
      // Now calculate the signal start offset in bytes
      mSignalStart = cHeaderSkip + (i * cSigSearchGridSize * cSiglen);
   }
   else
   {
      mSignalStart = cHeaderSkip;
   }
      
   
#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
   if (signalFound > 0)
   {
      printf("\nFindSignalStart: Signal found after %i repetitions.\n", i);
   }
   else
   {
      printf("\nFindSignalStart: No signal found after %i repetitions.\n", i);
   }
#endif
 
}

void FormatClassifier::ConvertSamples(void* in, float* out, FormatClassT format)
{
   switch(format.format)
   {
      case MultiFormatReader::Int8:
         ToFloat((int8_t*) in, out, cSiglen);
         break;
      case MultiFormatReader::Int16:
         ToFloat((int16_t*) in, out, cSiglen);
         break;
      case MultiFormatReader::Int32:
         ToFloat((int32_t*) in, out, cSiglen);
         break;
      case MultiFormatReader::Uint8:
         ToFloat((uint8_t*) in, out, cSiglen);
         break;
      case MultiFormatReader::Uint16:
         ToFloat((uint16_t*) in, out, cSiglen);
         break;
      case MultiFormatReader::Uint32:
         ToFloat((uint32_t*) in, out, cSiglen);
        break;
      case MultiFormatReader::Float:
         ToFloat((float*) in, out, cSiglen);
         break;
      case MultiFormatReader::Double:
         ToFloat((double*) in, out, cSiglen);
         break;
   }
}

void FormatClassifier::CalcSincwin(float* buffer, size_t len)
{
   float x = 0.0f;
   float M = len - 1.0f;

   for (size_t n = 0; n < len; n++)
   {
      x = (((4.0f * M_PI) / M) * n) - (2.0f * M_PI);
      buffer[n] = sinf(x) / x;

      // Multiply an additional Hann window to attenuate sidelobes further down
      buffer[n] *= 0.5f * (1.0f - cosf((2.0f * M_PI * n) / M));
   }
}

void FormatClassifier::CalcEqualizerMask(float* buffer, size_t len)
{
   float fLen = (float) len;
   size_t len2 = len / 2;
   float A = powf(10.0f, (-cDitherA / 20.0f));
   float m = (A - 1.0f) / (cDitherF2 - cDitherF1);
   
   for (size_t n = 0; n < len2; n++)
   {
      float f = ((float) n) / fLen;
      
      // Now select the different parts 
      // of the equalizer mask
      
      if (f < cDitherF1)
      {
         buffer[n] = 1.0f;
      }
      
      if ((f >= cDitherF1) && (f < cDitherF2))
      {
         buffer[n] = (m * (f - cDitherF1)) + 1.0f;
      }
      
      if (f >= cDitherF2)
      {
         buffer[n] = A;
      }      
      
      // Mirror mask to second half of spectrum
      buffer[len - n - 1] = buffer[n];   
   } 
   
}

void FormatClassifier::FilterPolyphase(float* x, float* y, float* win, size_t p, size_t len)
{
   size_t outlen = len / p;
  
   // Window signal
   for (size_t n = 0; n < len; n++)
   {
      x[n] *= win[n];
   }

   // Zero out output signal
   for (size_t n = 0; n < outlen; n++)
   {
      y[n] = 0.0f;
   }

   // Accumulate sub-windows
   for (size_t n = 0; n < p; n++)
   {
      Add(y, &(x[n * outlen]), outlen);
   }

}

void FormatClassifier::Add(float* in1, float* in2, size_t len)
{
   for (unsigned int n = 0; n < len; n++)
   {
      in1[n] += in2[n];
   }
}

void FormatClassifier::Sub(float* in, float subt, size_t len)
{
   for (unsigned int n = 0; n < len; n++)
   {
      in[n] -= subt;
   }
}

void FormatClassifier::Div(float* in, float div, size_t len)
{
   for (unsigned int n = 0; n < len; n++)
   {
      in[n] /= div;
   }
}


void FormatClassifier::Abs(float* in, float* out, size_t len)
{
   for (unsigned int n = 0; n < len; n++)
   {
      if (in[n] < 0.0f)
      {
         out[n] = -in[n];
      }
      else
      {
         out[n] = in[n];
      }
   }
}

float FormatClassifier::Mean(float* in, size_t len)
{
   float mean = 0.0f;

   for (unsigned int n = 0; n < len; n++)
   {
      mean += in[n];
   }

   mean /= len;
   
   return mean;
}

float FormatClassifier::Max(float* in, size_t len)
{
   size_t dummyidx;
   return Max(in, len, &dummyidx);
}

float FormatClassifier::Max(float* in, size_t len, size_t* maxidx)
{
   float max = -FLT_MAX;
   *maxidx = 0;
   
   for (unsigned int n = 0; n < len; n++)
   {
      if (in[n] > max)
      {
         max = in[n];
         *maxidx = n;
      }
   }

   return max;
}

float FormatClassifier::Rms(float* in, size_t len)
{
   float rms = 0.0f;
   
   for (size_t n = 0; n < len; n++)
   {
      rms += in[n] * in[n];
   }
   
   rms = sqrtf(rms);
   
   return rms;
}

template<class T> void FormatClassifier::ToFloat(T* in, float* out, size_t len)
{
   for(unsigned int n = 0; n < len; n++)
   {
      out[n] = (float) in[n];
   }
}
