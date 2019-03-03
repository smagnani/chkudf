// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999 Rob Simms. All rights reserved.

#include "nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

int CheckTag(const struct tag *TagPtr, uint32_t uTagLoc, uint16_t TagID,
             int crc_min, int crc_max)
{
  uint8_t checksum;
  int i, result = CHECKTAG_TAG_GOOD;
  uint16_t CRC;

  checksum = 0;
  for (i=0; i<4; i++) checksum += *((uint8_t *)TagPtr + i);
  for (i=5; i<16; i++) checksum += *((uint8_t *)TagPtr + i);
  if (TagPtr->uTagChecksum != checksum) {
    Error.Code = ERR_TAGCHECKSUM;
    Error.Sector = uTagLoc;
    Error.Expected = checksum;
    Error.Found = TagPtr->uTagChecksum;
    result = CHECKTAG_NOT_TAG;
  }

  if (!Error.Code) {
    if ((TagID != (uint16_t)-1) && (TagID != U_endian16(TagPtr->uTagID))) {
      Error.Code = ERR_TAGID;
      Error.Sector = uTagLoc;
      Error.Expected = TagID;
      Error.Found = U_endian16(TagPtr->uTagID);
      result = CHECKTAG_WRONG_TAG;
    }
  }

  if (!Error.Code) {
    if ((U_endian16(TagPtr->uCRCLen) >= crc_min) && (U_endian16(TagPtr->uCRCLen) <= crc_max)) {
      CRC = doCRC((uint8_t *)TagPtr + 16, U_endian16(TagPtr->uCRCLen));
      if (CRC != U_endian16(TagPtr->uDescriptorCRC)) {
        Error.Code = ERR_TAGCRC;
        Error.Sector = uTagLoc;
        Error.Expected = CRC;
        Error.Found = U_endian16(TagPtr->uDescriptorCRC);
        result = CHECKTAG_NOT_TAG;
      }
    } else {
      Error.Code = ERR_CRC_LENGTH;
      Error.Sector = uTagLoc;
      Error.Expected = crc_min;
      Error.Found = U_endian16(TagPtr->uCRCLen);
      result = CHECKTAG_TAG_DAMAGED;
    }
  }
 
  if (!Error.Code) {
    if (uTagLoc != U_endian32(TagPtr->uTagLoc)) {
      Error.Code = ERR_TAGLOC;
      Error.Sector = uTagLoc;
      Error.Expected = uTagLoc;
      Error.Found = U_endian32(TagPtr->uTagLoc);
      result = CHECKTAG_TAG_DAMAGED;
    }
  }

  if (!Error.Code) {
    uint16_t descriptorVersion = U_endian16(TagPtr->uDescriptorVersion);
    if (Version_OK && (descriptorVersion != UDF_Version)) {
      /*
       * ECMA-167r3 sec. 3/7.2.2 Descriptor Version
       *   "...This value shall be 2 or 3...
       *    Note 2:
       *    Structures with version 2 descriptors may be on the medium due to
       *    changing the medium from NSR02 to NSR03 without rewriting all
       *    descriptors as version 3. Originating systems shall record a 3 in
       *    this field; receiving systems shall allow a 2 or 3."
       *
       * Extended File Entry descriptors did not exist in ECMA-167r2.
       * Don't treat this as an error though because the above ECMA-167r3
       * clause implies that OS UDF drivers won't care.
       */
      if (!((UDF_Version == 3) && (descriptorVersion == 2))) {
        Version_OK = false;
        Error.Code = ERR_NSR_VERSION;
        Error.Sector = uTagLoc;
        Error.Expected = UDF_Version;
        Error.Found = U_endian16(TagPtr->uDescriptorVersion);
        result = CHECKTAG_TAG_DAMAGED;
      }
    }
  }

  if (!Error.Code) {
    if (Serial_OK && (U_endian16(TagPtr->uTagSerialNum != Serial_No))) {
      Version_OK = false;
      Error.Code = ERR_SERIAL;
      Error.Sector = uTagLoc;
      Error.Expected = Serial_No;
      Error.Found = U_endian16(TagPtr->uTagSerialNum);
      result = CHECKTAG_TAG_DAMAGED;
    }
  }

  return result;
}
