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

int checkICB(struct FileEntry *fe, struct long_ad FE, int dir)
{
  if (fe) {
    if (!CheckTag((struct tag *)fe, U_endian32(FE.Location_LBN), TAGID_FILE_ENTRY, 16, blocksize)) {
      unsigned long long infoLength =   (((unsigned long long) U_endian32(fe->InfoLengthH)) << 32)
                                      | U_endian32(fe->InfoLengthL);

      printf("(%ull) ", infoLength);
    }

    if (dir && fe->sICBTag.FileType != FILE_TYPE_DIRECTORY) {
       printf("[Type: %d] ", fe->sICBTag.FileType);
    }

    if (!dir && fe->sICBTag.FileType != FILE_TYPE_RAW) {
       printf("[Type: %d] ", fe->sICBTag.FileType);
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

