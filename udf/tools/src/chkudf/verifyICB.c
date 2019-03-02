// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999-2001 Ben Fennema. All rights reserved.
// Copyright (c) 2019 Digital Design Corporation. All rights reserved.

#include "nsr.h"
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/*
 *  Read a File Entry and extract the basics.
 */

int checkICB(struct FE_or_EFE *xfe, struct long_ad FE, int dir)
{
  if (xfe) {
    uint64_t infoLength = U_endian64(xfe->InfoLength);
    if (!CheckTag((struct tag *)xfe, U_endian32(FE.Location_LBN), TAGID_FILE_ENTRY, 16, blocksize)) {
      printf("(%" PRIu64 ") ", infoLength);
    } else {
      ClearError();
      if (!CheckTag((struct tag *)xfe, U_endian32(FE.Location_LBN), TAGID_EXT_FILE_ENTRY, 16, blocksize)) {
        printf("(%" PRIu64 ") ", infoLength);
      }
    }

    if (dir && xfe->sICBTag.FileType != FILE_TYPE_DIRECTORY) {
      printf("[Type: %u] ", xfe->sICBTag.FileType);
    }

    if (!dir && xfe->sICBTag.FileType != FILE_TYPE_RAW) {
      printf("[Type: %u] ", xfe->sICBTag.FileType);
    }
  } else {
    Error.Code = ERR_READ;
    Error.Sector = U_endian32(FE.Location_LBN);
  }

/* Verify that the information length is consistent with the descriptors.
   Verify the recorded sectors field. */

  DumpError();
  return 0;
}

