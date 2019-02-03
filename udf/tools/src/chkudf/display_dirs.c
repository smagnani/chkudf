#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/*
 *  Read the FSD and get the root directory ICB address
 */

int GetRootDir(void)
{

  struct FileSetDesc *FSDPtr;
  int i, error, result;

  error = ERR_NO_FSD;
  FSDPtr = (struct FileSetDesc *)malloc(blocksize);
  if (FSDPtr) {
    track_filespace(U_endian16(FSD.Location_PartNo), U_endian32(FSD.Location_LBN),
                    EXTENT_LENGTH(FSD.ExtentLengthAndType));

    for (i = 0; i < EXTENT_LENGTH(FSD.ExtentLengthAndType) >> bdivshift; i++) {
      result = ReadLBlocks(FSDPtr, U_endian32(FSD.Location_LBN) + i, 
                           U_endian16(FSD.Location_PartNo), 1);
      if (!result) {
        result = CheckTag((struct tag *)FSDPtr, U_endian32(FSD.Location_LBN) + i, 
                          TAGID_FSD, 496, 496);
        DumpError();
        if (result < CHECKTAG_OK_LIMIT) {
          RootDirICB = FSDPtr->sRootDirICB;
          StreamDirICB = FSDPtr->sStreamDirICB;
          if ((UDF_Version == 3) && EXTENT_LENGTH(StreamDirICB.ExtentLengthAndType)) {
            // @todo Real traversal of stream directory
            // Code below is a hack to avoid reporting space table mismatch
            // on filesystems having a stub stream directory
            track_filespace(U_endian16(StreamDirICB.Location_PartNo),
                            U_endian32(StreamDirICB.Location_LBN),
							EXTENT_LENGTH(StreamDirICB.ExtentLengthAndType));
          }
          error = 0;
          if (EXTENT_LENGTH(FSDPtr->sNextExtent.ExtentLengthAndType)) {
            printf("  Found another FSD extent.\n");
            FSD = FSDPtr->sNextExtent;
            i = -1;
          }
        } else {  /* All zeros, terminator, anything but an FSD */
          break;
        }
      } else {  /* Unreadable/blank block */
        break;
      }
    }
    free(FSDPtr);
  } else {
    printf("**Couldn't allocate memory for loading FSD.\n");
    return ERR_NO_FSD;
  }
  return error;
}

/*
 *  Display a directory hierarchy
 */ 

