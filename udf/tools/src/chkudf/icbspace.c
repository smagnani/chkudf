#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "chkudf.h"
#include "protos.h"

/* 
 * returns 1 if address 1 is greater than address 2, -1 if less than, and
 * 0 if equal. Used only by read_icb for sorting the icb list.
 */

int compare_address(uint16_t ptn1, uint16_t ptn2, uint32_t addr1, uint32_t addr2)
{
  if (ptn1 > ptn2) return 1;
  if (ptn1 < ptn2) return -1;
  if (addr1 > addr2) return 1;
  if (addr1 < addr2) return -1;
  return 0;
}

/*
 * The following routine takes a File Entry as input and tracks the space
 * used by the file data and by the Extended Attributes of that file.
 */
int track_file_allocation(const struct FE_or_EFE *xFE, uint16_t ptn)
{
  unsigned long long file_length;
  unsigned long long infoLength;
  int    error, sizeAD, Prev_Typ;
  struct long_ad *lad;
  struct short_ad *sad;
  struct AllocationExtentDesc *AED = NULL; 
  uint16_t ADlength;
  uint32_t Location_AEDP, ad_offset;
  uint16_t Next_ptn;
  uint32_t Next_LBN;
  uint32_t L_EA, L_AD;
  size_t   xfe_hdr_sz;
  uint8_t *ad_start;
  bool     isLAD;

  ad_offset = 0;   //Offset into the allocation descriptors
  Next_ptn = 0;    //The partition ref no of the previous AD
  Next_LBN = 0;    //The LBN of the sector after the previous AD
  Prev_Typ = -1;   //The type of the previous AD

  infoLength =   (((unsigned long long) U_endian32(xFE->InfoLengthH)) << 32)
               | U_endian32(xFE->InfoLengthL);
  if (U_endian16(xFE->sTag.uTagID) == TAGID_EXT_FILE_ENTRY) {
    xfe_hdr_sz = sizeof(struct ExtFileEntry);
    L_EA = U_endian32(xFE->EFE.L_EA);
    L_AD = U_endian32(xFE->EFE.L_AD);
  } else {
    xfe_hdr_sz = sizeof(struct FileEntry);
    L_EA = U_endian32(xFE->FE.L_EA);
    L_AD = U_endian32(xFE->FE.L_AD);
  }

  ADlength = L_AD;
  switch(U_endian16(xFE->sICBTag.Flags) & ADTYPEMASK) {
    case ADSHORT:
    case ADLONG:
      isLAD = (U_endian16(xFE->sICBTag.Flags) & ADTYPEMASK) == ADLONG;
      sizeAD = isLAD ?  sizeof(struct long_ad) : sizeof(struct short_ad);
      file_length = 0;
      ad_start = ((uint8_t *) xFE) + xfe_hdr_sz + L_EA;
      printf("\n  [type=%s, ADlength=%u, info_length=%llu]  ",
             isLAD ? "LONG" : "SHORT", ADlength, infoLength);
      while (ad_offset < ADlength) {
        uint32_t curExtentLength;
        sad = (struct short_ad *)(ad_start + ad_offset);
        lad = (struct long_ad *)(ad_start + ad_offset);

        // Note, since long_ad is a superset of short_ad this is valid for both
        curExtentLength = EXTENT_LENGTH(sad->ExtentLengthAndType);
        if (isLAD) {
          ptn = U_endian16(lad->Location_PartNo);
        }
        printf("\n    [ad_offset=%u, atype=%d, loc=%u, len=%u, file_length=%llu]  ",
               ad_offset,
               EXTENT_TYPE(sad->ExtentLengthAndType),
               U_endian32(sad->Location),
               curExtentLength,
               file_length);
        if (curExtentLength == 0) {
          // Zero-length extent - force loop exit
          break;
        } else {
          switch(EXTENT_TYPE(sad->ExtentLengthAndType)) {
            case E_RECORDED:
            case E_ALLOCATED:
              track_filespace(ptn, U_endian32(sad->Location), curExtentLength);
              // @todo If extent is invalid (i.e. huge length) we may continue on
              //       for quite a bit even though we've left the tracks
              if ((ptn == Next_ptn) && (U_endian32(sad->Location) == Next_LBN) &&
                  (EXTENT_TYPE(sad->ExtentLengthAndType) == Prev_Typ)) {
                Error.Code = ERR_SEQ_ALLOC;
                Error.Sector = xFE->sTag.uTagLoc;
                Error.Expected = Next_LBN;
                DumpError();
              }
              Next_ptn = ptn;
              Next_LBN = U_endian32(sad->Location) + (curExtentLength >> bdivshift);
              Prev_Typ = EXTENT_TYPE(sad->ExtentLengthAndType);
              if (file_length >= infoLength) {
                printf(" (Tail)");
              } else {
                file_length += curExtentLength;
              }
              ad_offset += sizeAD;
              break;

            case E_UNALLOCATED:
              if (file_length >= infoLength) {
                printf(" (ILLEGAL TAIL)");
              } else {
                file_length += curExtentLength;
              }
              ad_offset += sizeAD;
              printf(" --Unallocated Extent--");
              break;

            case E_ALLOCEXTENT:
              track_filespace(ptn, U_endian32(sad->Location), curExtentLength);
              if (!AED) {
                AED = (struct AllocationExtentDesc *)malloc(blocksize);
              }
              if (AED) {
                Location_AEDP = U_endian32(sad->Location);
                error = ReadLBlocks(AED, Location_AEDP, ptn, 1);
                if (!error) {
                  error = CheckTag((struct tag *)AED, Location_AEDP, TAGID_ALLOC_EXTENT, 8, blocksize - 16);
                }
              } else {
                error = 1;
              }
              if (error) {
                printf("Error=%d, Error.Code=%d\n", error, Error.Code);
                DumpError();
              }
              if (error == 2) {
                if (U_endian32(AED->sTag.uTagLoc) == 0xffffffff) {
                  error = 0;
                  Error.Code = 0;
                } else {
                  DumpError();
                  error = 0;
                }
              }
              if (!error) {
                ad_start = (uint8_t *)(AED + 1);
                ADlength = U_endian32(AED->L_AD);
                ad_offset = 0;
              } else {
                ad_offset = ADlength;
              }
              printf("\n      [NEW ADlength=%u]  ", ADlength);
              break;
          }
        }  // curExtentLength != 0
      }    // while (ad_offset < ADlength)
      printf("  [file_length=%llu]  ", file_length);
      if (file_length != infoLength) {
        if (((infoLength + blocksize - 1) & ~(blocksize - 1)) == file_length) {
          printf(" **ADs rounded up");
        } else {
          Error.Code = ERR_BAD_AD;
          Error.Sector = U_endian32(xFE->sTag.uTagLoc);
          Error.Expected = infoLength;
          Error.Found = file_length;
        }
      }
      free (AED);
      break;

    case ADNONE:
      if (U_endian32(xFE->InfoLengthL) != L_AD) {
        Error.Code = ERR_BAD_AD;
        Error.Sector = U_endian32(xFE->sTag.uTagLoc);
        Error.Expected = infoLength;
        Error.Found = L_AD;
      }
      break;
  }

  return error;
}

