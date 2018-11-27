#include "../nsrHdrs/nsr.h"
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
    unsigned long long infoLength =   (((unsigned long long) U_endian32(xfe->InfoLengthH)) << 32)
                                    | U_endian32(xfe->InfoLengthL);
    if (!CheckTag((struct tag *)xfe, U_endian32(FE.Location_LBN), TAGID_FILE_ENTRY, 16, blocksize)) {
      printf("(%llu) ", infoLength);
    } else {
      ClearError();
      if (!CheckTag((struct tag *)xfe, U_endian32(FE.Location_LBN), TAGID_EXT_FILE_ENTRY, 16, blocksize)) {
        printf("(%llu) ", infoLength);
      }
    }

    if (dir && xfe->sICBTag.FileType != FILE_TYPE_DIRECTORY) {
       printf("[Type: %d] ", xfe->sICBTag.FileType);
    }

    if (!dir && xfe->sICBTag.FileType != FILE_TYPE_RAW) {
       printf("[Type: %d] ", xfe->sICBTag.FileType);
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