int DisplayDirs(void)
{
  unsigned int offs[MAX_DEPTH + 1];   // Offset into current directory data
  uint32_t     addr[MAX_DEPTH + 1];   // Address of ICB of current dir
  uint16_t     part[MAX_DEPTH + 1];   // Partition of ICB of current dir
  int depth, i, error;
  struct FileIDDesc *File = NULL;     // Directory entry for the current file
  struct FE_or_EFE  *ICB  = NULL;     // ICB for current directory

  printf("\n--File Space report:\n");

  GetRootDir();

  do {
    uint32_t address = U_endian32(RootDirICB.Location_LBN);
    uint16_t partition = U_endian16(RootDirICB.Location_PartNo);

    if (!EXTENT_LENGTH(RootDirICB.ExtentLengthAndType)) {
      printf("  No root directory.\n");
      break;
    }

    printf("\nDisplaying directory hierarchy:\n%04x:%08x: \\", partition, address);
    File = (struct FileIDDesc *)malloc(blocksize);
    ICB = (struct FE_or_EFE *)malloc(blocksize);
    if (!File || !ICB) {
      printf("**Couldn't allocate space for FID buffer.\n");
      break;
    }

    depth = 1;
    offs[depth] = 0;
    addr[depth] = address;
    part[depth] = partition;
    error = read_icb(ICB, part[depth], addr[depth],
                     EXTENT_LENGTH(RootDirICB.ExtentLengthAndType), 0, 0);
    if (error)
      break;

    Num_Dirs++;  // We have to count the root directory ourselves

    printf("\n");
    do {
      printf("ICB %x:%05x offset %4x\n", part[depth], addr[depth], offs[depth]);
      if (offs[depth] >= U_endian32(ICB->InfoLengthL)) {
        for (i = 1; i <= depth; i++) printf("   ");
        printf("++End of directory\n");
        depth--;
      } else {
    	// @todo Consider warning if offs[depth] is less than sizeof(struct tag)
        //     from the end of a block. See UDF2.01 sec. 2.3.4.4.
        error = GetFID(File, ICB, part[depth], offs[depth]);
        if (!error) {
          for (i = 0; i < depth; i++) printf("   ");
          if (File->Characteristics & DIR_ATTR) {
            printf("+");
          } else {
            printf("-");
          }
          if (File->Characteristics & PARENT_ATTR) {
            printf("%04x:%08x: [parent] ", U_endian16(File->ICB.Location_PartNo), 
                                           U_endian32(File->ICB.Location_LBN));
            if (File->L_FI) {
              printf("ILLEGAL NAME ");
              printDchars((uint8_t *)File + FILE_ID_DESC_CONSTANT_LEN + U_endian16(File->L_IU), File->L_FI);
            } else {
              printf("NAME OK");
            }
            if (depth == 1 && ((U_endian16(File->ICB.Location_PartNo) != part[depth]) ||
                               (U_endian32(File->ICB.Location_LBN)    != addr[depth]))) {
              printf(" BAD PARENT OF ROOT (should be %04x:%08x", part[depth], addr[depth]);
            } else if (depth > 1 && ((U_endian16(File->ICB.Location_PartNo) != part[depth - 1]) ||
                      (U_endian32(File->ICB.Location_LBN)    != addr[depth - 1]))) {
              printf(" BAD PARENT (should be %04x:%08x", part[depth - 1], addr[depth - 1]);
            } else {
              printf(" parent location OK");
            }
            read_icb(ICB, U_endian16(File->ICB.Location_PartNo), U_endian32(File->ICB.Location_LBN),
                     EXTENT_LENGTH(File->ICB.ExtentLengthAndType), 1, File->Characteristics);
          } else {
            printf("%04x:%08x: ", U_endian16(File->ICB.Location_PartNo), U_endian32(File->ICB.Location_LBN));
            /*
             * Note: the following makes the assumption that a deleted file is no
             * longer allocated.  THIS IS WRONG according to the spec, but most
             * implementations do it this way.  This should instead be a check for
             * an extent length of zero for the ICB.
             */
            if (File->Characteristics & DELETE_ATTR) {
              printf("[DELETED] ");
              printDchars((uint8_t *)File + FILE_ID_DESC_CONSTANT_LEN + U_endian16(File->L_IU), File->L_FI);
            } else {
              printDchars((uint8_t *)File + FILE_ID_DESC_CONSTANT_LEN + U_endian16(File->L_IU), File->L_FI);
              read_icb(ICB, U_endian16(File->ICB.Location_PartNo), U_endian32(File->ICB.Location_LBN),
                       EXTENT_LENGTH(File->ICB.ExtentLengthAndType), 1, File->Characteristics);
              checkICB(ICB, File->ICB, File->Characteristics & DIR_ATTR);
            }
          }
          printf("\n");
          DumpError();
          offs[depth] += (FILE_ID_DESC_CONSTANT_LEN + File->L_FI + U_endian16(File->L_IU) + 3) & ~3;
          if ((File->Characteristics & DIR_ATTR) && 
              ! (File->Characteristics & PARENT_ATTR) &&
              ! (File->Characteristics & DELETE_ATTR)) {
            if (depth < MAX_DEPTH) {
              depth++;
              offs[depth] = 0;
              addr[depth] = U_endian32(File->ICB.Location_LBN);
              part[depth] = U_endian16(File->ICB.Location_PartNo);
            } else {
              for (i = 0; i <= depth; i++) printf("   ");
              printf(" +more subdirectories (not displayed)\n");
            }
          }
        } else {
          printf("**Error in directory\n");
          DumpError();
          depth--;
        }
      }
      /*
       * get the right ICB back.  We lost the length, but don't need it since
       * this ICB has been seen before.  So we don't disrupt the counts, we
       * claim no FID is identifying this ICB.
       */
      if (depth > 0) {
        read_icb(ICB, part[depth], addr[depth], blocksize, 0, 0);
      }
    } while (depth > 0);

  } while (0);

  free(File);
  free(ICB);

  return 0;
}

/**
 * @param[out] FID       Where to put File Identifier descriptor
 * @param[in]  fe        ICB of the directory containing the FID of interest
 * @param[in]  part      Which partition the directory is part of
 * @param[in]  offset    Number of bytes into the directory data where FID of interest
 *                       begins
 */
int GetFID(struct FileIDDesc *FID, const struct FE_or_EFE *fe, uint16_t part,
           unsigned int offset)
{
  unsigned int bytesRead;
  uint32_t location;
  
  bytesRead = ReadFileData(FID, fe, part, offset, blocksize, &location);
  if (bytesRead > FILE_ID_DESC_CONSTANT_LEN) {
    CheckTag((struct tag *)FID, location, TAGID_FILE_ID, 0, bytesRead - sizeof(struct tag));
    if (Error.Code == ERR_TAGLOC) {
      printf("** Wrong Tag Location. Expected %lld, Found %lld (%u)\n",
             Error.Expected, Error.Found, location);
      FID_Loc_Wrong++;
      Error.Code = 0;
    }
    if (Error.Code == ERR_CRC_LENGTH) {
      DumpError();
    }
    return Error.Code;
  } else {
    return ERR_READ;
  }
}

