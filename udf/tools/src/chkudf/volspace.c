// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999 Rob Simms. All rights reserved.

#include <stdio.h>
#include "chkudf.h"
#include "protos.h"

int track_volspace(uint32_t Location, uint32_t Length, char *Name)
{
  int i, j;
  bool error;

  error = false;

  if (Length > 0) {  
    //point to the location where the new extent is >= the list member
    for (i = 0; (i < VolSpaceListLen) && (Location > VolSpace[i].Location); i++);
    if (i > 0) {
      if (((Location + Length - 1) >= VolSpace[i-1].Location) && 
          (Location < (VolSpace[i-1].Location + VolSpace[i-1].Length))) {
        Error.Code = ERR_VOL_SPACE_OVERLAP;
        Error.Sector = Location;
        error = true;
      }
    }
    if (i < VolSpaceListLen) {
      if (((Location + Length - 1) >= VolSpace[i].Location) && 
          (Location < (VolSpace[i].Location + VolSpace[i].Length))) {
        Error.Code = ERR_VOL_SPACE_OVERLAP;
        Error.Sector = Location;
        error = true;
      }
    }
    if (VolSpaceListLen < MAX_VOL_EXTS - 1) {
      for (j = VolSpaceListLen; j > i; j--) {
        VolSpace[j] = VolSpace[j - 1];
      }
      VolSpace[i].Location = Location;
      VolSpace[i].Length = Length;
      VolSpace[i].Name = Name;
      VolSpaceListLen++;
    } else {
      printf("**Too many volume extents.");
    }
  //  printf("\nVolume Allocation list:\n");
  //  for (i = 0; i < VolSpaceListLen; i++) {
  //    printf("  %8u %u\n", VolSpace[i].Location, VolSpace[i].Length);
  //  }
    if (error) {
      DumpError();
    }
  }
  return error;
}
