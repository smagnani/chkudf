// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999-2001 Ben Fennema. All rights reserved.

#include "nsr.h"
#include "chkudf.h"
#include "protos.h"
#include <stdio.h>

void initialize(void) 
{
  int i;

  for (i = 0; i < NUM_CACHE; i++) {
    Cache[i].Buffer = NULL;
    Cache[i].Address = 0;
    Cache[i].Count = 0;
    Cache[i].Allocated = 0;
  }
}
