// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999 Rob Simms. All rights reserved.

#include <sys/types.h>
#include <sys/stat.h>
#include <blkid/blkid.h>     // libblkid
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include "nsr.h"
#include "chkudf.h"
#include "protos.h"

void SetSectorSize(void)
{
  char   *buffer;
  char   *avdpbuf;
  int    result, found;
  struct stat fileinfo;
  bool bFullDiskBlockDevice = false;

  secsize = 0;

  if ((fstat(device, &fileinfo) == 0) && S_ISBLK(fileinfo.st_mode)) {
    char diskname[32];
    dev_t fullDiskDev = 0;
    result = blkid_devno_to_wholedisk(fileinfo.st_rdev, diskname,
                                      sizeof(diskname), &fullDiskDev);
    if (result == 0) {
      bFullDiskBlockDevice = (fileinfo.st_rdev == fullDiskDev);
    }
  }
  if (bFullDiskBlockDevice) {
    /*
     * The device is a block device, let's try some SCSI stuff...
     */
    buffer = malloc(128);
    if (buffer) {
      memset(cdb, 0, 12);
      cdb[0] = 0x12;
      cdb[4] = 44;
      result = do_scsi(cdb, 6, buffer, 44, 0, sensedata, sensebufsize);
      if (!result) {
        /*
         * INQUIRY worked
         */
        scsi = 1;       // SCSI commands work on this device
        buffer[36] = 0;
        printf("  Device is: '%s' (type %d)\n", buffer + 8, buffer[0] & 0x1f);
        if ((buffer[0] & 0x1f) == 5) {  // Test for CD/DVD
          isType5 = true;
          secsize = 2048;
          printf("  Setting sector size to %u for CD/DVD device.\n", secsize);
        } else {
          memset(cdb, 0, 12);
          cdb[0] = 0x25;
          result = do_scsi(cdb, 10, buffer, 8, 0, sensedata, sensebufsize);
          if (!result) {
            secsize = S_endian32(*(uint32_t *)(buffer + 4));
            printf("  READ CAPACITY reports a sector size of %u (0x%x).\n", secsize, secsize);
          }
          if (!secsize) {
            scsi_modesense10(cdb, 0, 0, 0, 0);      // Get block descriptor
            result = do_scsi(cdb, 10, buffer, 16, 0, sensedata, sensebufsize);
            if (!result) {
              /*
               * MODE SENSE worked
               */
              if (S_endian16(*(uint16_t *)(buffer + 6)) == 8) {
                /*
                 * MODE SENSE returned a block descriptor
                 */
                secsize = *(int *)(buffer + 12);
                secsize = S_endian32(secsize) & 0x00ffffff;
                printf("  Mode Sense shows %u (0x%x) byte sectors.\n", secsize, secsize);
              }  //if (block descriptor)
            }  //if (mode sense worked)
          }
        }  //if (not a CD)
      }  //if (scsi device)
      free(buffer);
    }
  }
  if (secsize == 0) {                 /* Block size still not set */
    secsize = 0x200;
    found = false;
    avdpbuf = malloc(MAX_SECTOR_SIZE);
    if (avdpbuf) {
      secsize >>= 1;    // Precompensate for initial 'secsize <<= 1'
      while ((secsize < MAX_SECTOR_SIZE) && !found) {
        secsize <<= 1;
        lseek(device, 256 * secsize, SEEK_SET);
      
        result = read(device, avdpbuf, secsize);
        if (result == -1) {
          printf("\n**Read error #%d\n", errno);
        } else {
          found = !CheckTag((struct tag *)avdpbuf, 256, 2, 0, MAX_SECTOR_SIZE);
          ClearError();
          if (!found) {
            found = !CheckTag((struct tag *)avdpbuf, 512, 2, 0, MAX_SECTOR_SIZE);
            ClearError();
            if (found) {
              secsize >>= 1;
            }
          }
        }
      }
      if (found) {
        printf("  Guessing revealed %u byte sector size.\n", secsize);
      } else {
        secsize = 0;
      }
      free(avdpbuf);
    }
    if (secsize == 0) {
      secsize = 0x800;
      printf("**Guessing failed - assuming %u byte sector size.\n", secsize);
    }
  }

  if (secsize == 0) secsize = 1;
  while (!((1 << sdivshift) & secsize)) {
    sdivshift++;
  }
}
