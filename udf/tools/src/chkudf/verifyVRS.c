#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/*
 * The VRS is not at a fixed sector number.  The following routine reads
 * descriptor (i), where i is the VRS offset from 32K.  
 */
int ReadVRD (uint8_t *VRD, int i)
{
  uint32_t sector, count;

  count = 2048 >> sdivshift;
  if (count == 0) count = 1;
  sector = (32768 >> sdivshift) + i * count + lastSessionStartLBA;
  Verbose("  VRS %d (sector %u): ", i, sector);
  return ReadSectors(VRD, sector, count);
}

/*
 * The following routine looks for the Volume Recognition sequence.  
 * There is nothing fatal here.
 */
int VerifyVRS(void)
{
  int error = 0;
  uint32_t i;
  int  Term = 0, NSR_Found = 0;
  bool VRS_OK = true, BEA_Found = false;
  uint8_t *VRS;

  VRS = (uint8_t *)malloc(MAX(secsize, 2048));
  if (VRS) {
    Information("\n--Verifying the Volume Recognition Sequence.\n");
    /* Process ISO9660 VRS */
    i = 0;
    while (VRS_OK) {
      error = ReadVRD(VRS, i);
      if (!error) {
        VRS_OK = !strncmp((const char*) VRS+1, VRS_ISO9660, 5);
        if (VRS_OK) {
          Term = VRS[0] == 0xff;
          switch (VRS[0]) {
            case 0: Verbose("ISO 9660 Boot Record\n");                        break;
            case 1: Verbose("ISO 9660 Primary Volume Descriptor\n");          break;
            case 2: Verbose("ISO 9660 Supplementary Volume Descriptor\n");    break;
            case 3: Verbose("ISO 9660 Volume Partition Descriptor\n");        break;
            case 255: Verbose("ISO 9660 Volume Descriptor Set Terminator\n"); break;
            default: Verbose("9660 VRS (code %u)\n", VRS[0]);
          }
          i++;
        }
      } else {
        VRS_OK = false;
      }
    }
    if (i) {
      Verbose(" %u ISO 9660 descriptors found.\n", i);
      if (!Term) {
        UDFError("**However, it was not terminated!\n");
      }
    } else {
      Verbose(" No ISO 9660 descriptors found.\n");
    }

    /* Process ISO 13346 */
    Term = 0;  //No terminating descriptor yet
    if (!error) {
      Verbose("  VRS %u            : ", i);
      VRS_OK = !strncmp((const char*) VRS+1, VRS_ISO13346_BEGIN, 5);
      if (VRS_OK) {
        BEA_Found = true;
        Verbose("Beginning Extended Area descriptor found.\n");
      } else {
        UDFError("**BEA01 is not present, skipping remaining VRS.\n");
      }
    }
    while (VRS_OK && !Term) {
      i++;
      error = ReadVRD(VRS, i);
      if (!error) {
        if (!NSR_Found) {
          NSR_Found = !strncmp((const char*) VRS+1, VRS_ISO13346_NSR, 4);
          if (NSR_Found) {
            UDF_Version = VRS[5] & 0x0f;
            Version_OK = true;
            Verbose("NSR0%u descriptor found.\n", UDF_Version);
          }
        } else {
          if (!strncmp((const char*) VRS+1, VRS_ISO13346_NSR, 4)) {
            MinorError("\n**Found an extra NSR descriptor.\n");
          }
        }
        Term = !strncmp((const char*) VRS+1, VRS_ISO13346_END, 5);
        if (Term) {
          i++;
        }
      } else {
        VRS_OK = false;
      }
    }
    if (BEA_Found && !NSR_Found) {
      UDFError("\n**NSR0x is not present in the VRS!\n");
    }
    if (BEA_Found) {
      if (Term) {
        Verbose("VRS sequence was terminated.\n");
      } else {
        UDFError("\n**TEA01 is not present in the VRS!\n");
      }
    } else {
      UDFError("\n**No Extended VRS found.\n");
    }
    if (i) {
      track_volspace(lastSessionStartLBA + (32768 >> sdivshift), 
                     i * ((2048 >> sdivshift) ? (2048 >> sdivshift) : 1), 
                     "Volume Recognition Sequence");
    }
    free(VRS);
  } else {
    OperationalError("\n**Unable to allocate memory for reading the VRS.\n");
  }
  return error;
}
