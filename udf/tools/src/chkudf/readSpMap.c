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

int ReadSpaceMap(void)
{
  struct SpaceBitmapHdr *BMD;
  int i;


  for (i = 0; i < PTN_no; i++) {
    if (Part_Info[i].Space != -1) {
      printf("\n--Reading the Space Bitmap Descriptor for partition reference %d.\n", i);
      printf("  Descriptor is %d sectors at %d:%d.\n", 
             Part_Info[i].SpLen >> bdivshift, i, Part_Info[i].Space);
      BMD = (struct SpaceBitmapHdr *)malloc(Part_Info[i].SpLen);
      if (BMD) {
        ReadLBlocks(BMD, Part_Info[i].Space, i, Part_Info[i].SpLen >> bdivshift);
        track_filespace(i, Part_Info[i].Space, Part_Info[i].SpLen);
  
        CheckTag((struct tag *)BMD, Part_Info[i].Space, TAGID_SPACE_BMAP, 
                     0, Part_Info[i].SpLen);
        if (Error.Code == ERR_TAGID) {
          printf("**Not a space bitmap descriptor.\n");
        } else {
          DumpError();
        }
        if (!Error.Code) {
          unsigned int mapBytesRequired = BITMAP_NUM_BYTES(Part_Info[i].Len);
          unsigned int mapBytesRecorded = U_endian32(BMD->N_Bytes);
          printf("  Partition is %d blocks long, requiring %u bytes.\n",
                 Part_Info[i].Len, mapBytesRequired);
          if (U_endian32(BMD->N_Bits) != Part_Info[i].Len) {
            printf("**Partition is %d blocks long and is described by %d bits.\n",
                   Part_Info[i].Len, U_endian32(BMD->N_Bits));
          }
          if (BITMAP_NUM_BYTES(U_endian32(BMD->N_Bits)) != mapBytesRecorded) {
            printf("**Bitmap descriptor requires %d bytes to hold %d bits.\n",
                   mapBytesRecorded, U_endian32(BMD->N_Bits));
          }
          if (Part_Info[i].SpMap && (mapBytesRecorded < Part_Info[i].SpLen)) {
            memcpy(Part_Info[i].SpMap,
                   (UINT8 *)BMD + sizeof(struct SpaceBitmapHdr),
                    MIN(mapBytesRecorded, mapBytesRequired));

            // Mask out bits for blocks beyond end of partition
            Part_Info[i].SpMap[mapBytesRequired-1] &= Part_Info[i].FinalMapByteMask;

            printf("  Read the space bitmap for partition reference %d.\n", i);
          }
        } else {
          DumpError();
        }
        free(BMD);
      } else {
        printf("**Couldn't allocate memory for space bitmap.\n");  // if (BMD)
      }
    }
  }

  return 0;
}

