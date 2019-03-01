#include <stdio.h>
#include "chkudf.h"
#include "../nsrHdrs/nsr.h"
#include "protos.h"

void DumpError(void)
{
  if (Error.Code > 0) {
    printf("**[%08x] ", Error.Sector);
    printf(Error_Msgs[Error.Code - 1].format, Error.Expected, Error.Found);
    printf(".\n");

    g_exitStatus |= Error_Msgs[Error.Code - 1].exitCode;
  }
  ClearError();
}

void ClearError(void)
{
  Error.Code     = ERR_NONE;
  Error.Sector   = 0;
  Error.Expected = 0;
  Error.Found    = 0;
}
