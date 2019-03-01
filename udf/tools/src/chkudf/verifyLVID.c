#include "../nsrHdrs/nsr.h"
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * The following routine verifies the Logical Volume Integrity Descriptor
 * sequence.
 */
int verifyLVID(uint32_t loc, uint32_t len)
{
  uint32_t                           i, j;
  uint32_t                          *Table;
  uint8_t                           *buffer;
  struct LogicalVolumeIntegrityDesc *LVID;
  struct LVIDImplUse                *LVIDIU;

  printf("  Verifying the Logical Volume Integrity Descriptor Sequence.\n");
  buffer = malloc(secsize);
  if (buffer) {
    LVID = (struct LogicalVolumeIntegrityDesc *)buffer;
    for (i = 0; i < (len >> sdivshift); i++) {
      /*
       * Get a sector and test it.
       */
      ReadSectors(buffer, loc + i, 1);
      CheckTag((struct tag *)buffer, loc + i, TAGID_LVID, 0, secsize - 16);
      if (!Error.Code) {
        printf("  LVID at %08x: recorded at ", loc + i);
        printTimestamp(LVID->sRecordingTime);
        switch (U_endian32(LVID->integrityType)) {
          case 0:
            printf(" [Open]\n");
            break;
          case 1:
            printf(" [Close]\n");
            break;
          default:
            printf("** Illegal! (%u)\n", U_endian32(LVID->integrityType));
            break;
        }
        ID_UID = U_endian64(LVID->UniqueId);
        LVIDIU = (struct LVIDImplUse *)(buffer + 80 + U_endian32(LVID->N_P) * 8);

        bool bProcessIU = true;
        if (U_endian32(LVID->L_IU) != 46) {
          if (   IsKnownUDFVersion(U_endian16(LVIDIU->MaxUDFWrite))
              && IsKnownUDFVersion(U_endian16(LVIDIU->MinUDFRead))) {
            printf("**Length of Implementation use is %u (expected 46).\n",
                   U_endian32(LVID->L_IU));
            printf("  Continuing since data appears valid.\n");
          } else {
            printf("**Length of Implementation use is %u (expected 46).\n",
                   U_endian32(LVID->L_IU));
            bProcessIU = false;
          }
        }
        if (bProcessIU) {
          ID_Files = U_endian32(LVIDIU->numFiles);
          ID_Dirs = U_endian32(LVIDIU->numDirectories);
          printf("  %u directories, %u files, next UniqueID is %" PRIu64 ".\n",
                 ID_Dirs, ID_Files, ID_UID);
          printf("  Min read ver. %x, min write ver. %x, max write ver %x.\n",
                 U_endian16(LVIDIU->MinUDFRead), U_endian16(LVIDIU->MinUDFWrite),
                 U_endian16(LVIDIU->MaxUDFWrite));
          if (U_endian16(LVIDIU->MinUDFRead) > 0x201) {
            printf("**Cannot reliably analyze media that has min read ver > 201\n");
          }
          printf("  Recorded by: ");
          DisplayImplID(&(LVIDIU->implementationID));
        } else {
          printf("  Next unique ID is %" PRIu64 "\n", ID_UID);
        }
        Table = (uint32_t *)(buffer + 80);
        for (j = 0; j < U_endian32(LVID->N_P); j++) {
          printf("  Partition reference %u has %u of %u blocks available.\n",
                 j, U_endian32(Table[j]), U_endian32(Table[j + U_endian32(LVID->N_P)]));
        }
        if (U_endian32(LVID->nextIntegrityExtent.Length)) {
          len = U_endian32(LVID->nextIntegrityExtent.Length);
          loc = U_endian32(LVID->nextIntegrityExtent.Location);
          i = -1;
          printf("  Next extent is %u bytes at %u.\n", len, loc);
          track_volspace(loc, len >> sdivshift, "Integrity Sequence Extension");
        }
      } else {
        i = len;
        printf("  End of LVID sequence.\n");
        ClearError();
      }
    }  //run through extent  
    free(buffer);
  } else {
    Error.Code = ERR_NO_VD_MEM;
  }
  return Error.Code;
}
   
