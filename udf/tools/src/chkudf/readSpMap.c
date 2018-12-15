#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

static int ReadSpaceBitmap(UINT16 ptn)
{
  struct SpaceBitmapHdr *BMD;

  if (Part_Info[ptn].Space != -1) {
    printf("\n--Reading the Space Bitmap Descriptor for partition reference %d.\n", ptn);
    printf("  Descriptor is %d sectors at %d:%d.\n",
           Part_Info[ptn].SpLen >> bdivshift, ptn, Part_Info[ptn].Space);
    BMD = (struct SpaceBitmapHdr *)malloc(Part_Info[ptn].SpLen);
    if (BMD) {
      ReadLBlocks(BMD, Part_Info[ptn].Space, ptn, Part_Info[ptn].SpLen >> bdivshift);
      track_filespace(ptn, Part_Info[ptn].Space, Part_Info[ptn].SpLen);

      CheckTag((struct tag *)BMD, Part_Info[ptn].Space, TAGID_SPACE_BMAP,
                   0, Part_Info[ptn].SpLen);
      if (Error.Code == ERR_TAGID) {
        printf("**Not a space bitmap descriptor.\n");
      } else {
        DumpError();
      }
      if (!Error.Code) {
        unsigned int mapBytesRequired = BITMAP_NUM_BYTES(Part_Info[ptn].Len);
        unsigned int mapBytesRecorded = U_endian32(BMD->N_Bytes);
        printf("  Partition is %d blocks long, requiring %u bytes.\n",
               Part_Info[ptn].Len, mapBytesRequired);
        if (U_endian32(BMD->N_Bits) != Part_Info[ptn].Len) {
          printf("**Partition is %d blocks long and is described by %d bits.\n",
                 Part_Info[ptn].Len, U_endian32(BMD->N_Bits));
        }
        if (BITMAP_NUM_BYTES(U_endian32(BMD->N_Bits)) != mapBytesRecorded) {
          printf("**Bitmap descriptor requires %d bytes to hold %d bits.\n",
                 mapBytesRecorded, U_endian32(BMD->N_Bits));
        }
        if (Part_Info[ptn].SpMap && (mapBytesRecorded < Part_Info[ptn].SpLen)) {
          memcpy(Part_Info[ptn].SpMap,
                 (UINT8 *)BMD + sizeof(struct SpaceBitmapHdr),
                 MIN(mapBytesRecorded, mapBytesRequired));

          // Mask out bits for blocks beyond end of partition
          Part_Info[ptn].SpMap[mapBytesRequired-1] &= Part_Info[ptn].FinalMapByteMask;

          printf("  Read the space bitmap for partition reference %d.\n", ptn);
        }
      } else {
        DumpError();
      }
      free(BMD);
    } else {
      printf("**Couldn't allocate memory for space bitmap.\n");  // if (BMD)
    }
  }

  return 0;
}

int ReadSpaceMap(void)
{
  int i;

  for (i = 0; i < PTN_no; i++) {
      ReadSpaceBitmap(i);
  }

  return 0;
}