/* 
 * This routine walks an ICB hierarchy, marking space as allocated as it
 * goes.  The authoritative FE is noted in the FE_ptn and FE_LBN fields
 * of the entry in the ICB list.
 *
 * Currently, the space map does not have "owners" attached to allocation.
 * This means that on write once media, errors will be generated when more
 * than one File Entry in an ICB hierarchy identifies the same space.
 */
int walk_icb_hierarchy(struct FE_or_EFE *xFE, uint16_t ptn, uint32_t Location,
                       uint32_t Length, int ICB_offs)
{
  int i, error;

  /*
   * Mark the ICB extent as allocated
   */
  track_filespace(ptn, Location, Length);

  /*
   * Read each sector in turn (1 sector == 1 ICB)
   */
  for (i = 0; i < (Length >> bdivshift); i++) {
    error = ReadLBlocks(xFE, Location + i, ptn, 1);
    if (!error) {
      if (!CheckTag((struct tag *)xFE, Location + i, TAGID_FILE_ENTRY, 16, Length)) {
        ICBlist[ICB_offs].LinkRec = U_endian16(xFE->LinkCount);
        ICBlist[ICB_offs].UniqueID_L = U_endian32(xFE->FE.UniqueIdL);
        ICBlist[ICB_offs].FE_LBN = Location + i;
        ICBlist[ICB_offs].FE_Ptn = ptn;
        track_file_allocation(xFE, ptn);
      } else {
        ClearError();
        if (!CheckTag((struct tag *)xFE, Location + i, TAGID_EXT_FILE_ENTRY, 16, Length)) {
          ICBlist[ICB_offs].LinkRec = U_endian16(xFE->LinkCount);
          ICBlist[ICB_offs].UniqueID_L = U_endian32(xFE->EFE.UniqueIdL);
          ICBlist[ICB_offs].FE_LBN = Location + i;
          ICBlist[ICB_offs].FE_Ptn = ptn;
          track_file_allocation(xFE, ptn);
        } else {
          /*
           * A descriptor was found that wasn't a File Entry.
           */
          ClearError();
          if (!CheckTag((struct tag *)xFE, Location + i, TAGID_INDIRECT, 16, Length)) {
            walk_icb_hierarchy(xFE, U_endian32(((struct IndirectEntry *)xFE)->sIndirectICB.Location_LBN),
                               U_endian16(((struct IndirectEntry *)xFE)->sIndirectICB.Location_PartNo),
                               EXTENT_LENGTH(((struct IndirectEntry *)xFE)->sIndirectICB.ExtentLengthAndType),
                               ICB_offs);
          } else {
            DumpError();  // Wasn't a file entry, but should have been.
          }
        }
      }
    } else {
      Error.Code = ERR_READ;
      Error.Sector = Location;
      i = Length;
    }  /* Read/didn't read sector */
  }    /* Do each ICB in the extent */
  return error;
}


