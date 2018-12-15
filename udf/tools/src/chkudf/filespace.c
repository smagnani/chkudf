#include <stdio.h>
#include "chkudf.h"
#include "protos.h"

int bitv[8] = {1, 2, 4, 8, 16, 32, 64, 128};

int track_filespace(UINT16 ptn, UINT32 addr, UINT32 extent)

{
  UINT32 bytep, bitp;
  if (ptn < PTN_no) {
    if (addr < Part_Info[ptn].Len) {
      if (Part_Info[ptn].MyMap) {
        extent = (extent + blocksize - 1) >> bdivshift;
        while (extent > 0) {
          bytep = addr >> 3;
          bitp = addr & 7;
          if ((Part_Info[ptn].MyMap[bytep] & bitv[bitp]) == 0) {
            Error.Code = ERR_FILE_SPACE_OVERLAP;
            Error.Sector = addr;
          } else {
            Part_Info[ptn].MyMap[bytep] &= ~bitv[bitp];
          }
          extent--;
          addr++;
        }
      }
    } else {
      Error.Code = ERR_BAD_LBN;
      Error.Sector = addr;
      Error.Expected = Part_Info[ptn].Len;
      Error.Found = addr;
    }
  } else {
    Error.Code = ERR_BAD_PTN;
    Error.Sector = addr;
    Error.Expected = PTN_no;
    Error.Found = ptn;
  }
  if (Error.Code) {
    DumpError();
  }
  return 0;
}

int check_filespace(void)
{
  int i, j;
  int pass;    // 1 == in-use blocks marked free, 2 == free blocks marked in-use

  for (i = 0; i < PTN_no; i++) {
    if (Part_Info[i].SpMap && Part_Info[i].MyMap) {
      unsigned int numMapBytes = BITMAP_NUM_BYTES(Part_Info[i].Len);
      unsigned int numMismarkedFree = 0;
      unsigned int numMismarkedInUse = 0;
      printf("\n--Checking partition reference %d for space errors.\n", i);
      for (pass = 1; pass <= 2; ++pass) {
        int numSuppressed = 0;
        int numReported = 0;
        int askForMore = 24;
        BOOL bSuppress = FALSE;

        for (j = 0; j < numMapBytes; j++) {
          if (Part_Info[i].SpMap[j] != Part_Info[i].MyMap[j]) {
            // See if the mismatch is for the current pass
            UINT8 mismatchBits = Part_Info[i].SpMap[j] ^ Part_Info[i].MyMap[j];
            if (pass == 1) {
              // In-use, but marked free?
              mismatchBits &= ~Part_Info[i].MyMap[j];
              if (mismatchBits)
                numMismarkedFree += countSetBits(mismatchBits);
              else
                continue;  // No
            } else {
              // Free, but marked in-use?
              mismatchBits &= Part_Info[i].MyMap[j];
              if (mismatchBits)
                numMismarkedInUse += countSetBits(mismatchBits);
              else
                continue;  // No
            }
            if (bSuppress) {
              ++numSuppressed;
            } else {
              if (numReported == 0) {
                if (pass == 1)
                  printf("  In-use blocks marked free:\n");
                else
                  printf("  Free blocks marked in-use:\n");
              }
              ++numReported;
              printf("  **At byte %d, (sectors %d-%d), recorded mask is %02x, mapped is %02x (mismatch %02x)\n",
                   j, j * 8, j* 8 + 7, Part_Info[i].SpMap[j], Part_Info[i].MyMap[j], mismatchBits);

              if (askForMore && ((numReported % askForMore) == 0)) {
                char ans;
                printf("Print more? ");
                fflush(stdout);
                ans = getchar();
                if ((ans == 'n') || (ans == 'N')) {
                  bSuppress = TRUE;
                } else if ((ans == 'a') || (ans == 'A')) {
                  askForMore = 0;
                }
              }
            } // if details not suppressed
          }   // if byte mismatch
        }     // for each partition slot

        if (numSuppressed > 0) {
          printf("  (%d additional mismatching bytes)\n", numSuppressed);
        }
      }  // for each pass

      printf("\n  %u in-use blocks mismarked free.\n", numMismarkedFree);
      printf("  %u free blocks mismarked in-use.\n", numMismarkedInUse);
    }  // if maps are available to compare
  }    // for each partition

  printf("  There are %d directories and %d files.\n", Num_Dirs, Num_Files);
  if (Num_Dirs != ID_Dirs) {
    printf("**The integrity descriptor (if it existed) indicated %d directories.\n",
           ID_Dirs);
  }
  if (Num_Files != ID_Files) {
    printf("**The integrity descriptor (if it existed) indicated %d files.\n",
           ID_Files);
  }
  if (Num_Type_Err) {
    printf("**%d files had a bad File Type.\n", Num_Type_Err);
  }
  printf("\n%s%d FIDs had a wrong location value.\n", FID_Loc_Wrong ? "**" : "  ", FID_Loc_Wrong);
  return 0;
}

int check_uniqueid(void)
{
  int i, j;
  UINT32 Max;

  Max = 0;
  printf("\n--Checking Unique ID list.\n");
  for (i = 0; i < ICBlist_len; i++) {
    if (ICBlist[i].UniqueID_L > Max) Max = ICBlist[i].UniqueID_L;

    for (j = i + 1; j < ICBlist_len; j++) {
      if (ICBlist[i].UniqueID_L == ICBlist[j].UniqueID_L) {
        printf("ICBs at %04x:%08x and %04x:%08x have a Unique ID of %d.\n",
          ICBlist[i].Ptn, ICBlist[i].LBN, ICBlist[j].Ptn, ICBlist[j].LBN,
          ICBlist[i].UniqueID_L);
      }
    }
  }
  printf("  The maximum Unique ID is %d.\n", Max);
  if (Max != (ID_UID-1)) {
    printf("**The Integrity Descriptor indicated a maximum Unique ID of %d.\n",
           (ID_UID-1));
  }
  return 0;
}
