#define _LARGEFILE64_SOURCE    // lseek64()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include "protos.h"

/* Cache everything in units of packet_size.  packet_size will be filled
 * in for all media, packet or not.
 * Subtle point: the caching obviates any need for 'buffer' to have any special alignment
 */

int ReadSectors(void *buffer, UINT32 address, UINT32 Count)
{
  int readOK, result, numsecs, i;
  void *curbuffer;

  //printf("  Reading sector %u.\n", address);
  readOK = FALSE;

  /* Search cache for existing bits */
  for (i = 0; i < NUM_CACHE; i++) {
    if ((Cache[i].Count > 0) && (address >= Cache[i].Address) && 
        ((address + Count) <= (Cache[i].Address + Cache[i].Count)) &&
         (address + Count > address)) {
      readOK = 1;
      curbuffer = Cache[i].Buffer + (address - Cache[i].Address) * secsize;
      bufno = i;
      i = NUM_CACHE;
    }
  }

  if (!readOK) {
    bufno++;
    bufno %= NUM_CACHE;
    if (Count > Cache[bufno].Allocated) {
      if (Cache[bufno].Buffer) {
        free(Cache[bufno].Buffer);
      }
      Cache[bufno].Buffer = malloc(secsize * Count);
      Cache[bufno].Allocated = Count;
    }
    if (Cache[bufno].Buffer) {
      if (scsi) {
        readOK = TRUE;
        for (i = 0; i < Count; i++) {
          scsi_read10(cdb, address + i, 1, secsize, 0, 0, 0);
          result = do_scsi(cdb, 10, Cache[bufno].Buffer + i * secsize,
                           secsize, 0, sensedata, sensebufsize);
          if (result) {
            readOK = FALSE;
          }
        }
        if (readOK) {
          Cache[bufno].Address = address;
          Cache[bufno].Count = Count;
        } else {
          Cache[bufno].Count = 0;
        }
      } else {
        off64_t byte_address = address * (off64_t) secsize;
        result = lseek64(device, byte_address, SEEK_SET);
        if (result != -1) {
          result = read(device, Cache[bufno].Buffer, secsize * Count);
          if (result == -1) {
            printf("**Read error #%d in %u\n", errno, address);
            readOK = 0;
            Cache[bufno].Count = 0;
          } else {
            if (result < secsize * Count) {
              numsecs = result / secsize;
              Cache[bufno].Address = address;
              Cache[bufno].Count = numsecs;
              readOK = numsecs > 0;
              if (readOK) {
                printf("**Only read %d sector%s.\n", numsecs, numsecs == 1 ? "" : "s");
              }
            } else {
              readOK = 1;
              Cache[bufno].Address = address;
              Cache[bufno].Count = Count;
            }   /* partial read  */
          }     /* non-scsi read */
        } else {
          readOK = 0; /* Seek failure */
        }
      }       /* scsi */
    } else {
      printf("**Couldn't malloc space for %u %u byte sectors.\n", Count, secsize);
    }
    if (readOK) {
      curbuffer = Cache[bufno].Buffer;
    } else {
      curbuffer = 0;
    }
  }
  if (curbuffer) {
    memcpy(buffer, curbuffer, Count << sdivshift);
  }
  return !readOK;
}


/* Bug: If count crosses sparing boundary... */

