// SPDX-License-Identifier: MIT
// © Copyright 1996 Hewlett-Packard Development Company, L.P

#ifndef __UDFH__
#define __UDFH__

#define UDF_DOMAIN_ID           "*OSTA UDF Compliant"
#define UDF_CHARSPEC            "OSTA Compressed Unicode"

#define N_REGID_NSR       0
#define N_REGID_IUVD      1
#define N_REGID_FREE_EA   2
#define N_REGID_OS2_EA    3
#define N_REGID_OS2_EAL   4
#define N_REGID_MAC_VOL   5
#define N_REGID_MAC_FIND  6
#define N_REGID_MAC_UNQ   7
#define N_REGID_MAC_RES   8
#define N_REGID_CD_VP     9
#define N_REGID_VAT      10
#define N_REGID_SPARE    11
#define N_REGID_CD_SP    12

#define E_REGID_NSR      "+NSR02"
#define E_REGID_IUVD     "*UDF LV Info"
#define E_REGID_FREE_EA  "*UDF FreeEASpace"
#define E_REGID_OS2_EA   "*OS/2 EA"
#define E_REGID_OS2_EAL  "*OS/2 EALength"
#define E_REGID_MAC_VOL  "*UDF Mac VolumeInfo"
#define E_REGID_MAC_FIND "*UDF Mac FinderInfo"
#define E_REGID_MAC_UNQ  "*UDF Mac UniqueIDTable"
#define E_REGID_MAC_RES  "*UDF Mac ResourceFork"
#define E_REGID_CD_VP    "*UDF Virtual Partition"
#define E_REGID_VAT      "*UDF Virtual Alloc Tbl"
#define E_REGID_SPARE    "*UDF Sparing Table"
#define E_REGID_CD_SP    "*UDF Sparable Partition"

#define OSCLASS_UNDEF    0
#define OSCLASS_DOS      1
#define OSCLASS_OS2      2
#define OSCLASS_MAC      3
#define OSCLASS_UNIX     4
#define OSCLASS_WIN9x    5
#define OSCLASS_WINNT    6
#define OSCLASS_OS400    7
#define OSCLASS_BEOS     8
#define OSCLASS_WINCE    9

#define OSID_GENERIC     0
#define OSID_IBM_AIX     1
#define OSID_SUN_SOLARIS 2
#define OSID_HPUX        3
#define OSID_SGI_IRIX    4
#define OSID_LINUX       5
#define OSID_MKLINUX     6
#define OSID_FREEBSD     7
#define OSID_NETBSD      8

/* UDF 2.2.10 Virtual Allocation Table -------------------------*/
#define FILE_TYPE_VAT             248  /* UDF 2.00 and later */

/* UDF 2.2.11 Sparing Table for CD-RW --------------------------*/
struct SparingTable {
    struct tag sTag;       /* uTagID = 0 */
    struct udfEntityId sEntityId;
    uint16_t uRT_L;
    uint8_t  Reserved[2];
    uint32_t uSequence;
};


#endif
