#if !defined(_LINUX_UDF_H)
#define _LINUX_UDF_H
/*
 * udf.h
 *
 * PURPOSE
 *	OSTA-UDF(tm) format specification structures.
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
 *	Adapted from the UDF 1.50 specification.
 */

#include <linux/udf_fs.h>
#include <linux/udf_e167.h>
#include <linux/udf_e119.h>

/* UDF Specification Version */
#define UDF_SPEC_VERSION 0x0150

/* UDF character set (UDF 1.50 2.1.2) */
#define UDF_CHAR_SET_TYPE	0
#define UDF_CHAR_SET_INFO	"OSTA Compressed Unicode"
;;
/* Entity ID (UDF 1.50 2.1.5) */
typedef E167regid UDFEntityID;

/* This should do */
#define UDF_ID_DEVELOPER	"*Linux UDF Team"

/* Implementation Version (Our custom developer ID suffix) */
struct UDFImpVersion {
	/* Kernel version info */
	__u8 kernelmajor;
	__u8 kernelminor;
	__u8 kernelPatchLevel;
	__u8 kernelRelease;

	/* Development patch number */
	__u16 patchNumber;
};

/* Entity Identifiers (UDF 1.50 6.1) */
#define	UDF_ID_COMPLIANT	"*OSTA UDF Compliant"
#define UDF_ID_LV_INFO		"*UDF LV Info"
#define UDF_ID_FREE_EA		"*UDF FreeEASpace"
#define UDF_ID_FREE_APP_EA	"*UDF FreeAppEASpace"
#define UDF_ID_DVD_CGMS		"*UDF DVD CGMS Info"
#define UDF_ID_OS2_EA		"*UDF OS/2 EA"
#define UDF_ID_OS2_EA_LENGTH	"*UDF OS/2 EALength"
#define UDF_ID_MAC_VOLUME	"*UDF Mac VolumeInfo"
#define UDF_ID_MAC_FINDER	"*UDF Mac FinderInfo"
#define UDF_ID_MAC_UNIQUE	"*UDF Mac UniqueIDTable"
#define UDF_ID_MAC_RESOURCE	"*UDF Mac ResourceFork"
#define UDF_ID_PARTITION	"*UDF Virtual Partition"
#define UDF_ID_SPARABLE		"*UDF Sparable Partition"
#define UDF_ID_ALLOC		"*UDF Virtual Alloc Tbl"
#define UDF_ID_SPARING		"*UDF Sparing Table"

/* Operating System Identifiers (UDF 1.50 6.3) */
#define UDF_OS_CLASS_UNDEF	0x00U
#define UDF_OS_CLASS_DOS	0x01U
#define UDF_OS_CLASS_OS2	0x02U
#define UDF_OS_CLASS_MAC	0x03U
#define UDF_OS_CLASS_UNIX	0x04U
#define UDF_OS_CLASS_WIN95	0x05U
#define UDF_OS_CLASS_WINNT	0x06U
#define UDF_OS_ID_UNDEF		0x00U
#define UDF_OS_ID_DOS		0x00U
#define UDF_OS_ID_OS2		0x00U
#define UDF_OS_ID_MAC		0x00U
#define UDF_OS_ID_UNIX		0x00U
#define UDF_OS_ID_WIN95		0x00U
#define UDF_OS_ID_WINNT		0x00U
#define UDF_OS_ID_AIX		0x01U
#define UDF_OS_ID_SOLARIS	0x02U
#define UDF_OS_ID_HPUX		0x03U
#define UDF_OS_ID_IRIX		0x04U
#define UDF_OS_ID_LINUX		0x05U
#define UDF_OS_ID_MKLINUX	0x06U
#define UDF_OS_ID_FREEBSD	0x07U

#define UDF_NAME_LEN	255
#define UDF_PATH_LEN	1024
#define U32_MAX		0xffffffffU

/* Logical Volume Integrity Descriptor - Implementation Use (UDF 2.2.64) */
struct UDFLogicalVolIntegrityDescImpUse {
	struct UDFEntityID;
	__u32 numberOfFiles;
	__u32 numberOfDirectories;
	__u16 minUDFReadRevision;
	__u16 minUDFWriteRevision;
	__u16 maxUDFWriteRevision;
	__u8 impUse[0];
};

/*
 * Implementation Use Volume Descriptor - Logical Volume Information
 * (UDF 2.2.7.2)
 */
struct UDFLVInformation {
	struct UDFcharspec lviCharset;
	__u8 logicalVolID;
	__u8 lvInfo1[36];
	__u8 lvInfo2[36];
	__u8 lvInfo3[36];
	struct E167regid impID;
	__u8 impUse[128];
};

/* Virtual Partition Map (UDF 2.2.8) */
struct UDFVirtualPartitionMap {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u8 reserved1[2];
	struct UDFEntityID partitionTypeID;
	__u16 volSeqNumber;
	__u16 partitionNumber;
        __u8 reserved2[24];
};

/* Sparable Partition Map (UDF 2.2.9) */
struct UDFSparablePartitionMap {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u8 reserved1[2];
	struct UDFEntityID typeID;
	__u16 volSeqNumber;
	__u16 partitionNumber;
	__u16 packetLength;
	__u8 numberOfMapTables;
	__u8 reserved2;
	__u32 sparingTableSize;
	__u32 sparingTableLoc;
	__u8 padding[0];
};

/* Sparing Table Map Entry (UDF 1.50 2.2.11) */
struct UDFSparingMapEntry {
	__u32 originalLoc;
	__u32  mappedLoc;
};

/* Sparing Table (UDF 1.50 2.2.11) */
struct UDFSparingTable {
	E167tag tag;
	UDFEntityID sparingID;
	__u16 reallocationTableLength;
	__u8 reserved[2];
	__u32 seqNumber;
	UDFSparingMapEntry mapEntries[0];
};

/* Non-Allocatable SPace List (UDF 1.50 2.3.13) */
#define UDF_NONALLOCATABLE_SPACE_LIST "Non-Allocatable Space"


/* Invalid user identifier (UDF 1.50 3.3.3.1) */
#define UDF_INVALID_UID		0xffffffffU

/* Invalid group identifier (UDF 1.50 3.3.3.2) */
#define UDF_INVALID_GID		0xffffffffU

/* Unique Identifier (UDF 1.50 3.3.3.4) */
#define UDF_MAX_UNIQUE_ID	0xffffffffU
#define UDF_MIN_UNIQUE_ID	0x00000010U

/* Free Extended Attribute Space (UDF 1.50 3.3.4.5.1.1) */
struct UDFFreeEASpace {
	__u16 headerChecksum;
	__u8 byes[0];
};

/* DVD Copyright Management Information (UDF 1.50 3.3.4.5.1.2) */
struct UDFDVDCGMSInfo {
	__u16 headerChecksum;
	__u8 cgmsInfo;
	__u8 dataStructType;
	__u8 protectionSystemInfo;
};

#endif /* !defined(_LINUX_UDF_H) */
