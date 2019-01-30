#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include "protos.h"

/*
 * Generic SCSI command processor.  The identification of the device is
 * done elsewhere, and assumed to be global.  In the case of the Linux
 * implementation, the device identification is a file handle kept in 
 * the "device" global.
 */
bool do_scsi(uint8_t *command, int cmd_len, void *buffer, uint32_t in_len,
             uint32_t out_len, uint8_t *sense, int sense_len)
{
  uint32_t   *ip;
  uint32_t   buffer_needed;
  bool     fail = true;
  int      result;

  buffer_needed = MAX(in_len, out_len) + 8 + cmd_len;
  if (buffer_needed > scsibufsize) {
    scsibuf = realloc(scsibuf, buffer_needed);
  }
  if (scsibuf) {
    memset(scsibuf, 0, buffer_needed);
    ip = (uint32_t *)scsibuf;
    ip[0] = out_len;
    ip[1] = in_len;
    memcpy(scsibuf + 8, command, cmd_len);
    memcpy(scsibuf + 8 + cmd_len, buffer, out_len);
    result = ioctl(device, 1, scsibuf);
    if (result == 0) {
      memcpy(buffer, scsibuf + 8, in_len);
      fail = false;
    } else {
      if (result == 0x28000000) {
        memcpy(sense, scsibuf + 8, sense_len);
        printf("**SCSI error %x/%02x/%02x**", sense[2] & 0xf, 
               (unsigned int)sense[12], (unsigned int)sense[13]);
      } else if (result == 0x25040000) {
        printf("SCSI error - can't talk to drive.\n");
      } else if (result < 0) {
        if (result == -EPERM)
          printf("  SCSI access not permitted.\n");
        else
          printf("ioctl error %d.\n", -result);
      } else {
        printf("Unknown ioctl error 0x%08x.\n", (unsigned int) result);
      }
    }
  } //if scsibuf allocated
  return fail;
}
