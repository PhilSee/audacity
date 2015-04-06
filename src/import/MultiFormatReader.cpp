/**********************************************************************

  Audacity: A Digital Audio Editor

  MultiFormatReader.cpp

  Philipp Sibler

******************************************************************//**

\class MultiFormatReader
\brief MultiFormatReader reads raw audio files in different formats and 
machine endianness representations.

*//*******************************************************************/

#include <exception>
#include <stdexcept>
#include <cstring>
#include <stdint.h>
#include <cstdio>

#include <wx/defs.h>

#include "MultiFormatReader.h"

MultiFormatReader::MultiFormatReader(const char* filename)
   : mpFid(NULL)
{
   mpFid = fopen(filename, "rb");
      
   if (mpFid == NULL)
   {
      throw std::runtime_error("Error opening file");
   }
}

MultiFormatReader::~MultiFormatReader()
{
   if (mpFid != NULL)
   {
      fclose(mpFid);
   }
}

void MultiFormatReader::Reset()
{
   if (mpFid != NULL)
   {
      rewind(mpFid);
   }
}

void MultiFormatReader::Reset(size_t startoffset)
{
   if (mpFid != NULL)
   {
      fseek (mpFid, startoffset, SEEK_SET);
   }
}

size_t MultiFormatReader::ReadSamples(void* buffer, size_t len,
                    MultiFormatReader::FormatT format,
                    MachineEndianness::EndiannessT end)
{
   return ReadSamples(buffer, len, 1, format, end);
}

   
size_t MultiFormatReader::ReadSamples(void* buffer, size_t len, size_t stride,
                    MultiFormatReader::FormatT format,
                    MachineEndianness::EndiannessT end)
{
   bool swapflag = (mEnd.Which() != end);
   size_t actRead;
   
   switch(format)
   {
      case Int8:
      case Uint8:
         actRead = Read(buffer, 1, len, stride);
         break;
      case Int16:
      case Uint16:
         actRead = Read(buffer, 2, len, stride);
         if(swapflag) SwapBytes(buffer, 2, len);
         break;
      case Int32:
      case Uint32:
      case Float:
         actRead = Read(buffer, 4, len, stride);
         if(swapflag) SwapBytes(buffer, 4, len);
         break;
      case Double:
         actRead = Read(buffer, 8, len, stride);
         if(swapflag) SwapBytes(buffer, 8, len);
         break;
      default:
         break;
   }

   return actRead;
}

size_t MultiFormatReader::Read(void* buffer, size_t size, size_t len, size_t stride)
{
   size_t actRead = 0;
   uint8_t* pWork = (uint8_t*) buffer;
   
   if (stride > 1)
   {
      // There are gaps between consecutive samples,
      // so do a scattered read
      for (size_t n = 0; n < len; n++)
      {
         actRead += fread(&(pWork[n*size]), size, 1, mpFid);
         fseek(mpFid, (stride - 1) * size, SEEK_CUR);
      }
   }
   else
   {
      // Just do a linear read
      actRead = fread(buffer, size, len, mpFid);
   }

   return actRead;
}

   
void MultiFormatReader::SwapBytes(void* buffer, size_t size, size_t len)
{
   uint8_t* pResBuffer = (uint8_t*) buffer;
   uint8_t* pCurBuffer;
   
   if (size > 8)
   {
      throw std::runtime_error("SwapBytes Exception: Format width exceeding 8 bytes.");
   }
   
   for (size_t i = 0; i < len; i++)
   {
      pCurBuffer = &(pResBuffer[i*size]);
      memcpy(mSwapBuffer, &(pCurBuffer[0]), size);
      
      for (size_t n = 0; n < size; n++)
      {
         pCurBuffer[n] = mSwapBuffer[size - n - 1];   
      }
   }
}

