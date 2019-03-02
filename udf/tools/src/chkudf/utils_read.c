// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999-2001 Ben Fennema. All rights reserved.
// Copyright (c) 2019 Digital Design Corporation. All rights reserved.

#define _LARGEFILE64_SOURCE    // lseek64()
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "nsr.h"
#include "chkudf.h"
#include "protos.h"

/* Cache everything in units of packet_size.  packet_size will be filled
 * in for all media, packet or not.
 * Subtle point: the caching obviates any need for 'buffer' to have any special alignment
 *
 * WARNING: pointer returned by CacheSectors() is only guaranteed valid until next
 *          call to CacheSectors
 */
static const void* CacheSectors(uint32_t address, uint32_t Count)
{
  int readOK, result, numsecs, i;
  void *curbuffer;

  //printf("  Reading sector %u.\n", address);
  readOK = false;

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
        readOK = true;
        for (i = 0; i < Count; i++) {
          scsi_read10(cdb, address + i, 1, secsize, 0, 0, 0);
          result = do_scsi(cdb, 10, Cache[bufno].Buffer + i * secsize,
                           secsize, 0, sensedata, sensebufsize);
          if (result) {
            readOK = false;
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
      curbuffer = NULL;
    }
  }

  return curbuffer;
}

int ReadSectors(void *buffer, uint32_t address, uint32_t Count)
{
    const void *cachedBuf = CacheSectors(address, Count);
    if (cachedBuf) {
      memcpy(buffer, cachedBuf, Count << sdivshift);
    }

    return (cachedBuf == NULL);
}

/**
 * Pull data from a range of partition logical blocks into a contiguous cache buffer.
 *
 * @param[in]  p_address   Partition-relative address of the first block to cache
 * @param[in]  p_ref       Index of the partition where blocks reside
 * @param[in]  Count       Number of logical blocks to fetch.
 *                         Must not be larger than 1 if the partition has a Virtual
 *                         or Sparable Partition Map because sequential partition blocks
 *                         may reside in discontiguous media sectors.
 *
 * @return     NULL        Read error, or request could not be satisfied
 *                         (see constraints spelled out under "Count")
 * @return     non-NULL    Pointer to cached block data
 */
static const uint8_t* CachePBlocks(uint32_t p_address, uint16_t p_ref, uint32_t Count)
{
  const void *cachedBuf = NULL;
  sST_desc *PM_ST;
  uint32_t i;
  uint32_t numSectors = Count * s_per_b;
  uint32_t secaddr = (p_address * s_per_b) + Part_Info[p_ref].Offs;

  if (p_ref < PTN_no) {
    switch(Part_Info[p_ref].type) {
      case PTN_TYP_REAL:
        if (p_address < Part_Info[p_ref].Len) {
          cachedBuf = CacheSectors(secaddr, numSectors);
        }
        break;

      case PTN_TYP_VIRTUAL:
        if ((p_address < Part_Info[p_ref].Len) && (Count == 1)) {
          secaddr =   (Part_Info[p_ref].Extra[p_address] * s_per_b)
                    + Part_Info[p_ref].Offs;
          cachedBuf = CacheSectors(secaddr, s_per_b);
        }
        break;

      case PTN_TYP_SPARE:
        PM_ST = (struct _sST_desc *)Part_Info[p_ref].Extra;
        if (PM_ST) {
          if (Count == 1) {
            bool spared = false;
            for (i = 0; (i < PM_ST->Size) && !spared; i++) {
              if ((p_address >= PM_ST->Map[i].Original)  &&
                  (p_address < (PM_ST->Map[i].Original + PM_ST->Extent))) {
                printf("!!Getting sector from spare area!!\n");
                spared = true;
                secaddr = Part_Info[p_ref].Extra[2*i+1];
                cachedBuf = CacheSectors(secaddr, s_per_b);
                break;
              }
            }
            if (!spared) {
              cachedBuf = CacheSectors(secaddr, s_per_b);
            }
          } // else unsupported case, since spared blocks can be discontiguous on the medium
        } else {
          // No sparing table available
          cachedBuf = CacheSectors(secaddr, numSectors);
        }
        break;

      default:
        break;
    }
  }

  return cachedBuf;
}

