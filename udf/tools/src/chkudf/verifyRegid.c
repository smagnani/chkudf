// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 1999 Rob Simms. All rights reserved.

#include "nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

int CheckRegid(const struct udfEntityId *reg, const char *ID)
{
  int error = 0;

  if (!reg) {
    error = 1;
  }
  if (strncmp((const char*)reg->aID, ID, 23)) {
    error = 1;
  }
  if (reg->uOSClass > OSCLASS_WINCE) {
    error = 1;
  }
  if ((U_endian16(reg->uUDFRevision) < 0x100) || (U_endian16(reg->uUDFRevision) > 0x201)) {
    error = 1;
  }
  return error;
}

/********************************************************************/
/* Display only the first part of a regid                           */
/********************************************************************/
void DisplayRegIDID( struct regid *RegIDp)
{
  /* assumes character is positioned */
  /* Make a null-terminated version of the Identifier field. */
  char id[24];
  memcpy(id,RegIDp->aRegisteredID,23);
  id[23] = '\000';

  if ( RegIDp->uFlags & DIRTYREGID )
    printf("(Dirty **NON-UDF**)     ");
  if ( RegIDp->uFlags & PROTECTREGID )
    printf("(Protected **NON-UDF**) ");

  printf("'%.23s'",id);
}

void printOSInfo( uint8_t osClass, uint8_t osIdentifier )
{
  printf(" OS: %u,%u",
         osClass,osIdentifier);
  switch (osClass) {
    case OSCLASS_UNDEF: printf(" (Undefined)");  break;
    case OSCLASS_DOS:   printf(" (DOS)");        break;
    case OSCLASS_OS2:   printf(" (OS/2)");       break;
    case OSCLASS_MAC:   printf(" (Macintosh)");  break;
    case OSCLASS_UNIX:  printf(" (UNIX)");       break;
    case OSCLASS_WIN9x: printf(" (Windows 9x)"); break;
    case OSCLASS_WINNT: printf(" (Windows NT)"); break;
    case OSCLASS_OS400: printf(" (Windows NT)"); break;
    case OSCLASS_BEOS:  printf(" (BeOS)");       break;
    case OSCLASS_WINCE: printf(" (Windows CE)"); break;
    default:            printf(" (Illegal) ** NON-UDF **");
  }

  if (osClass == OSCLASS_UNIX) {
    switch (osIdentifier) {
      case OSID_GENERIC:     printf(" (Undefined)"); break;
      case OSID_IBM_AIX:     printf(" AIX");         break;
      case OSID_SUN_SOLARIS: printf(" Solaris");     break;
      case OSID_HPUX:        printf(" HPUX");        break;
      case OSID_SGI_IRIX:    printf(" SGI_Irix");    break;
      case OSID_LINUX:       printf(" Linux");       break;
      case OSID_MKLINUX:     printf(" MkLinux");     break;
      case OSID_FREEBSD:     printf(" FreeBSD");     break;
      case OSID_NETBSD:      printf(" NetBSD");      break;
      default:               printf(" (Unknown) ** NON-UDF **");
    }
  } else if (osClass == OSCLASS_MAC) {
    switch (osIdentifier) {
      case 0:                printf(" (pre-OS X)");    break;
      case 1:                printf(" (OS X)");        break;
      default:               printf(" (Unknown) ** NON-UDF **");
    }
  } else {
    if (osIdentifier != 0)
      printf("\n  ** NON-UDF **");
  }
}

/**************************/
/* Display a UDF EntityID */
/**************************/
void DisplayUdfID(struct udfEntityId * ueip)
{
  int i;
  /* Display the Identifier and flags field first */
  DisplayRegIDID((struct regid *)ueip);

  /* Then display the suffix */
  printf(", UDF Ver.: %02x.%02x",
         (U_endian16(ueip->uUDFRevision) & 0xff00) >> 8, U_endian16(ueip->uUDFRevision) & 0xff);
  printOSInfo(ueip->uOSClass,ueip->uOSIdentifier);
  if (*(ueip->aReserved) != 0) {
    printf(", Reserved: ");
    for (i = 0; i < 4; i++) {
      printf("%02x ", ueip->aReserved[i]);
    }
  }
  printf("\n");
}

/********************************/
/* Display an Application ID    */
/********************************/
void DisplayAppID(struct regid *pAppID)
{
  DisplayRegIDID(pAppID);
  printf("\n");
}

/********************************/
/* Display an implementation ID */
/********************************/

void DisplayImplID(struct implEntityId * ieip)
{
  int i;
  /* Display the Identifier and flags field first */
  DisplayRegIDID((struct regid *)ieip);

  /* Then display the suffix */
  printOSInfo(ieip->uOSClass,ieip->uOSIdentifier);
  if (*(ieip->uImplUse) != 0) {
    printf(", Impl. use: ");
    for (i = 0; i < 6; i++) {
      printf("%02x ", ieip->uImplUse[i]);
    }
  }
  printf("\n");
}
