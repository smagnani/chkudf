// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999 Rob Simms. All rights reserved.

#include <stdio.h>
#include "chkudf.h"
#include "protos.h"

int TestLinkCount(void)
{
  uint32_t i;

  printf("\n--Testing link counts.\n");

  for (i = 0; i < ICBlist_len; i++) {
    if (ICBlist[i].Link != ICBlist[i].LinkRec) {
      printf("**ICB at %04x:%08x has a link count of %u, found %u link%s.\n",
             ICBlist[i].Ptn, ICBlist[i].LBN, ICBlist[i].LinkRec, 
             ICBlist[i].Link, ICBlist[i].Link == 1 ? "" : "s");
    }
  }

  return 0;
}