/* Bug: If count crosses sparing boundary... */
int ReadLBlocks(void *buffer, uint32_t p_address, uint16_t p_ref, uint32_t Count)
{
    const void *cachedBuf = NULL;
    sST_desc *PM_ST;
    uint32_t i;
    int error = 1;
    uint8_t *destBuffer = (uint8_t*) buffer;

    if (p_ref < PTN_no) {
      switch(Part_Info[p_ref].type) {
        case PTN_TYP_SPARE:
          PM_ST = (struct _sST_desc *)Part_Info[p_ref].Extra;
          if (PM_ST) {
            error = 0;
            // Sparable blocks may not be contiguous,
            // do each one independently
            for (i = 0; !error && (i < Count); i++) {
              cachedBuf = CachePBlocks(p_address + i, p_ref, 1);
              if (cachedBuf) {
                memcpy(destBuffer + (i << bdivshift), cachedBuf, blocksize);
              } else {
                error = 1;
              }
            }
            break;
          } // else no sparing table available
          // fallthrough
        case PTN_TYP_REAL:
          cachedBuf = CachePBlocks(p_address, p_ref, Count);
          if (cachedBuf) {
            memcpy(destBuffer, cachedBuf, Count << bdivshift);
            error = 0;
          }
          break;

        case PTN_TYP_VIRTUAL:
          // Virtual blocks may not be contiguous,
          // do each one independently
          error = 0;
          for (i = 0; !error && (i < Count); i++) {
            cachedBuf = CachePBlocks(p_address + i, p_ref, 1);
            if (cachedBuf) {
              memcpy(destBuffer + (i << bdivshift), cachedBuf, blocksize);
            } else {
              error = 1;
            }
          }
          break;

        default:
          break;
      }
    }

    return error;
}

/*
 * @param[out]   buffer           Data read from the file.
 *                                This buffer should have a minimum length of
 *                                ((bytesRequested + 2 * blocksize - 2) / blocksize) blocks
 *                                to avoid overrun for all (startOffset, bytesRequested) combinations.
 *
 * @param[in]    xfe              ICB describing the file
 * @param[in]    part             Which partition the file is part of
 * @param[in]    startOffset      # of bytes into the file at which to begin reading
 * @param[in]    bytesRequested   Desired number of file data bytes
 * @param[out]   data_start_loc   Sector in which the startOffset byte of the file resides,
 *                                if one has been allocated.  @todo What if not allocated?
 *
 * @return       Number of bytes read
 */
