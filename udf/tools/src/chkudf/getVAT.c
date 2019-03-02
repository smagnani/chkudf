// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999-2001 Ben Fennema. All rights reserved.

#include "nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * Read the VAT from the medium. 
 * This can be made to work with blocksize != sectorsize, but since
 * it's not allowed by UDF, we won't go to the extra effort.
 */


void GetVAT(void)
{
  struct FileEntry *VATICB;
  bool             found;
  int              result;
  uint32_t         i;
  uint16_t         VirtPart;

  found = false;
  for (i = 0; (i < PTN_no) && !found; i++) {
    if (Part_Info[i].type == PTN_TYP_VIRTUAL) {
      VirtPart = i;
      found = true;
    }
  }

  if (found && (s_per_b == 1)) {
    VATICB = (struct FileEntry *)malloc(blocksize);
    if (VATICB) {
      printf("\n--Partition Reference %u is virtual, finding VAT ICB.\n", VirtPart);
      ReadSectors(VATICB, LastSector, 1);

      result = CheckTag((struct tag *)VATICB, LastSector - Part_Info[VirtPart].Offs,
                        TAGID_FILE_ENTRY, 20, blocksize);
      if (result > CHECKTAG_OK_LIMIT) {
        printf("**No VAT in the last sector.  Trying back 150 sectors.\n");
        ReadSectors(VATICB, LastSector - 150, 1);
        result = CheckTag((struct tag *)VATICB, 
                          LastSector - Part_Info[VirtPart].Offs - 150,
                          TAGID_FILE_ENTRY, 20, blocksize);
      }
      if (result < CHECKTAG_OK_LIMIT) {
        printf("  VAT ICB candidate was found.\n");
        // We have a good ICB
        if (VATICB->sICBTag.FileType == FILE_TYPE_VAT) {
#if 1
          // @todo Replace this with a real implementation when sample media is available
          // "Found VAT ICB. Unfortunately, code to process it does not yet exist."
          Error.Code = ERR_NOVATCODE;
          Error.Sector = U_endian32(VATICB->sTag.uTagLoc);
          Fatal = true;
#else     // Obsolete code for UDF1.50 VAT format. @todo Retain it in case we ever see 1.50 media?
          uint64_t infoLength = U_endian64(VATICB->InfoLength);
          if ((infoLength <= 0x3FFFFFFFFULL) && ((size_t) infoLength) == infoLength) {
            Part_Info[VirtPart].Extra = malloc(infoLength);   // @todo This may not work as expected on 32-bit systems
          }
          if (Part_Info[VirtPart].Extra) {
            printf("  Allocated %" PRIu64 " (0x%" PRIx64 ") bytes for the VAT.\n", infoLength, infoLength);
            // FIXME: short read and read error are not handled
            ReadFileData(Part_Info[VirtPart].Extra, (struct FE_or_EFE*)VATICB, Part_Info[VirtPart].Num,
                         0, infoLength, &i);   // @todo ReadFileData() isn't coded to read > UINT32_MAX a a time
            Part_Info[VirtPart].Len = (uint32_t)((infoLength - 36) >> 2);
            printf("  Virtual partition is %u sectors long.\n", Part_Info[VirtPart].Len);
            printf("%sVAT Identifier is: ", CheckRegid((struct udfEntityId *)(Part_Info[VirtPart].Extra + Part_Info[VirtPart].Len), E_REGID_VAT) ? "**" : "  ");
            DisplayRegIDID((struct regid *)(Part_Info[VirtPart].Extra + Part_Info[VirtPart].Len));
            printf("\n");
            // @todo 50 is arbitrary. Limit this detail to a verbose mode.
            for (i = 0; (i < 50) && (i < Part_Info[VirtPart].Len); i++) {
              printf("%02x: %08x\n", i, Part_Info[VirtPart].Extra[i]);
            }
          } else {
            Error.Code = ERR_NOVATMEM;
            Error.Sector = LastSector - Part_Info[VirtPart].Offs;
            Fatal = true;
          }
#endif
        } else {
          Error.Code = ERR_NOVAT;
          Error.Sector = LastSector - Part_Info[VirtPart].Offs;
          Fatal = true;
        }
      } else {
        Error.Code = ERR_NOVAT;
        Error.Sector = LastSector - Part_Info[VirtPart].Offs;
        Fatal = true;
      }
      free(VATICB);
    } else {
      printf("**Can't malloc memory for VAT ICB.\n");
      Fatal = true;
    }
  } else {
    if (found) {
      Error.Code = ERR_NOVAT;
      Error.Sector = LastSector - Part_Info[VirtPart].Offs;
      Fatal = true;
    }
  }
}
