#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include "protos.h"

void die_usage(const char* myName)
{
    fprintf(stderr, "**Usage: %s [-n|-y] [-v] device_or_file\n", myName);
    exit(EXIT_USAGE);
}

int main(int argc, char **argv)
{
  char   *devname;
  struct stat fileinfo;
  int opt;

/*
 * Initialize cache management structures
 */
  initialize();

  while ((opt = getopt(argc, argv, "ny")) != -1) {
    switch (opt) {
      case 'n':
      case 'y':
        if (g_defaultAnswer) {
          fprintf(stderr, "**Only one of -n or -y may be specified.\n");
          die_usage(argv[0]);
        }
        g_defaultAnswer = (char) opt;
        break;

      default:
        die_usage(argv[0]);
        break;
     }
   }

/*
 * Find the name of the file or device we're talking to
 */
  if (optind < argc) {
    devname = argv[optind];
  } else {
    die_usage(argv[0]);
  }

  device = open(devname, O_RDONLY);

  if (device > 0) {
    printf("--Determining device/media parameters.\n");
    SetSectorSize();
    SetLastSector();
    if (LastSector == -1) {
      fstat(device, &fileinfo);
      LastSector = (fileinfo.st_size >> sdivshift) - 1;
    }
    printf("  Last Sector = %u (0x%x) and is%s accurate\n", LastSector,
           LastSector, LastSectorAccurate ? "" : " not");
    if (!LastSectorAccurate) {
      SetLastSectorAccurate();
    }
    if (isType5) {
      SetFirstSector();
    }
    Check_UDF();
    cleanup();
    close(device);
  } else {
    printf("**Can't open %s (error %d)\n", devname, errno);
  }
  return 0;
}
