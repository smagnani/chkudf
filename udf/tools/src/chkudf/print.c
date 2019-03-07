// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2019 Digital Design Corporation. All rights reserved.

#include <stdarg.h>
#include <stdio.h>

#include "chkudf.h"
#include "protos.h"

int Debug(const char* format, ...)
{
  int charsPrinted = 0;
  if (g_bDebug) {
    va_list args;
    va_start(args, format);

    charsPrinted = vprintf(format, args);
    va_end(args);
  }

  return charsPrinted;
}

int Verbose(const char* format, ...)
{
  int charsPrinted = 0;
  if (g_bVerbose) {
    va_list args;
    va_start(args, format);

    charsPrinted = vprintf(format, args);
    va_end(args);
  }

  return charsPrinted;
}

int Information(const char* format, ...)
{
  int charsPrinted = 0;

  va_list args;
  va_start(args, format);

  charsPrinted = vprintf(format, args);
  va_end(args);

  return charsPrinted;
}

int OperationalError(const char* format, ...)
{
  int charsPrinted = 0;
  g_exitStatus |= EXIT_OPERATIONAL_ERROR;

  va_list args;
  va_start(args, format);

  charsPrinted = vprintf(format, args);
  va_end(args);

  return charsPrinted;
}

int MinorError(const char* format, ...)
{
  int charsPrinted = 0;
  g_exitStatus |= EXIT_MINOR_UNCORRECTED_ERRORS;

  va_list args;
  va_start(args, format);

  charsPrinted = vprintf(format, args);
  va_end(args);

  return charsPrinted;
}

int UDFError(const char* format, ...)
{
  int charsPrinted = 0;
  g_exitStatus |= EXIT_UNCORRECTED_ERRORS;

  va_list args;
  va_start(args, format);

  charsPrinted = vprintf(format, args);
  va_end(args);

  return charsPrinted;
}

int UDFErrorIf(bool bError, const char* format, ...)
{
  int charsPrinted = 0;

  va_list args;
  va_start(args, format);

  if (bError) {
    g_exitStatus |= EXIT_UNCORRECTED_ERRORS;
    charsPrinted = printf("**");
  }

  if (bError || g_bVerbose) {
    charsPrinted += vprintf(format, args);
  }
  va_end(args);

  return charsPrinted;
}
