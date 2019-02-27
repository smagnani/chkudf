#include <inttypes.h>
#include <stdio.h>
#include "chkudf.h"
#include "protos.h"

int bitv[8] = {1, 2, 4, 8, 16, 32, 64, 128};

int track_freespace(uint16_t ptn, uint32_t addr, uint32_t extentNumBytes)
{
  // @todo Decide if Error.Sector should be block address of extent's container
  do {
    uint32_t endAddr = addr + ((extentNumBytes + blocksize - 1) >> bdivshift);
    if (ptn >= PTN_no) {
      Error.Code = ERR_BAD_PTN;
      Error.Sector = addr;
      Error.Expected = PTN_no;
      Error.Found = ptn;
      break;
    }
    if (addr >= Part_Info[ptn].Len) {
      Error.Code = ERR_BAD_LBN;
      Error.Sector = addr;
      Error.Expected = Part_Info[ptn].Len;
      Error.Found = addr;
      break;
    }
    if ((endAddr > Part_Info[ptn].Len) || (endAddr < addr)) {
      Error.Code = ERR_BAD_LBN;
      Error.Sector = addr;
      Error.Expected = Part_Info[ptn].Len;
      Error.Found = endAddr;
      break;
    }

    if (Part_Info[ptn].SpMap) {
      uint32_t extentNumBlocks = endAddr - addr;
      while (extentNumBlocks > 0) {
        uint32_t bytep, bitp;
        bytep = addr >> 3;
        bitp = addr & 7;
        if (Part_Info[ptn].SpMap[bytep] & bitv[bitp]) {
          // Report only the first overlapping block as that is what limits the extent
          if (!Error.Code) {
            Error.Code = ERR_FILE_SPACE_OVERLAP;    // @todo Appropriate error?
            Error.Sector = addr;
          }
        } else {
          Part_Info[ptn].SpMap[bytep] |= bitv[bitp];
        }
        extentNumBlocks--;
        addr++;
      }
    }
  } while (0);

  if (Error.Code) {
    DumpError();
  }
  return 0;
}

int track_filespace(uint16_t ptn, uint32_t addr, uint32_t extentNumBytes)
{
  // @todo Decide if Error.Sector should be block address of extent's container
  do {
    uint32_t endAddr = addr + ((extentNumBytes + blocksize - 1) >> bdivshift);
    if (ptn >= PTN_no) {
      Error.Code = ERR_BAD_PTN;
      Error.Sector = addr;
      Error.Expected = PTN_no;
      Error.Found = ptn;
      break;
    }
    if (addr >= Part_Info[ptn].Len) {
      Error.Code = ERR_BAD_LBN;
      Error.Sector = addr;
      Error.Expected = Part_Info[ptn].Len;
      Error.Found = addr;
      break;
    }
    if ((endAddr > Part_Info[ptn].Len) || (endAddr < addr)) {
      Error.Code = ERR_BAD_LBN;
      Error.Sector = addr;
      Error.Expected = Part_Info[ptn].Len;
      Error.Found = endAddr;
      break;
    }

    if (Part_Info[ptn].MyMap) {
      uint32_t extentNumBlocks = endAddr - addr;
      while (extentNumBlocks > 0) {
        uint32_t bytep, bitp;
        bytep = addr >> 3;
        bitp = addr & 7;
        if ((Part_Info[ptn].MyMap[bytep] & bitv[bitp]) == 0) {
          // Report only the first overlapping block as that is what limits the extent
          if (!Error.Code) {
            Error.Code = ERR_FILE_SPACE_OVERLAP;
            Error.Sector = addr;
          }
        } else {
          Part_Info[ptn].MyMap[bytep] &= ~bitv[bitp];
        }
        extentNumBlocks--;
        addr++;
      }
    }
  } while (0);

  if (Error.Code) {
    DumpError();
  }
  return 0;
}