unsigned int ReadFileData(void *buffer, const struct FE_or_EFE *xfe, uint16_t part,
                          uint64_t startOffset, unsigned int bytesRequested,
                          uint32_t *data_start_loc)
{
  const char *exts_ptr, *exts_end;
  struct AllocationExtentDesc *AED = NULL;
  uint32_t           sector;    // @todo Rename - confusing b/c this is not used with ReadSectors()
  uint16_t           curPartitionIndex = part;  // Default matches short_ad case
  uint32_t           curExtentLocation;
  uint32_t           curExtentLength;
  uint32_t           curExtentType;
  uint32_t           blockBytesAvailable;
  uint64_t           infoLength;
  uint64_t           curFileOffset;
  uint64_t           offset;
  unsigned int       bytesRemaining;
  int                error;
  bool               firstpass;
  uint8_t           *fileData;
  const uint16_t     adtype = U_endian16(xfe->sICBTag.Flags) & ADTYPEMASK;

  firstpass = true;
  error = 0;
  curFileOffset = startOffset;
  bytesRemaining = bytesRequested;  // TODO: This should be reduced if xfe InfoLength is too small
  fileData = (uint8_t*) buffer;
  infoLength = U_endian64(xfe->InfoLength);

  do {
    uint32_t  L_EA, L_AD;
    size_t  xfeHeaderSize;

    if (U_endian16(xfe->sTag.uTagID) == TAGID_EXT_FILE_ENTRY) {
      L_EA = U_endian32(xfe->EFE.L_EA);
      L_AD = U_endian32(xfe->EFE.L_AD);
      xfeHeaderSize = sizeof(struct ExtFileEntry);
    } else {
      L_EA = U_endian32(xfe->FE.L_EA);
      L_AD = U_endian32(xfe->FE.L_AD);
      xfeHeaderSize = sizeof(struct FileEntry);
    }

    // @todo switch to using Error so we can report meaningful information
    if (   (L_EA > (blocksize - xfeHeaderSize))
        || (L_AD > (blocksize - xfeHeaderSize))) {
      error = 1;
      break;
    }

    if ((xfeHeaderSize + L_EA + L_AD) > blocksize) {
      error = 1;
      break;
    }

    if (adtype == ADEXTENDED) {
      error = 1;
      break;
    }

    if (adtype == ADNONE) {
      const char *emb_data;

      *data_start_loc = U_endian32(xfe->sTag.uTagLoc);
      // @todo Why qualify with !startOffset?
      if ((L_AD != infoLength) && !startOffset) {
        printf("**Embedded data error: L_AD = %u, Information Length = %" PRIu64 "\n",
               L_AD, infoLength);
      }

      blockBytesAvailable = blocksize - xfeHeaderSize - L_EA;
      if (L_AD < blockBytesAvailable) {
          blockBytesAvailable = L_AD;
      }
      if (infoLength < blockBytesAvailable) {
        blockBytesAvailable = (uint32_t) infoLength;
      }

      if (startOffset >= blockBytesAvailable) {
        // Attempted read beyond EOF
        error = 1;
        break;
      }
      blockBytesAvailable -= (uint32_t) startOffset;

      if (bytesRequested < blockBytesAvailable) {
        blockBytesAvailable = bytesRequested;
      }

      emb_data = ((const char*) xfe) + xfeHeaderSize + L_EA + (uint32_t) startOffset;

      memcpy(buffer, emb_data, blockBytesAvailable);
      bytesRemaining -= blockBytesAvailable;

      break;
    }  // adtype == ADNONE

    // At this point adtype == ADSHORT || adtype == ADLONG
    // @todo check that L_EA and L_AD are proper multiples of adsize

    while ((bytesRemaining > 0) && !error) {
      offset = curFileOffset;

      if (offset >= infoLength) {
        // Attempted read beyond EOF
        error = 1;
        break;
      }

      // FIXME: efficiency: don't restart from the beginning if !firstpass
      exts_ptr = ((const char*) xfe) + xfeHeaderSize + L_EA;
      exts_end = exts_ptr + L_AD;

      // The following while loop "eats" all unneeded extents.
      while (exts_ptr < exts_end) {
        if (adtype == ADSHORT) {
          const struct short_ad *cur_ext = (const struct short_ad *)exts_ptr;
          curExtentLength   = EXTENT_LENGTH(cur_ext->ExtentLengthAndType);
          curExtentType     = EXTENT_TYPE(cur_ext->ExtentLengthAndType);
          curExtentLocation = U_endian32(cur_ext->Location);
        } else if (adtype == ADLONG) {
          const struct long_ad *cur_ext = (const struct long_ad *)exts_ptr;
          curExtentLength   = EXTENT_LENGTH(cur_ext->ExtentLengthAndType);
          curExtentType     = EXTENT_TYPE(cur_ext->ExtentLengthAndType);
          curExtentLocation = U_endian32(cur_ext->Location_LBN);
          curPartitionIndex = U_endian16(cur_ext->Location_PartNo);
        }

        if (curExtentLength == 0) {
          // ECMA-167r3 sec. 4.12: zero extent length terminates allocation descriptors
          // @todo error if exts_ptr < exts_end "- 1"?
          // Error will get set below the 'while' loop but it might be more informative
          // to log that we found an unexpected zero-length extent
          exts_ptr = exts_end;
          break;
        }
        if (curExtentType == E_ALLOCEXTENT) {
          // Chain to (next) Allocation Extent Descriptor
          if (!AED) {
            AED = (struct AllocationExtentDesc *)malloc(blocksize);
          }
          if (AED) {
            error = ReadLBlocks(AED, curExtentLocation, curPartitionIndex, 1);
            if (!error) {
              error = CheckTag((struct tag *)AED, curExtentLocation,
                               TAGID_ALLOC_EXTENT, 8, blocksize - 16);
            }
          } else {
            error = 1;
          }
          if (!error) {
            exts_ptr = (const char*)(AED + 1);
            exts_end = exts_ptr + U_endian32(AED->L_AD);
            // FIXME: AED->L_AD assumed sane.  At least prevent buffer overread.
          } else {
            exts_ptr = exts_end;
          }
        } else if (offset < curExtentLength) {
          // curFileOffset is at "offset" bytes into the current extent
          break;
        } else {
          // Haven't reached the extent containing curFileOffset yet
          // FIXME: Terminate if curExtentLength == 0
          offset -= curExtentLength;
          exts_ptr += (adtype == ADSHORT) ? sizeof(struct short_ad)
                                          : sizeof(struct long_ad);
        }
      }
      // Now to read from the right extent
      // FIXME: efficiency: process until bytesRemaining == 0, error, exts_end, or chain
      // FIXME: Terminate if curExtentLength == 0
      // FIXME: need to return all zero for blocks in extents not marked E_RECORDED
      if ((exts_ptr < exts_end) && (offset < curExtentLength)) {
        const uint32_t offset32 = (uint32_t) offset;   // Same value - to avoid repeated casting
        uint32_t blockStartOffset = offset32 & (blocksize - 1);
        blockBytesAvailable = blocksize - blockStartOffset;  // FIXME: assumes sane curExtentLength

        if (blockBytesAvailable > (curExtentLength - offset32))
          blockBytesAvailable = curExtentLength - offset32;

        if (blockBytesAvailable > bytesRemaining)
          blockBytesAvailable = bytesRemaining;

        // Speculative - don't use resulting value if E_UNALLOCATED
        sector = curExtentLocation + (offset32 >> bdivshift);

        if (curExtentType == E_RECORDED) {
          const uint8_t *cacheBuf;

          // Note, block-at-a-time in case of sparing or virtual mapping
          cacheBuf = CachePBlocks(sector, curPartitionIndex, 1);
          memcpy(fileData, cacheBuf + blockStartOffset, blockBytesAvailable);

        } else {
          // Maybe allocated, but definitely unrecorded
          memset(fileData, 0, blockBytesAvailable);
        }

        if (firstpass && (curExtentType != E_UNALLOCATED)) {
          *data_start_loc = sector;
        }

        if (!error) {
          curFileOffset  += blockBytesAvailable;
          bytesRemaining -= blockBytesAvailable;
          fileData       += blockBytesAvailable;
        }
      } else {
        error = 1;
      }

      firstpass = false;
    }

    if (AED)
      free(AED);

  } while (0);

  return (bytesRequested - bytesRemaining);
}