int ReadLBlocks(void *buffer, UINT32 address, UINT16 p_ref, UINT32 Count)
{
  sST_desc *PM_ST;
  int error, spared;
  UINT32 i, j;

  error = 0;
  if (p_ref < PTN_no) {
    switch(Part_Info[p_ref].type) {
      case PTN_TYP_REAL:
        if (address < Part_Info[p_ref].Len) {
          error = ReadSectors(buffer, 
                              (address * s_per_b) + Part_Info[p_ref].Offs,
                              Count * s_per_b);
        }
        break;

      case PTN_TYP_VIRTUAL:
        if ((address < Part_Info[p_ref].Len) && (Count == 1)) {
          error = ReadSectors(buffer, 
                              (Part_Info[p_ref].Extra[address] * s_per_b) + 
                                Part_Info[p_ref].Offs, Count * s_per_b);
        }
        break;

      case PTN_TYP_SPARE:
        PM_ST = (struct _sST_desc *)Part_Info[p_ref].Extra;
        if (PM_ST) {
          for (j = 0; j < Count; j++) {  /* Do each block independently */
            spared = FALSE;
            for (i = 0; (i < PM_ST->Size) && !spared; i++) {
              if (((address + j) >= PM_ST->Map[i].Original)  &&
                  ((address + j) < (PM_ST->Map[i].Original + PM_ST->Extent))) {
                printf("!!Getting sector from spare area!!\n");
                spared = TRUE;
                error = ReadSectors(buffer + (j << bdivshift), Part_Info[p_ref].Extra[2*i+1], s_per_b);
              }
            }
            if (!spared) {
              error = ReadSectors(buffer + (j << bdivshift), (address + j) * s_per_b
                                     + Part_Info[p_ref].Offs, s_per_b);
            }
          }
        } else {
          error = ReadSectors(buffer, address * s_per_b + Part_Info[p_ref].Offs, 
                              Count * s_per_b);
        }
        break;

      default:
        return 1;
    }
  }
  return error;
}

/*
 * @param[out]   buffer           Data read from the file.
 *                                This buffer should have a minimum length of
 *                                ((bytesRequested + 2 * blocksize - 2) / blocksize) blocks
 *                                to avoid overrun for all (startOffset, bytesRequested) combinations.
 *   FIXME: memcpy file data directly from block cache so we don't have to rely on caller overallocation to avoid overrun
 *
 * @param[in]    xfe              ICB describing the file
 * @param[in]    part             Which partition the file is part of
 * @param[in]    startOffset      # of bytes into the file at which to begin reading   FIXME: files >= 2 GiB
 * @param[in]    bytesRequested   Desired number of file data bytes
 * @param[out]   data_start_loc   Sector in which the startOffset byte of the file resides
 *
 * @return       Number of bytes read
 */