int check_filespace(void)
{
  unsigned int i, j;
  int pass;    // 1 == in-use blocks marked free, 2 == free blocks marked in-use

  for (i = 0; i < PTN_no; i++) {
    if (Part_Info[i].SpMap && Part_Info[i].MyMap) {
      unsigned int numMapBytes = BITMAP_NUM_BYTES(Part_Info[i].Len);
      unsigned int numMismarkedFree = 0;
      unsigned int numMismarkedInUse = 0;
      printf("\n--Checking partition reference %u for space errors.\n", i);
      for (pass = 1; pass <= 2; ++pass) {
        int numSuppressed = 0;
        int numReported = 0;
        int askForMore = 24;
        bool bSuppress = false;

        for (j = 0; j < numMapBytes; j++) {
          if (Part_Info[i].SpMap[j] != Part_Info[i].MyMap[j]) {
            // See if the mismatch is for the current pass
            uint8_t mismatchBits = Part_Info[i].SpMap[j] ^ Part_Info[i].MyMap[j];
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
              printf("  **At byte %u, (sectors %u-%u), recorded mask is %02x, mapped is %02x (mismatch %02x)\n",
                     j, j * 8, j* 8 + 7, Part_Info[i].SpMap[j], Part_Info[i].MyMap[j], mismatchBits);

              if (askForMore && ((numReported % askForMore) == 0)) {
                char ans;
                printf("Print more? ");
                fflush(stdout);
                ans = getchar();
                if ((ans == 'n') || (ans == 'N')) {
                  bSuppress = true;
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

  printf("  There are %u directories and %u files.\n", Num_Dirs, Num_Files);
  if (ID_UID && (Num_Dirs != ID_Dirs)) {
    printf("**The integrity descriptor indicated %u directories.\n",
           ID_Dirs);
  }
  if (ID_UID && (Num_Files != ID_Files)) {
    printf("**The integrity descriptor indicated %u files.\n",
           ID_Files);
  }
  if (Num_Type_Err) {
    printf("**%u files had a bad File Type.\n", Num_Type_Err);
  }
  printf("\n%s%u FIDs had a wrong location value.\n", FID_Loc_Wrong ? "**" : "  ", FID_Loc_Wrong);
  return 0;
}

int check_uniqueid(void)
{
  int i, j, ii, jj;
  uint64_t maxUID;
  uint64_t nextUID;

  printf("\n--Checking Unique ID list.\n");

  // Determine the maximum unique ID we've encountered
  maxUID = 0;
  for (i = 0; i < ICBlist_len; i++) {
    uint32_t *linkedUIDs = ICBlist[i].LinkedUIDs;
    if (ICBlist[i].UniqueID > maxUID) {
      maxUID = ICBlist[i].UniqueID;
    }

    for (j = 0; j < ICBlist[i].MaxLinkedUIDs; j++) {
      if (linkedUIDs[j] > maxUID) {
        maxUID = linkedUIDs[j];
      }
    }
  }

  // Scan for illegal values
  for (i = 0; i < ICBlist_len; i++) {
    uint32_t *linkedUIDs = ICBlist[i].LinkedUIDs;
    if ((ICBlist[i].UniqueID > 0) && ((ICBlist[i].UniqueID & 0xFFFFFFF0) == 0)) {
      printf("**ICB at %04x:%08x has illegal UID 0x%" PRIX64 "\n", ICBlist[i].Ptn,
             ICBlist[i].LBN, ICBlist[i].UniqueID);
    }

    for (j = 0; j < ICBlist[i].MaxLinkedUIDs; j++) {
      if ((linkedUIDs[j] > 0) && ((linkedUIDs[j] & 0xFFFFFFF0) == 0)) {
        printf("**ICB at %04x:%08x has illegal linked UID 0x%" PRIX64 "\n", ICBlist[i].Ptn,
               ICBlist[i].LBN, ICBlist[i].UniqueID);
      }
    }
  }

  for (i = 0; i < ICBlist_len; i++) {

    for (ii=0; ii <= ICBlist[i].MaxLinkedUIDs; ++ii) {
      uint64_t iUniqueID = (ii == 0) ? ICBlist[i].UniqueID
                                     : ICBlist[i].LinkedUIDs[ii-1];
      if ((ii > 0) && !iUniqueID)
        break;   // No more linked UIDs for [i]

      bool bDuplicate = false;
      bool bAlreadyReported = false;
      for (j = 0; !bAlreadyReported && (j < ICBlist_len); j++) {
        if (j == i)
          continue;  // @todo This skips over duplicate checking within ICBlist[i].LinkedUIDs

        for (jj=0; jj <= ICBlist[j].MaxLinkedUIDs; ++jj) {
          uint64_t jUniqueID = (jj == 0) ? ICBlist[j].UniqueID
                                         : ICBlist[j].LinkedUIDs[jj-1];
          if ((jj > 0) && !jUniqueID)
            break;   // No more linked UIDs for [j]

          if (iUniqueID == jUniqueID) {
            if (j < i) {
              // This duplicate was already reported in an earlier context
              bAlreadyReported = true;
             break;
            }

            if (!bDuplicate) {
              printf("**Multiple ICBs with unique ID %" PRIu64 ":\n", jUniqueID);
              printf("    %04x:%08x%s\n", ICBlist[i].Ptn, ICBlist[i].LBN,
                     (ii > 0) ? " [link]" : "");
              bDuplicate = true;
            }
            printf("    %04x:%08x%s\n", ICBlist[j].Ptn, ICBlist[j].LBN,
                   (jj > 0) ? " [link]" : "");
            break;   // stop processing [j], don't want to print it more than once
          }
        }   // for each unique ID in entry [j]
      }     // for each entry [j]
    }       // for each unique ID in entry [i]
  }         // for each entry [i]

  nextUID = maxUID + 1;
  // UDF reserves UIDs ending in 00000000 - 0000000F
  if (!(nextUID & 0xFFFFFFF0))
    nextUID = (nextUID | 0xF) + 1;
  printf("  The next Unique ID is %" PRIu64 ".\n", nextUID);
  if (ID_UID && (nextUID != ID_UID)) {
    const char* errFlag = (ID_UID > nextUID) ? "  " : "**";
    printf("%sThe Integrity Descriptor indicated a next Unique ID of %" PRIu64 ".\n",
           errFlag, ID_UID);
  }
  return 0;
}
