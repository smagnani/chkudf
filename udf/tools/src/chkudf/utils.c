// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999 Rob Simms. All rights reserved.
// Copyright (c) 2019 Digital Design Corporation. All rights reserved.

#include "nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/* some default values */

#define SPACECHAR "."
#define TITLELEN 78
#define MAXCOLUMNS 8

uint64_t endian64(uint64_t toswap)
{
  size_t i;
  uint64_t swappedValue = 0;

  uint8_t* pDestByte   = ((uint8_t*)(&swappedValue)) + sizeof(uint64_t) - 1;
  uint8_t* pSourceByte = (uint8_t*)(&toswap);

  for (i=0; i<sizeof(uint64_t); ++i)
  {
    *pDestByte = *pSourceByte;
    ++pSourceByte;
    --pDestByte;
  }

  return swappedValue;
}

uint32_t endian32(uint32_t toswap)
{
  return (toswap << 24) | 
         ((toswap << 8) & 0x00ff0000) |
         ((toswap >> 8) & 0x0000ff00) |
         (toswap >> 24);
}

uint16_t endian16(uint16_t toswap)
{
  return (toswap << 8) | (toswap >> 8);
}


#define MASK 0x1021

uint16_t doCRC(uint8_t *buffer, int n)
{
  uint16_t CRC = 0;
  int byte, bit;
  uint8_t bitval, msb;

  if (n > 4080) {
    CRC = 0xffff;
  } else {
    for (byte = 0; byte < n; byte++) {
      for (bit=7; bit>=0; bit--) {
        bitval = (*(buffer + byte) >> bit) & 0x01;
        msb = CRC >> 15;
        CRC <<=  1;
        CRC ^= MASK * (bitval ^ msb);
      }
    }
  }
  return CRC;
}

/*********************************************************************/
/* prints a Dstring field -- no guaranteed trailing null, and length */
/* in last byte.                                                     */
/* Supports OSTA Compressed Unicode, but if no Compression algorithm */
/* supplied, will print out ASCII string.                            */
/*********************************************************************/
void printDstring(const uint8_t *start, uint8_t fieldLen)
{
    /* First, grab the length of the string */
    uint8_t dstringLen = start[fieldLen - 1];

    /* Then, hand it all off to Dchars */
    printDchars(start, dstringLen);
    printf("\n");
    return;
}

/*********************************************************************/
/* prints a Dchars field -- no guaranteed trailing null, and length  */
/* of Dchars field is max length of string.                          */
/* Supports OSTA Compressed Unicode, but if no Compression algorithm */
/* supplied, will print out ASCII string.                            */
/*********************************************************************/
void printDchars(const uint8_t *start, uint8_t length)
{
  /* Some (one) local variable(s) */
  uint16_t i;                       /* Index  */
  uint16_t unichar;                 /* Unicode character */

  char tbuff[257];                /* Buffer for non-unicode */

  uint8_t dispLen = length;

  /* First, grab the Compressed Algorithm Number. */
  uint8_t alg = start[0];

  /* Throw out the algorithm byte, if any, and print out compression
     algorithm */
  switch (alg) {
    case 16:
      /* 16-bit Unicode, but ignore the first byte */
      start++;
      dispLen--;
      break;
    case 8:
      /* ASCII, but ignore the first byte */
      start++;
      dispLen--;
      break;
    default:
      /* ASCII, including the first byte */
      break;
  }

  /* Print out the characters. */
  if ( alg == 16 ) {
    printf("\"");
    for (i=0;i<dispLen;i++,i++) {
      unichar = *(start + i) << 8;
      unichar |= *(start + i + 1);
      if ((unichar > 31) && (unichar < 127)) {
        printf("%c", (uint8_t)unichar);
      } else {
        printf("[%4x]", unichar);
      }
    }
    printf("\"");
  } else {
    /* Now make a copy of all the bytes, to make sure we have a null
       termination */
    strncpy( tbuff, (const char*) start, dispLen); /* copy over just enough characters */
    tbuff[dispLen]=0;           /* null terminate the string */
    printf("'%s'", tbuff);    /* print it out */
  }
  return;
}

void printExtentAD(struct extent_ad extent)
{
  printf("%u [0x%08x] @ %u [0x%08x]\n",
         U_endian32(extent.Length), U_endian32(extent.Length),
         U_endian32(extent.Location), U_endian32(extent.Location));
}

void printCharSpec(struct charspec chars)
{
  int i;

  printf("[%u] ", (int)chars.uCharSetType);
  for (i = 0; i < 63; i++) {
    if (chars.aCharSetInfo[i]) {
      printf("%c", chars.aCharSetInfo[i]);
    } else {
      i = 64;
    }
  }
  printf("\n");
}

int Is_Charspec(const struct charspec *chars)
{
  size_t i = 0;
  const uint8_t ref[] = UDF_CHARSPEC;

  if (chars->uCharSetType)
    return 0;
  i = 0;
  while (i < 63) {
    if (i < (sizeof(UDF_CHARSPEC)-1)) {
      if (ref[i] != chars->aCharSetInfo[i])
        return 0;
    } else {
      if (chars->aCharSetInfo[i])
        return 0;
    }
    i++;
  }
  return 1;
}


/********************************************************************/
/* Display an ISO/IEC 13346 timestamp structure                     */
/********************************************************************/
void printTimestamp( struct timestamp Time)
{
  /* assumes character is positioned, ends with newline */

  int tp = GetTSTP(U_endian16(Time.uTypeAndTimeZone));
  int tz = GetTSTZ(U_endian16(Time.uTypeAndTimeZone));

  printf("%4.4u/%2.2u/%2.2u ",
         U_endian16(Time.iYear), Time.uMonth, Time.uDay);
  printf("%2.2u:%2.2u:%2.2u.",
         Time.uHour, Time.uMinute, Time.uSecond);
  printf("%2.2u%2.2u%2.2u",
         Time.uCentiseconds,Time.uHundredMicroseconds,Time.uMicroseconds);
  printf(" (%s)",
         (tp == 0 ? "UTC" :
          (tp == 1 ? "Local" :
           "Non-ISO")));
  printf(", %d %s\n",
         tz,(tz == -2047 ? "(No timezone specified)" :
             (tz <= 1440 && tz >= -1440 ? "min. from UTC" :
              "(***INVALID timezone value***)")));
}

/********************************************************************/
/* Display a long_ad structure                                      */
/********************************************************************/
void printLongAd(struct long_ad *longad)
{
  printf("%u bytes @ %u:%u\n", EXTENT_LENGTH(longad->ExtentLengthAndType),
         U_endian16(longad->Location_PartNo), U_endian32(longad->Location_LBN));
}

unsigned int countSetBits(unsigned int value)
{
  unsigned int numSetBits = 0;

  while (value) {
    if (value & 1) {
      ++numSetBits;
    }

    value >>= 1;
  }

  return numSetBits;
}

// As of 2019-02-27
bool IsKnownUDFVersion(uint16_t bcdVersion)
{
  bool bKnown;
  switch (bcdVersion) {
    case 0x100:
    case 0x101:
    case 0x102:
    case 0x150:
    case 0x200:
    case 0x201:
    case 0x250:
    case 0x260:
      bKnown = true;
      break;

    default:
      bKnown = false;
      break;
  }

  return bKnown;
}
