// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999-2001 Ben Fennema. All rights reserved.
// Copyright (c) 2019 Steve Magnani. All rights reserved.

#include <inttypes.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "chkudf.h"
#include "protos.h"

// Forward declarations
static bool set_true_unique_id(sICB_trk *pICBinfo, uint64_t uniqueID);
static bool link_icb(sICB_trk *pICBinfo, uint32_t uniqueID_L);

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
  uint64_t file_length;
  uint64_t infoLength;
  int    error, sizeAD;
  struct long_ad *lad;
  struct short_ad *sad;
  struct AllocationExtentDesc *AED = NULL; 
  uint16_t ADlength;
  uint32_t Location_AEDP, ad_offset;
  uint32_t L_EA, L_AD;
  size_t   xfe_hdr_sz;
  uint8_t *ad_start;
  bool     isLAD;

  ad_offset = 0;   //Offset into the allocation descriptors

  infoLength = U_endian64(xFE->InfoLength);
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
      Debug("\n  [type=%s, ADlength=%u, info_length=%" PRIu64 "]  ",
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
        Debug("\n    [ad_offset=%u, atype=%d, loc=%u, len=%u, file_length=%" PRIu64 "]  ",
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
              if (file_length >= infoLength) {
                Debug(" (Tail)");
              } else {
                file_length += curExtentLength;
              }
              ad_offset += sizeAD;
              break;

            case E_UNALLOCATED:
              if (file_length >= infoLength) {
                UDFError(" **(ILLEGAL TAIL)");
              } else {
                file_length += curExtentLength;
              }
              ad_offset += sizeAD;
              Debug(" --Unallocated Extent--");
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
                Debug("Error=%d, Error.Code=%d\n", error, Error.Code);
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
              Debug("\n      [NEW ADlength=%u]  ", ADlength);
              break;

            // No other cases, this is just to avoid a "missing default" warning
            default:
              break;
          }
        }  // curExtentLength != 0
      }    // while (ad_offset < ADlength)
      printf("  [file_length=%" PRIu64 "]  ", file_length);
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
      if (U_endian64(xFE->InfoLength) != L_AD) {
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
        set_true_unique_id(ICBlist + ICB_offs, U_endian64(xFE->FE.UniqueId));
        ICBlist[ICB_offs].LinkRec = U_endian16(xFE->LinkCount);
        ICBlist[ICB_offs].FE_LBN = Location + i;
        ICBlist[ICB_offs].FE_Ptn = ptn;
        track_file_allocation(xFE, ptn);
      } else {
        ClearError();
        if (!CheckTag((struct tag *)xFE, Location + i, TAGID_EXT_FILE_ENTRY, 16, Length)) {
          set_true_unique_id(ICBlist + ICB_offs, U_endian64(xFE->EFE.UniqueId));
          ICBlist[ICB_offs].LinkRec = U_endian16(xFE->LinkCount);
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
int read_icb(struct FE_or_EFE *xFE, struct long_ad icbExtent,
             struct FileIDDesc *FID, uint16_t* pPrevCharacteristics)
{
  uint32_t interval;
  int32_t  ICB_offs;
  int      error, temp;
  struct FE_or_EFE *EA;
  struct long_ad *sExtAttrICB = NULL;

  uint16_t ptn      = U_endian16(icbExtent.Location_PartNo);
  uint32_t Location = U_endian32(icbExtent.Location_LBN);
  uint32_t Length   = EXTENT_LENGTH(icbExtent.ExtentLengthAndType);

  error = 0;

  if (pPrevCharacteristics) {
    *pPrevCharacteristics = 0;
  }

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
          if (U_endian16(FID->sTag.uDescriptorVersion) > 2) {
            link_icb(ICBlist + ICB_offs, U_endian32(FID->ICB.UdfUniqueId_L));
          } else {
            // Pre-UDF2.00: UdfUniqueId_L not available
            ICBlist[ICB_offs].Link++;
          }
          if (pPrevCharacteristics) {
            *pPrevCharacteristics = ICBlist[ICB_offs].Characteristics;
          }
          if (   !(ICBlist[ICB_offs].Characteristics & CHILD_ATTR)
              && ((FID->Characteristics & (PARENT_ATTR | DIR_ATTR)) == DIR_ATTR)) {
            // First time this directory has been counted as a child
            ICBlist[ICB_offs].Characteristics |= CHILD_ATTR;
            Num_Dirs++;
          }
          ICBlist[ICB_offs].Characteristics |= FID->Characteristics;
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

      memset(ICBlist + ICB_offs, 0, sizeof(ICBlist[ICB_offs]));
      ICBlist[ICB_offs].LBN = Location;
      ICBlist[ICB_offs].Ptn = ptn;
      if (FID) {
        ICBlist[ICB_offs].Link = 1;
        ICBlist[ICB_offs].Characteristics = FID->Characteristics;
        if (U_endian16(FID->sTag.uDescriptorVersion) > 2) {
          ICBlist[ICB_offs].UniqueID = U_endian32(FID->ICB.UdfUniqueId_L);
        }
      }
      walk_icb_hierarchy(xFE, ptn, Location, Length, ICB_offs);

      // Accounting for cross-check of Logical Volume Integrity Descriptor
      // These are the clarified rules first specified in UDF 2.50.
      if (FID && !(FID->Characteristics & (PARENT_ATTR | DELETE_ATTR))) {
        if (FID->Characteristics & DIR_ATTR) {
          if (xFE->sICBTag.FileType != FILE_TYPE_STREAMDIR) {
            ICBlist[ICB_offs].Characteristics |= CHILD_ATTR;
            Num_Dirs++;
          }
        } else {
          // @todo Don't bump this when we're traversing a stream directory
          Num_Files++;
        }
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
            read_icb(EA, *sExtAttrICB, NULL, NULL);
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

static bool add_linked_uid(sICB_trk *pICBinfo, uint32_t uniqueID_L)
{
  bool bSuccess = false;
  uint32_t i;

  if (!pICBinfo->LinkedUIDs) {
    pICBinfo->LinkedUIDs = calloc(LINKED_UIDS_PER_CHUNK, sizeof(uniqueID_L));
    if (pICBinfo->LinkedUIDs) {
      pICBinfo->MaxLinkedUIDs = LINKED_UIDS_PER_CHUNK;
    }
  }

  for (i=0; i < pICBinfo->MaxLinkedUIDs; ++i) {
    if (pICBinfo->LinkedUIDs[i] == 0) {
      pICBinfo->LinkedUIDs[i] = uniqueID_L;
      bSuccess = true;
      break;
    }
  }

  if (!bSuccess) {
    // Time to grow the array
    uint32_t newMaxLinkedUIDs = pICBinfo->MaxLinkedUIDs + LINKED_UIDS_PER_CHUNK;
    uint32_t *newLinkedUIDs   = realloc(pICBinfo->LinkedUIDs, newMaxLinkedUIDs * sizeof(uint32_t));
    if (newLinkedUIDs) {
      pICBinfo->LinkedUIDs    = newLinkedUIDs;
      pICBinfo->MaxLinkedUIDs = newMaxLinkedUIDs;
      pICBinfo->LinkedUIDs[i] = uniqueID_L;
      bSuccess = true;
      memset(pICBinfo->LinkedUIDs + i + 1, 0, sizeof(uint32_t) * (LINKED_UIDS_PER_CHUNK - 1));
    }
  }

  return bSuccess;
}

/**
 * Make the unique ID from an (Extended) File Entry the 'real' ID for its ICB.
 *
 * @param[in,out]  pICBinfo    Tracking entry for the EFE/FE containing the uniqueID
 * @param[in]      uniqueID    Unique ID recorded in the EFE/FE
 */
static bool set_true_unique_id(sICB_trk *pICBinfo, uint64_t uniqueID)
{
  bool bSuccess = false;

  if ((pICBinfo->UniqueID & UINT64_C(0xFFFFFFFF00000000)) == 0) {
    // pICBinfo->UniqueID might be a 32-bit ID from a struct FileIDDesc
    if (pICBinfo->UniqueID == (uniqueID & 0xFFFFFFFF)) {
      pICBinfo->UniqueID = uniqueID;  // Possibly filling in the high word
      bSuccess = true;
    } else {
      Debug("Found unique ID %" PRIu64 " for hard link %" PRIu64, uniqueID, pICBinfo->UniqueID);
      bSuccess = add_linked_uid(pICBinfo, (uint32_t) pICBinfo->UniqueID);
      pICBinfo->UniqueID = uniqueID;
    }
  } else {
    if (pICBinfo->UniqueID == uniqueID) {
      bSuccess = true;
    } else {
      // @todo report an error - attempt to change unique ID
    }
  }

  return bSuccess;
}

/**
 * Increment the link count for an ICB, and associate the specified (short) unique ID
 * with that ICB.
 *
 * @param[in,out]  pICBinfo    Tracking entry for an ICB
 * @param[in]      uniqueID_L  Unique ID recorded in a FileIDDesc referencing the ICB
 *
 * @return @b      true        Success
 * @return @b      false       Memory allocation failure
 */
static bool link_icb(sICB_trk *pICBinfo, uint32_t uniqueID_L)
{
  bool bSuccess = true;
  // @todo check for illegal uniqueID_L

  pICBinfo->Link++;
  if (uniqueID_L != (pICBinfo->UniqueID & 0xFFFFFFFF)) {
    Debug(" Hard link unique ID %u -> %" PRIu64 "\n", uniqueID_L, pICBinfo->UniqueID);
    bSuccess = add_linked_uid(pICBinfo, uniqueID_L);
  }

  return bSuccess;
}
