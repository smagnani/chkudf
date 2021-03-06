// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999 Rob Simms. All rights reserved.

#include "nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

void VerifyAVDP(void)
{
  int avdp_count = 0;
  int front_avdp[] = {256, 512, -1};
  int back_avdp[] = {0, 150, 256, 406, -1};
  int i, result;
  struct AnchorVolDesPtr *AVDPtr;

  AVDPtr = (struct AnchorVolDesPtr *)malloc(secsize);
  if (AVDPtr) {
    Information("\n--Verifying the Anchor Volume Descriptor Pointers.\n");
    
    for (i = 0; front_avdp[i] != -1; i++) {
      Verbose("  Checking %u: ", lastSessionStartLBA + front_avdp[i]);
      result = ReadSectors(AVDPtr, lastSessionStartLBA + front_avdp[i], 1);
      if (!result) {
        result = CheckTag((struct tag *)AVDPtr, lastSessionStartLBA + front_avdp[i], TAGID_ANCHOR, 
                          16, 496);
        if (result < CHECKTAG_OK_LIMIT) {
          Verbose("AVDP present.\n");
          DumpError();
          track_volspace(lastSessionStartLBA + front_avdp[i], 1, "Front AVDP");
          if (!avdp_count) {
            VDS_Loc = U_endian32(AVDPtr->sMainVDSAdr.Location);
            VDS_Len = U_endian32(AVDPtr->sMainVDSAdr.Length);
            RVDS_Loc = U_endian32(AVDPtr->sReserveVDSAdr.Location);
            RVDS_Len = U_endian32(AVDPtr->sReserveVDSAdr.Length);
            track_volspace(VDS_Loc, VDS_Len >> sdivshift, "Main VDS (Front AVDP)");
            track_volspace(RVDS_Loc, RVDS_Len >> sdivshift, "Reserve VDS (Front AVDP)");
          } else {
            if (VDS_Loc != U_endian32(AVDPtr->sMainVDSAdr.Location) ||
                VDS_Len != U_endian32(AVDPtr->sMainVDSAdr.Length) ||
                RVDS_Loc != U_endian32(AVDPtr->sReserveVDSAdr.Location) ||
                RVDS_Len != U_endian32(AVDPtr->sReserveVDSAdr.Length)) {
              Error.Code = ERR_VDS_NOT_EQUIVALENT;
              Error.Sector = lastSessionStartLBA + front_avdp[i];
              DumpError();
            }
          } /* If first AVDP */
          avdp_count++;
        } else {
          Verbose("No AVDP.\n");
          ClearError();
        } /* If is AVDP */
      } else {
        OperationalError("read error.\n");
      }
    }

    /* Check the end referenced AVDPs */
    for (i = 0; back_avdp[i] != -1; i++) {
      Verbose("  Checking %u (n - %d): ", LastSector - back_avdp[i], back_avdp[i]);
      result = ReadSectors(AVDPtr, LastSector - back_avdp[i], 1);
      if (!result) {
        result = CheckTag((struct tag *)AVDPtr, LastSector - back_avdp[i], 
                          TAGID_ANCHOR, 16, 496);
        if (result < CHECKTAG_OK_LIMIT) {
          Verbose("AVDP present.\n");
          DumpError();
          track_volspace(LastSector - back_avdp[i], 1, "Back AVDP");
          if (!avdp_count) {
            VDS_Loc = U_endian32(AVDPtr->sMainVDSAdr.Location);
            VDS_Len = U_endian32(AVDPtr->sMainVDSAdr.Length);
            RVDS_Loc = U_endian32(AVDPtr->sReserveVDSAdr.Location);
            RVDS_Len = U_endian32(AVDPtr->sReserveVDSAdr.Length);
            track_volspace(VDS_Loc, VDS_Len >> sdivshift, "Main VDS (Back AVDP)");
            track_volspace(RVDS_Loc, RVDS_Len >> sdivshift, "Reserve VDS (Back AVDP)");
          } else {
            if (VDS_Loc != U_endian32(AVDPtr->sMainVDSAdr.Location) ||
                VDS_Len != U_endian32(AVDPtr->sMainVDSAdr.Length) ||
                RVDS_Loc != U_endian32(AVDPtr->sReserveVDSAdr.Location) ||
                RVDS_Len != U_endian32(AVDPtr->sReserveVDSAdr.Length)) {
              Error.Code = ERR_VDS_NOT_EQUIVALENT;
              Error.Sector = LastSector - back_avdp[i];
              DumpError();
            }
          } /* If first AVDP */
          avdp_count++;
        } else {
          Verbose("No AVDP.\n");
          ClearError();
        } /* If is AVDP */
      } else {
        OperationalError("read error.\n");
      }
    }

    /* Check the N/59 referenced AVDPs */
//    for (i = 1; i < 59; i++) {
//      result = ReadSectors(AVDPtr, LastSector / 59 * i, 1);
//      if (!result) {
//        CheckTag((struct tag *)AVDPtr, LastSector / 59 * i, TAGID_ANCHOR, 16, 496);
//        if (!Error.Code) {
//          track_volspace(LastSector / 59 * i, 1);
//          if (!avdp_count) {
//            *VDS_Loc = AVDPtr->sMainVDSAdr.Location;
//            *VDS_Len = AVDPtr->sMainVDSAdr.Length;
//            *RVDS_Loc = AVDPtr->sReserveVDSAdr.Location;
//            *RVDS_Len = AVDPtr->sReserveVDSAdr.Length;
//            track_volspace(*VDS_Loc, *VDS_Len >> sdivshift);
//            track_volspace(*RVDS_Loc, *RVDS_Len >> sdivshift);
//          } else {
//            if (*VDS_Loc != AVDPtr->sMainVDSAdr.Location ||
//                *VDS_Len != AVDPtr->sMainVDSAdr.Length ||
//                *RVDS_Loc != AVDPtr->sReserveVDSAdr.Location ||
//                *RVDS_Len != AVDPtr->sReserveVDSAdr.Length) {
//              error = ERR_AVDP_NOT_EQUIVALENT;
//            }
//          } /* If first AVDP */
//          avdp_count++;
//        } else {
//          error = 0;
//        } /* If is AVDP */
//      } else {
//        printf("ReadSectors(%u, 1) failed.\n", LastSector / 59 * i);
//      }  /* If sector is read */
//    }
    if (avdp_count) {
      // @todo We could be a little more tolerant here
      UDFErrorIf(avdp_count == 1,
                 "Found %d Anchor Volume Descriptor Pointer%s.\n",
                 avdp_count,
                 avdp_count == 1 ? "" : "s");
      UDFErrorIf(VDS_Len < (16 << bdivshift),
                 "Main Volume Descriptor Sequence is at %u, %u bytes long.\n",
                 VDS_Loc, VDS_Len);
      UDFErrorIf(RVDS_Len < (16 << bdivshift),
                 "Reserve Volume Descriptor Sequence is at %u, %u bytes long.\n",
                 RVDS_Loc, RVDS_Len);
      if (!VDS_Len && !RVDS_Len) {
        UDFError("**Both Volume Descriptor Sequences have zero length.\n");
        Fatal = true;
      }
    } else {
      UDFError("**No Anchor Volume Descriptor Pointers found.\n");
      Error.Code = ERR_NOAVDP;
      Error.Sector = LastSector;
      Fatal = true;
    }
    free(AVDPtr);
  } else {
    OperationalError("**Couldn't allocate memory for reading AVDP.\n");
    Fatal = true;
  }
}