/*
 * This routine takes a partition, location, and length of an ICB extent as
 * input, marks the appropriate space as allocated in the space map, 
 * maintains a link count for the ICB, and returns the appropriate File
 * Entry (or Extended File Entry) data in *xFE.
 *
 * If FID == 0, this is a root entry or a re-read of an ICB.  If FID == 1,
 * this is the first read of an FE from a particular FID.  Note that the FE
 * might have been read before, but due to another FID.  
 *
 * Summary:
 *   FID == 0, space is not tracked and link counts not incremented.
 *   FID == 1, space is tracked and link counts are incremented.
 */
int read_icb(struct FE_or_EFE *xFE, uint16_t ptn, uint32_t Location, uint32_t Length,
             int FID)
{
  uint32_t interval;
  int32_t  ICB_offs;
  int      error, temp;
  struct FE_or_EFE *EA;
  struct long_ad *sExtAttrICB = NULL;

  error = 0;

  if (Length == 0) {
    // Nothing to track
    memset(xFE, 0, blocksize);  // Make sure caller doesn't see garbage or stale data
  } else {
    /*
     * Something to track...
     */
    ICB_offs = ICBlist_len >> 1; // start halfway for binary search
    temp = ICB_offs;
    interval = 1;
    while (temp) {
      interval <<= 1;
      temp >>= 1;
    }                          // set interval to 1/4 - 1/2 for binary search
    temp = 1;              /* temp is used as a relative position 
                            * indicator.  If temp = 0, ICB_offs exactly
                            * identifies the entry.  If temp = -1, ICB_offs
                            * points to an entry that should come before the
                            * new one.  If temp = 1, ICB_offs points to an
                            * entry that should come after the new one (or
                            * the end of the list)
                            */
                                
    while ((interval > 0) && ICBlist_len) {
      interval >>= 1;
      temp = compare_address(ICBlist[ICB_offs].Ptn, ptn, ICBlist[ICB_offs].LBN, Location);
      if (temp == 0) {
        interval = 0;
        if (FID) {
          /*
           * A FID was pointing to this ICB, which we already have tracked.
           * Increment our link count to note the fact.
           */
          ICBlist[ICB_offs].Link++;
        }
        ReadLBlocks(xFE, ICBlist[ICB_offs].FE_LBN, ICBlist[ICB_offs].FE_Ptn, 1);
      } else if (temp == 1) {
        ICB_offs -= interval;
        if (ICB_offs < 0) ICB_offs = 0;
      } else {
        ICB_offs += interval;
        if (ICB_offs >= ICBlist_len) ICB_offs = ICBlist_len - 1;
      }
    }
    if (temp) {
      /*
       * No match was found in the tracked list.
       * This code inserts an entry for a new ICB hierarchy.
       * The above code may have left the pointer to a point either
       * before or after the insertion point.
       */
      while ((temp == -1)  && (ICB_offs < (ICBlist_len - 1))) {
        ICB_offs++;
        temp = compare_address(ICBlist[ICB_offs].Ptn, ptn, ICBlist[ICB_offs].LBN, Location);
      }
  
      /*
       * ICB_offs now points to the first entry greater than the one we 
       * are inserting or the end of the list.
       */
      ICBlist_len++;
      if (ICBlist_len > ICBlist_alloc) {
        ICBlist = realloc(ICBlist, (ICBlist_alloc + ICB_Alloc) * sizeof(struct _sICB_trk));
        if (ICBlist) {
          ICBlist_alloc += ICB_Alloc;
        } else {
          Error.Code = ERR_NO_ICB_MEM;
        }
      }
      for (temp = ICBlist_len -1; temp > ICB_offs; temp--) {
        ICBlist[temp] = ICBlist[temp - 1];
      }
      if ((compare_address(ICBlist[ICB_offs].Ptn, ptn, ICBlist[ICB_offs].LBN, Location) < 0) &&
         (ICB_offs < (ICBlist_len - 1))) {
        ICB_offs++;
      }
      ICBlist[ICB_offs].LBN = Location;
      ICBlist[ICB_offs].Ptn = ptn;
      ICBlist[ICB_offs].UniqueID_L = 0;
      ICBlist[ICB_offs].LinkRec = 0;
      if (FID) {
        ICBlist[ICB_offs].Link = 1;
      } else {
        ICBlist[ICB_offs].Link = 0;
      }
      walk_icb_hierarchy(xFE, ptn, Location, Length, ICB_offs);
      switch(xFE->sICBTag.FileType) {
        case FILE_TYPE_UNSPECIFIED:
          Num_Type_Err++;
          break;

        case FILE_TYPE_DIRECTORY:
          Num_Dirs++;
          break;

        case FILE_TYPE_RAW:
          Num_Files++;
          break;
      }

      if (U_endian16(xFE->sTag.uTagID) == TAGID_EXT_FILE_ENTRY) {
        sExtAttrICB = &xFE->EFE.sExtAttrICB;
      } else {
        sExtAttrICB = &xFE->FE.sExtAttrICB;
      }

      if (EXTENT_LENGTH(sExtAttrICB->ExtentLengthAndType)) {
        EA = (struct FE_or_EFE *)malloc(blocksize);
        if (EA) {
          if (U_endian16(sExtAttrICB->Location_PartNo) < PTN_no) {
            printf(" EA: [%x:%08x]", U_endian16(sExtAttrICB->Location_PartNo),
                   U_endian32(sExtAttrICB->Location_LBN));
            read_icb(EA, U_endian16(sExtAttrICB->Location_PartNo), U_endian32(sExtAttrICB->Location_LBN),
                     EXTENT_LENGTH(sExtAttrICB->ExtentLengthAndType), 0);
          } else {
            printf("\n**EA field contains illegal partition reference number.\n");
          }
          free(EA);
        }
      }   
    }
  }        /* If something to track */      
  if (error) {
    DumpError();
  }
  return error;
}