int ReadFileData(void *buffer, const struct FE_or_EFE *xfe, UINT16 part,
                 int startOffset, int bytesRequested, UINT32 *data_start_loc)
{
  const struct short_ad  *exts_ptr, *exts_end;
  const struct long_ad   *extl_ptr, *extl_end;
  struct AllocationExtentDesc *AED = NULL;
  UINT32           sector;
  UINT32           L_EA, L_AD;
  UINT32           blockBytesAvailable;
  int              error, offset, count, curFileOffset, bytesRemaining, firstpass;
  void            *fileData;
  BOOL             isEFE;

  firstpass = TRUE;
  error = 0;
  curFileOffset = startOffset;
  bytesRemaining = bytesRequested;  // TODO: This should be reduced if xfe InfoLength is too small
  fileData = buffer;

  if (U_endian16(xfe->sTag.uTagID) == TAGID_EXT_FILE_ENTRY) {
    isEFE = TRUE;
    L_EA = U_endian32(xfe->EFE.L_EA);
    L_AD = U_endian32(xfe->EFE.L_AD);
  } else {
    isEFE = FALSE;
    L_EA = U_endian32(xfe->FE.L_EA);
    L_AD = U_endian32(xfe->FE.L_AD);
  }
  // FIXME: L_EA and L_AD are assumed sane. At least prevent buffer overread.

  while (bytesRemaining > 0 && !error) {
    offset = curFileOffset;
    count  = bytesRemaining;
    // FIXME: files where InfoLengthH != 0
    if (offset < U_endian32(xfe->InfoLengthL)) {
      switch(U_endian16(xfe->sICBTag.Flags) & ADTYPEMASK) {
        case ADSHORT:
          // FIXME: efficiency: don't restart from the beginning if !firstpass
          if (isEFE) {
            exts_ptr = (const struct short_ad *)((const char *)xfe + sizeof(struct ExtFileEntry) + L_EA);
          } else {
            exts_ptr = (const struct short_ad *)((const char *)xfe + sizeof(struct FileEntry) + L_EA);
          }
          exts_end = (struct short_ad *)((char *)exts_ptr + L_AD);
          // The following while loop "eats" all unneeded extents.
          while (exts_ptr < exts_end) {
            if ((U_endian32(exts_ptr->ExtentLength.Length32) >> 30) == E_ALLOCEXTENT) {
              // Chain to (next) Allocation Extent Descriptor
              if (!AED) {
                AED = (struct AllocationExtentDesc *)malloc(blocksize);
              }
              if (AED) {
                error = ReadLBlocks(AED, U_endian32(exts_ptr->Location), part, 1);
                if (!error) {
                  error = CheckTag((struct tag *)AED, U_endian32(exts_ptr->Location), TAGID_ALLOC_EXTENT, 8, blocksize - 16);
                }
              } else {
                error = 1;
              }
              if (!error) {
                exts_ptr = (const struct short_ad *)(AED + 1);
                exts_end = exts_ptr + (U_endian32(AED->L_AD) >> 3);
                // FIXME: AED->L_AD assumed sane.  At least prevent buffer overread.
              } else {
                exts_ptr = exts_end;
              }
            } else if (offset < (U_endian32(exts_ptr->ExtentLength.Length32) & 0x3FFFFFFF)) {
            	// curFileOffset is at "offset" bytes into the current extent
                break;
            } else {
            	// Haven't reached the extent containing curFileOffset yet
                // FIXME: Terminate if ExtentLength.Length32 == 0
                offset -= U_endian32(exts_ptr->ExtentLength.Length32) & 0x3FFFFFFF;
                exts_ptr++;
            }
          }
          // Now to read from the right extent
          // FIXME: efficiency: process until bytesRemaining <= 0, error, exts_end, or chain
          // FIXME: Terminate if ExtentLength.Length32 == 0
          // FIXME: need to return all zero for blocks in extents not marked E_RECORDED
          if ((exts_ptr < exts_end) && (offset < (U_endian32(exts_ptr->ExtentLength.Length32) & 0x3FFFFFFF))) {
            sector = U_endian32(exts_ptr->Location) + (offset >> bdivshift);
            error = ReadLBlocks(fileData, sector, part, 1);
            // Note: misalignment [(startOffset % blocksize) != 0] is handled at the end of the function
            if (firstpass) {
              *data_start_loc = sector;
            }
            if (!error) {
              UINT32 blockStartOffset = offset & (blocksize - 1);
              blockBytesAvailable = blocksize - blockStartOffset;  // FIXME: assumes sane ExtentLength.Length32
              curFileOffset  += blockBytesAvailable;
              bytesRemaining -= blockBytesAvailable;
              fileData += blocksize;
            }
          } else {
            error = 1;
          }
          break;

        case ADLONG:
          // FIXME: efficiency: don't restart from the beginning if !firstpass
          if (isEFE) {
            extl_ptr = (const struct long_ad *)((const char *)xfe + sizeof(struct ExtFileEntry) + L_EA);
          } else {
            extl_ptr = (const struct long_ad *)((const char *)xfe + sizeof(struct FileEntry) + L_EA);
          }
          extl_end = (const struct long_ad *)((char *)extl_ptr + L_AD);
          // The following while loop "eats" all unneeded extents.
          while (extl_ptr < extl_end) {
            if ((U_endian32(extl_ptr->ExtentLength.Length32) >> 30) == E_ALLOCEXTENT) {
              if (!AED) {
                AED = (struct AllocationExtentDesc *)malloc(blocksize);
              }
              if (AED) {
                error = ReadLBlocks(AED, U_endian32(extl_ptr->Location_LBN),
                                    U_endian16(extl_ptr->Location_PartNo), 1);
                if (!error) {
                  error = CheckTag((struct tag *)AED, U_endian32(extl_ptr->Location_LBN),
                                   TAGID_ALLOC_EXTENT, 8, blocksize - 16);
                }
              } else {
                error = 1;
              }
              if (!error) {
                extl_ptr = (struct long_ad *)(AED + 1);
                extl_end = extl_ptr + (U_endian32(AED->L_AD) >> 4);
                // FIXME: AED->L_AD assumed sane. At least prevent buffer overread.
              } else {
                extl_ptr = extl_end;
              }
            } else if (offset < (U_endian32(extl_ptr->ExtentLength.Length32) & 0x3FFFFFFF)) {
              // curFileOffset is at "offset" bytes into the current extent
              break;
            } else {
              // Haven't reached the extent containing curFileOffset yet
              // FIXME: Terminate if ExtentLength.Length32 == 0
              offset -= U_endian32(extl_ptr->ExtentLength.Length32) & 0x3FFFFFFF;
              extl_ptr++;
            }
          }
          // Now to read from the right extent
          // FIXME: efficiency: process until bytesRemaining <= 0, error, extl_end, or chain
          // FIXME: Terminate if ExtentLength.Length32 == 0
          // FIXME: need to return all zero for blocks in extents not marked E_RECORDED
          if ((extl_ptr < extl_end) && (offset < (U_endian32(extl_ptr->ExtentLength.Length32) & 0x3FFFFFFF))) {
            sector = U_endian32(extl_ptr->Location_LBN) + (offset >> bdivshift);
            ReadLBlocks(fileData, sector, U_endian16(extl_ptr->Location_PartNo), 1);
            // Note: misalignment [(startOffset % blocksize) != 0] is handled at the end of the function
            if (firstpass) {
              *data_start_loc = sector;
            }
            if (!error) {
              UINT32 blockStartOffset = offset & (blocksize - 1);
              blockBytesAvailable = blocksize - blockStartOffset;  // FIXME: assumes sane ExtentLength.Length32
              curFileOffset  += blockBytesAvailable;
              bytesRemaining -= blockBytesAvailable;
              fileData += blocksize;
            }
          } else {
            error = 1;
          }
          break;
    
        case ADNONE:
          // @todo Why qualify with !offset?
          // FIXME: files where InfoLengthH != 0
          if ((L_AD != U_endian32(xfe->InfoLengthL)) && !offset) {
            printf("**Embedded data error: L_AD = %u, Information Length = %u\n",
                   L_AD, U_endian32(xfe->InfoLengthL));
          }
          // FIXME: InfoLength assumed sane. At least prevent buffer overread.
          // TODO: Use of MAX() here is debatable, L_AD and InfoLength should match
          blockBytesAvailable = MAX(L_AD, U_endian32(xfe->InfoLengthL));
          if (offset < blockBytesAvailable) {
            const char *emb_data = (const char *)xfe + L_EA;
            emb_data += isEFE ? sizeof(struct ExtFileEntry)
                              : sizeof(struct FileEntry);
            *data_start_loc = U_endian32(xfe->sTag.uTagLoc);

            // Note: misalignment (startOffset > 0) is handled at the end of the function
            memcpy(buffer, emb_data, blockBytesAvailable);
            bytesRemaining -= blockBytesAvailable - curFileOffset;
            curFileOffset += blockBytesAvailable - curFileOffset;
          } else {
            error = 1;
          }
          break;
      }
    } else {
      // Attempted read beyond EOF
      error = 1;
    }
    firstpass = FALSE;
  }

  if (AED)
    free(AED);

  // BUG: must use memmove() when src & dst overlap
  memcpy(buffer, buffer + (startOffset & (blocksize - 1)), bytesRequested - bytesRemaining);
  if (bytesRemaining < 0) bytesRemaining = 0;
  return bytesRemaining;
}

