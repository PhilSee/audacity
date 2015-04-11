/**********************************************************************

  Audacity: A Digital Audio Editor

  FormatClassifier.h

  Philipp Sibler

**********************************************************************/

#ifndef __AUDACITY_FORMATCLASSIFIER_H_
#define __AUDACITY_FORMATCLASSIFIER_H_

#ifndef SNDFILE_1
#error Requires libsndfile 1.0.3 or higher
#endif

// #define FORMATCLASSIFIER_SIGNAL_DEBUG 1

#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG

#include <cstdio>

class DebugWriter
{
   FILE* mpFid;

public:
   DebugWriter(const char* filename)
   {
      mpFid = fopen(filename, "wb");
   }

   ~DebugWriter()
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

class FormatClassifier
{
public:

   typedef struct
   {
      MultiFormatReader::FormatT format;
      MachineEndianness::EndiannessT endian;
   } FormatClassT;

   typedef std::vector<FormatClassT> FormatVectorT;
   typedef std::vector<FormatClassT>::iterator FormatVectorIt;

private:

   const size_t  cSiglen = 1024;
   const size_t  cRawSiglen = 8 * cSiglen;
   const size_t  cPolyTaps = 4;
   const size_t  cNumInts = 32;
   // Normalized dither set on frequency
   const float   cDitherF1 = 0.31f;
   // Normalized dither level frequency
   const float   cDitherF2 = 0.42f;
   // Dither equalizer attenuation [dB]
   const float   cDitherA  = 12.0f;
   // Minimum RMS value of a signal window to be treated as a signal
   const float   cMinRms = 1e-12;
   // Number of windows to skip between signal search evaluations
   const size_t  cSigSearchGridSize = 32;
   // Number of bytes to skip file header
   const size_t  cHeaderSkip = 1024;
      
   
   size_t               mFiltSiglen;

   FormatVectorT        mClasses;
   MultiFormatReader    mReader;
   SpecPowerMeter       mMeter;

#ifdef FORMATCLASSIFIER_SIGNAL_DEBUG
   DebugWriter*         mpWriter;
#endif

   std::vector<float>   mSigBuffer;
   std::vector<float>   mAuxBuffer;
   std::vector<float>   mWinBuffer;
   std::vector<float>   mEqBuffer;
   std::vector<uint8_t> mRawBuffer;
   
   size_t               mSignalStart;
   
   std::vector<float>   mPLo;
   std::vector<float>   mPHiM;
   std::vector<float>   mPHiS;
   
   float*               mMonoFeat;
   float*               mStereoFeat;
   
   FormatClassT         mResultFormat;
   int                  mResultChannels;
   
public:
   FormatClassifier(const char* filename);
   ~FormatClassifier();

   FormatClassT GetResultFormat();
   int GetResultFormatLibSndfile();
   int GetResultChannels();
private:
   void Run();
   void ReadSignal(FormatClassT format, size_t stride);
   void FindSignalStart();
   void ConvertSamples(void* in, float* out, FormatClassT format);
   
   void CalcSincwin(float* buffer, size_t len);
   void CalcEqualizerMask(float* buffer, size_t len);
   void FilterPolyphase(float* x, float* y, float* win, size_t p, size_t len);

   void Add(float* in1, float* in2, size_t len);
   void Sub(float* in, float subt, size_t len);
   void Div(float* in, float div, size_t len);
   void Abs(float* in, float* out, size_t len);
   float Mean(float* in, size_t len);
   float Max(float* in, size_t len);
   float Max(float* in, size_t len, size_t* maxidx);
   float Rms(float* in, size_t len);

   template<class T> void ToFloat(T* in, float* out, size_t len);
};

#endif
