#include <sys/types.h>
#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <linux/cdrom.h>
#include "chkudf.h"
#include "protos.h"

/* The following routine attempts to find the true last block based on
 * the last AVDP.  This is only needed on CD media.
 */

bool Get_First_RTI()
{
  uint8_t  *buffer;
  int      result = 0;
  int      *ip;
  bool     success = false;
  uint8_t  first_track, track_no, target_session;

  buffer = malloc(128);
  ip = (int *)buffer;
  if (buffer) {
    memset(cdb, 0, 12);      /* Clear the request buffer */
    cdb[0] = 0x51;           /* Generic Read Disk Info */
    cdb[8] = 32;             /* Allocation length in CDB */
    result = do_scsi(cdb, 10, buffer, 32, 0, sensedata, sensebufsize);
    if (!result) {
      first_track = buffer[3];
      track_no = buffer[5];
      printf("  Generic Read Disc Info worked; first track in last session is %u.\n", track_no);
      memset(cdb, 0, 12);
      cdb[0] = 0x52;       /* Read Track Information          */
      cdb[1] = 1;          /* For a track number              */
      cdb[5] = track_no;   /* The first track in last session */
      cdb[8] = 36;         /* Allocation Length               */
      result = do_scsi(cdb, 10, buffer, 36, 0, sensedata, sensebufsize);
      if (!result) {
        if (buffer[6] & 0x40) {
          /*
           * Track is blank; we want the one from the previous session
           */
          target_session = buffer[3] - 1;
          if (target_session > 0) {
            printf("Session %u is blank; going back to Session %u.\n",
                   target_session + 1, target_session);
            while ((track_no > first_track) && (buffer[3] >= target_session)) {
              track_no--;            
              cdb[5] = track_no;
              result = do_scsi(cdb, 10, buffer, 36, 0, sensedata, sensebufsize);
              if (!result && (buffer[3] == target_session)) {
                lastSessionStartLBA = S_endian32(ip[2]);
                printf("  Generic RDI/RTI:  Session %u, track %u, start %u.\n",
                       buffer[3], track_no, lastSessionStartLBA);
                success = true;
              }
            }
          }
        } else {
          /*
           * Track is recorded.  Use it.
           */
          lastSessionStartLBA = S_endian32(ip[2]);
          printf("  Generic RDI/RTI worked.  Last session starts at %u.\n", lastSessionStartLBA);
          success = true;
        }
      }
    }
    free(buffer);
  }
  return success;
}

void SetFirstSector(void)
{
  lastSessionStartLBA = 0;
  Get_First_RTI();
}
