#if !defined(_LINUX_ECMA119_H)
#define _LINUX_ECMA119_H
/*
 * udf_e119.h
 *
 * PURPOSE.
 *	Definitions from the ECMA-119 standard.
 *
 * DESCRIPTION
 *	ECMA-119 is equivalent to ISO-9660. This file is actually based on
 *	ISO-9660 (1990 revision).
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL) version 2.0. Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Adapted from ISO-9660 (1990 revision).
 */

typedef __u8 digit, achar, dchar;

/* d-characters (ECMA-119 7.4.1) */
#define IS_DCHAR(X)	(((X) >= 0x30 && (X) <= 0x39)\
			|| ((X) >= 0x41 && (X) <= 0x5a)\
			|| ((X) == 0x5f))

/* a-characters (ECMA-119 7.4.1) */
#define IS_ACHAR(X)	(((X) >= 0x20 && (X) <= 0x22)\
			|| ((X) >= 0x25 && (X) <= 0x3f)\
			|| ((X) >= 0x41 && (X) <= 0x5a)\
			|| ((X) == 0x5f))

/* Separators (ECMA-119 7.4.3) */
#define E119_SEPARATOR1	'.'
#define E119_SEPARATOR2 ';'

/* Volume Descriptor Types */
#define E119_BOOT_RECORD_DESC		0x00U
#define E119_PRIMARY_VOL_DESC		0x01U
#define E119_SUPPLEMENTARY_VOL_DESC	0x02U
#define E119_VOL_PARTITION_DESC		0x03U
#define E119_VOL_DESC_SET_TERM		0xffU

/* Standard Identifier (ECMA-119 8.1.2) */
#define E119_STD_IDENT_LEN	5
#define E119_STD_IDEN		"CD001"

/* Volume Descriptor (ECMA-119 8.1) */
struct E119VolDesc {
	__u8 volDescType;
	__u8 stdIdent[E119_STD_IDENT_LEN];
	__u8 volDescVersion;
	__u8 data[2041];
};

/* Boot Record Descriptor (ECMA-119 8.2) */
struct E119BootRecordDesc {
	__u8 volDescType;
	__u8 stdIdent[5];
	__u8 volDescVersion;
	__u8 bootSystemIdent[32];
	__u8 bootIdent[32];
	__u8 BootSystemUse[1977];
};

/* Volume Descriptor Set Terminator (ECMA-119 8.3) */
struct E119VolDescSetTerminator {
	__u8 volDescType;
	__u8 stdIdent[5];
	__u8 volDescVersion;
	__u8 reserved[2041];
};

/* Time Stamp (ECMA-119 8.4.26.1) */
struct E119TimeStamp {
	digit year[4];
	digit month[2];
	digit day[2];
	digit hour[2];
	digit minute[2];
	digit second[2];
	digit hundredths[2];
	__s8 offsetCUT;
};

/* Primary Volume Descriptor (ECMA-119 8.4) */
struct E119PrimaryVolDesc {
	__u8 volDescType;
	__u8 stdIdent[5];
	__u8 volDescVersion;
	__u8 unused1;
	__u8 systemIdent[32];
	__u8 volIdent[32];
	__u8 unused2[8];
	__u32 volSpaceSize_le;
	__u32 volSpaceSize_be;
	__u8 unused3[32];
	__u16 volSetSize_le;
	__u16 volSetSize_be;
	__u16 volSeqNumber_le;
	__u16 volSeqNumber_be;
	#if 0
	__u16 volSeqNumber_le;
	__u16 volSeqNumber_be;
	#endif
	__u16 logicalBlockSize_le;
	__u16 logicalBlockSize_be;
	__u32 pathTblSize_le;
	__u32 pathTblSize_be;
	__u32 pathTblLoc_L_le;
	__u32 pathTblLoc_Lopt_le;
	__u32 pathTblLoc_M_le;
	__u32 pathTblLoc_Mopt_le;
	__u8 rootDirRecord[34];
	__u8 volSetIdent[128];
	__u8 publisherIdent[128];
	__u8 dataPreparerIdent[128];
	__u8 applicationIdent[128];
	__u8 copyrightFileIdent[37];
	__u8 abstractFileIdent[37];
	__u8 bibliographicFileIdent[37];
	struct E119TimeStamp volCreation;
	struct E119TimeStamp volModification;
	struct E119TimeStamp volExpiration;
	struct E119TimeStamp volEffective;
	__u8 fileStructureVersion;
	__u8 reserved1;
	__u8 applicationUse[512];
	__u8 reserved2[653];
};

/* Supplementary Volume Descriptor (ECMA-119 8.5) */
struct E119SupplementaryVolDesc {
	__u8 volDescType;
	__u8 stdIdent[5];
	__u8 volDescVersion;
	__u8 volFlags;
	__u8 systemIdent[32];
	__u8 volIdent[32];
	__u8 unused1[8];
	__u32 volSpaceSize_le;
	__u32 volSpaceSize_be;
	__u8 escapeSeqs[32];
	__u16 volSetSize_le;
	__u16 volSetSize_be;
	__u16 volSeqNumber_le;
	__u16 volSeqNumber_be;
	#if 0
	__u16 volSeqNumber_le;
	__u16 volSeqNumber_be;
	#endif
	__u16 logicalBlockSize_le;
	__u16 logicalBlockSize_be;
	__u32 pathTblSize_le;
	__u32 pathTblSize_be;
	__u32 pathTblLoc_L_le;
	__u32 pathTblLoc_Lopt_le;
	__u32 pathTblLoc_M_le;
	__u32 pathTblLoc_Mopt_le;
	__u8 rootDirRecord[34];
	__u8 volSetIdent[128];
	__u8 publisherIdent[128];
	__u8 dataPreparerIdent[128];
	__u8 applicationIdent[128];
	__u8 copyrightFileIdent[37];
	__u8 abstractFileIdent[37];
	__u8 bibliographicFileIdent[37];
	struct E119TimeStamp volCreation;
	struct E119TimeStamp volModification;
	struct E119TimeStamp volExpiration;
	struct E119TimeStamp volEffective;
	__u8 fileStructureVersion;
	__u8 reserved1;
	__u8 applicationUse[512];
	__u8 reserved2[653];
};

#endif /* !defined(_LINUX_ECMA119_H) */
