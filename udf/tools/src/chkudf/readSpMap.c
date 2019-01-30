#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

static int ReadSpaceBitmap(uint16_t ptn)
{
  struct SpaceBitmapHdr *BMD;

  if (Part_Info[ptn].Space != -1) {
    printf("\n--Reading the Space Bitmap Descriptor for partition reference %u.\n", ptn);
    printf("  Descriptor is %u sectors at %u:%u.\n",
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
        printf("  Partition is %u blocks long, requiring %u bytes.\n",
               Part_Info[ptn].Len, mapBytesRequired);
        if (U_endian32(BMD->N_Bits) != Part_Info[ptn].Len) {
          printf("**Partition is %u blocks long but is described by %u bits.\n",
                 Part_Info[ptn].Len, U_endian32(BMD->N_Bits));
        }
        if (BITMAP_NUM_BYTES(U_endian32(BMD->N_Bits)) != mapBytesRecorded) {
          printf("**Bitmap descriptor requires %u bytes to hold %u bits.\n",
                 mapBytesRecorded, U_endian32(BMD->N_Bits));
        }
        if (Part_Info[ptn].SpMap && (mapBytesRecorded < Part_Info[ptn].SpLen)) {
          memcpy(Part_Info[ptn].SpMap,
                 (uint8_t *)BMD + sizeof(struct SpaceBitmapHdr),
                 MIN(mapBytesRecorded, mapBytesRequired));

          // Mask out bits for blocks beyond end of partition
          Part_Info[ptn].SpMap[mapBytesRequired-1] &= Part_Info[ptn].FinalMapByteMask;

          printf("  Read the space bitmap for partition reference %u.\n", ptn);
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

static void ReadSpaceTable(uint16_t ptn)
{
  struct UnallocSpEntry *USE = malloc(blocksize);

  if (USE) {
    uint32_t nextUSELocation = Part_Info[ptn].Space;
    uint32_t nextUSESize     = Part_Info[ptn].SpLen;
    uint32_t minNextUnallocStart = 0;
    bool     bWarnedUnsorted = false;
    const uint32_t maxExtentLength = 0x3FFFFFFF & ~(blocksize - 1);

    printf("\n--Reading Unallocated Space Entries for partition reference %u.\n", ptn);
    while (nextUSELocation != -1) {
      struct short_ad *sad;
      uint32_t L_AD;
      uint32_t ad_offset = 0;

      uint32_t curUSESize     = nextUSESize;
      uint32_t curUSELocation = nextUSELocation;
      nextUSELocation = -1;

      printf("  [loc=%u, size=%u]\n", curUSELocation, curUSESize);
      ReadLBlocks(USE, curUSELocation, ptn, 1);
      // @todo Handle nextSpaceSize > blocksize gracefully
      track_filespace(ptn, curUSELocation, blocksize);

      CheckTag((struct tag *)USE, curUSELocation, TAGID_UNALLOC_SP_ENTRY,
               0, curUSESize);
      if (Error.Code == ERR_TAGID) {
        printf("    **Not a space entry descriptor.\n");
        break;
      }

      DumpError();

// @todo bail if  Error.Code??

      // UDF: "Only Short Allocation Descriptors shall be used."
      if ((U_endian16(USE->sICBTag.Flags) & ADTYPEMASK) != ADSHORT) {
        Error.Code     = ERR_PROHIBITED_AD_TYPE;
        Error.Sector   = curUSELocation;
        Error.Expected = ADSHORT;
        Error.Found    = U_endian16(USE->sICBTag.Flags) & ADTYPEMASK;

        DumpError();
        break;    // Can't proceed further with the table
      }

      L_AD = U_endian32(USE->L_AD);
      sad = (struct short_ad *) (USE + 1);

      while ((ad_offset < L_AD) && (nextUSELocation == -1)) {
        uint32_t extentLocation = U_endian32(sad->Location);
        uint32_t extentLength   = EXTENT_LENGTH(sad->ExtentLengthAndType);
        uint32_t extentType     = EXTENT_TYPE(sad->ExtentLengthAndType);
        if (extentLength) {
          switch (extentType) {
            case E_RECORDED:
            case E_UNALLOCATED:
              Error.Code     = ERR_PROHIBITED_EXTENT_TYPE;
              Error.Expected = E_ALLOCATED;
              Error.Found    = extentType;
              break;

            case E_ALLOCATED:
              // UDF requires extents to be sorted by ascending location,
              // and for adjacent extents to be discontiguous except when
              // the preceding one is the maximum allowable length
              if (extentLength & (blocksize - 1)) {
                Error.Code     = ERR_BAD_AD;
                Error.Expected = (extentLength & ~(blocksize - 1)) + blocksize;
                Error.Found    = extentLength;
              } else if (extentLocation < minNextUnallocStart) {
                Error.Expected = minNextUnallocStart;
                Error.Found    = extentLocation;
                if (extentLocation == (minNextUnallocStart - 1)) {
                  Error.Code = ERR_SEQ_ALLOC;  // Adjacent, but shouldn't be
                } else {
                  Error.Code = ERR_UNSORTED_EXTENTS;
                }
              } else {
                minNextUnallocStart = extentLocation + (extentLength >> bdivshift);
                if (extentLength < maxExtentLength)
                  ++minNextUnallocStart;
              }
              break;

            case E_ALLOCEXTENT:
              // Chain
              if ((extentLength > blocksize) || (extentLength < sizeof(*USE))) {
                Error.Code     = ERR_BAD_AD;
                Error.Expected = blocksize;
                Error.Found    = extentLength;
              }

              nextUSESize     = extentLength;
              nextUSELocation = extentLocation;
              break;
          }  // switch (extentType)
        }    // if (extentLength)

        printf("    [ad_offset=%u, atype=%u, loc=%u, len=%u]%s\n",
                ad_offset, extentType, extentLocation, extentLength,
                Error.Code ? " (ILLEGAL!)" : "");

        if (Error.Code == ERR_UNSORTED_EXTENTS) {
          if (bWarnedUnsorted) {
            ClearError();
          }
          bWarnedUnsorted = true;
        }

        if (!Error.Code && (extentLength == 0)) {
            Error.Code     = ERR_UNEXPECTED_ZERO_LEN;
            Error.Expected = L_AD;
            Error.Found    = ad_offset;
        }

        if (Error.Code) {
          Error.Sector = curUSELocation;
          DumpError();
        }

        if (extentLength == 0) {
          // ECMA-167r3 sec. 4.12: zero extent length terminates allocation descriptors
          nextUSELocation = -1;
          break;
        }

        // Do this after the above print to provide context in the event of
        // a tracking error
        if (extentType == E_ALLOCATED) {
          track_freespace(ptn, extentLocation, extentLength >> bdivshift);
        }
        ++sad;
        ad_offset += sizeof(sad);
      }  // walk short_ads

    }  // walk USE block chain

    free(USE);
  }
}
/*
 *  Read description of unallocated space (bitmap or table) for each partition.
 */

int ReadPartitionUnallocatedSpaceDescs(void)
{
  uint16_t i;

  for (i = 0; i < PTN_no; i++) {
    if (Part_Info[i].SpaceTag == TAGID_SPACE_BMAP) {
      ReadSpaceBitmap(i);
    } else if (Part_Info[i].SpaceTag == TAGID_UNALLOC_SP_ENTRY) {
      ReadSpaceTable(i);
    }
  }

  return 0;
}

