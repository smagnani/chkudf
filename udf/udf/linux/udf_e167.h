#if !defined(_LINUX_ECMA167_H)
#define _LINUX_ECMA167_H
/*
 * udf_e167.h
 *
 * DESCRIPTION
 *	Definitions from the ECMA-167 standard.
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
 *	July 12, 1997 - Andrew E. Mileski
 *	Adapted from the ECMA-167 standard.
 */
#include <linux/udf_fs.h>

/* CS0 Charspec (ECMA-167 1/7.2.1) */
struct E167charspec {
	__u8 charSetType;
	__u8 charSetInfo[63];
};

/* Timestamp (ECMA-167 1/7.3) */
struct E167timestamp{
	__u16 typeAndTimezone;
	__u16 year;
	__u8 month;
	__u8 day;
	__u8 hour;
	__u8 minute;
	__u8 second;
	__u8 centiSecond;
	__u8 hundredsOfMicroSeconds;
	__u8 microSecond;
};

/* Timestamp types (ECMA-167 1/7.3.1) */
#define E167_TIMESTAMP_TYPE_CUT		0x0000U
#define E167_TIMESTAMP_TYPE_LOCAL	0x1000U
#define E167_TIMESTAMP_TYPE_AGREEMENT	0x2000U

/* Entity Identifier (ECMA-167 1/7.4) */
struct E167regid {
	__u8 flags;
	__u8 ident[23];
	__u8 identSuffix[8];
};


/* Entity identifier flags (ECMA-167 1/7.4.1) */
#define E167_REGID_FLAGS_DIRTY		0x01U
#define E167_REGID_FLAGS_PROTECTED	0x02U

/* Std structure identifiers (ECMA-167 2/9.1.2) */
#define E167_STDID_LEN		5
#define E167_STDID_BEA01	"BEA01"
#define E167_STDID_BOOT2	"BOOT2"
#define E167_STDID_CD001	"CD001"
#define E167_STDID_CDW02	"CDW02"
#define E167_STDID_NSR02	"NSR02"
#define E167_STDID_NSR03	"NSR03"
#define E167_STDID_TEA01	"TEA01"

/* Volume Structure Descriptor (ECMA-167 2/9.1) */
struct E167VolStructDesc {
	__u8 structType;
	__u8 stdIdent[E167_STDID_LEN];
	__u8 structVersion;
	__u8 structData[2041];
};

/* Beginning Extended Area Descriptor (ECMA-167 2/9.2) */
struct E167BeginningExtendedAreaDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 structData[2041];
};

/* Terminating Extended Area Descriptor (ECMA-167 2/9.3) */
struct E167TerminatingExtendedAreaDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 structData[2041];
};

/* Boot Descriptor (ECMA-167 2/9.4) */
struct E167BootDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 reserved1;
	struct E167regid architectureType;
	struct E167regid bootIdent;
	__u32 bootExtLocation;
	__u32 bootExtLength;
	__u64 loadAddress;
	__u64 startAddress;
	struct E167timestamp descCreationDateAndTime;
	__u16 flags;
	__u8 reserved2[32];
	__u8 bootUse[1906];
};

/* Boot flags (ECMA-167 2/9.4.12) */
#define E167_BOOT_FLAGS_ERASE	1

/* Extent Descriptor (ECMA-167 3/7.1) */
struct E167extent_ad {
	__u32 extLength;
	__u32 extLocation;
};

/* Descriptor Tag (ECMA-167 3/7.2) */
struct E167tag {
	__u16 tagIdent;
	__u16 descVersion;
	__u8 tagChecksum;
	__u8 reserved;
	__u16 tagSerialNum;
	__u16 descCRC;
	__u16 descCRCLength;
	__u32 tagLocation;
};

/* Tag Identifiers (ECMA-167 3/7.2.1) */
#define E167_UNUSED_DESC		0x0000U
#define E167_PRIMARY_VOL_DESC		0x0001U
#define E167_ANCHOR_VOL_DESC_PTR	0x0002U
#define E167_VOL_DESC_PTR		0x0003U
#define E167_IMP_USE_VOL_DESC		0x0004U
#define E167_PARTITION_DESC		0x0005U
#define E167_LOGICAL_VOL_DESC		0x0006U
#define E167_UNALLOC_SPACE_DESC		0x0007U
#define E167_TERMINATING_DESC		0x0008U
#define E167_LOGICAL_VOL_INTEGRITY_DESC	0x0009U

/* Tag Identifiers (ECMA-167 4/7.2.1) */
#define E167_FILE_SET_DESC		0x0100U
#define E167_FILE_IDENT_DESC		0x0101U
#define E167_ALLOC_EXTENT_DESC		0x0102U
#define E167_INDIRECT_ENTRY		0x0103U
#define E167_TERMINAL_ENTRY		0x0104U
#define E167_FILE_ENTRY			0x0105U
#define E167_EXTENDED_ATTRE_HEADER_DESC	0x0106U
#define E167_UNALLOCATED_SPACE_ENTRY	0x0107U
#define E167_SPACE_BITMAP_DESC		0x0108U
#define E167_PARTITION_INTEGRITY_ENTRY	0x0109U

/* NSR Descriptor (ECMA-167 3/9.1) */
struct E167NSRDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 reserved;
	__u8 structData[2040];
};
	
/* Primary Volume Descriptor (ECMA-167 3/10.1) */
struct E167PrimaryVolDesc {
	struct E167tag descTag;
	__u32 volDescSeqNum;
	__u32 primaryVolDescNum;
	dstring volIdent[32];
	__u16 volSeqNum;
	__u16 maxVolSeqNum;
	__u16 interchangeLvl;
	__u16 maxInterchangeLvl;
	__u32 charSetList;
	__u32 maxCharSetList;
	dstring volSetIdent[128];
	struct E167charspec descCharSet;
	struct E167charspec explanatoryCharSet;
	struct E167extent_ad volAbstract;
	struct E167extent_ad volCopyrightNotice;
	struct E167regid appIdent;
	struct E167timestamp recordingDateAndTIme;
	struct E167regid impdent;
	__u8 impUse[64];
	__u32 predcessorVolDescSeqLocation;
	__u32 flags;
	__u8 reserved[22];
};

/* Primary volume descriptor flags (ECMA-167 3/10.1.21) */
#define E167_VOL_SET_IDENT	1

/* Anchor Volume Descriptor Pointer (ECMA-167 3/10.2) */
struct E167AnchorVolDescPtr {
	struct E167tag descTag;
	struct E167extent_ad mainVolDescSeqExt;
	struct E167extent_ad reserveVolDescSeqExt;
	__u8 reserved[480];
};

/* Volume Descriptor Pointer (ECMA-167 3/10.3) */
struct E167VolDescPtr {
	struct E167tag descTag;
	__u32 volDescSeqNum;
	struct E167extent_ad nextVolDescSeqExt;
	__u8 reserved[484];
};

/* Implementation Use Volume Descriptor (ECMA-167 3/10.4) */
struct E167ImpUseVolDesc {
	struct E167tag descTag;
	__u32 volDescSeqNum;
	struct E167regid impIdent;
	__u8 impUse[460];
};

/* Partition Descriptor (ECMA-167 3/10.5) */
struct E167PartitionDesc {
	struct E167tag descTag;
	__u32 volDescSeqNum;
	__u16 partitionFlags;
	__u16 partitionNumber;
	struct E167regid partitionContents;
	__u8 partitionContentsUse[128];
	__u32 accessType;
	__u32 partitionStartingLocation;
	__u32 partitionLength;
	struct E167regid impIdent;
	__u8 impUse[128];
	__u8 reserved[156];
};

/* Partition Flags (ECMA-167 3/10.5.3) */
#define E167_PARTITION_FLAGS_ALLOC	1

/* Partition Contents (ECMA-167 3/10.5.5) */
#define E167_PARTITION_CONTENTS_FDC01	"+FDC01"
#define E167_PARTITION_CONTENTS_CD001	"+CD001"
#define E167_PARTITION_CONTENTS_CDW02	"+CDW02"
#define E167_PARTITION_CONTENTS_NSR02	"+NSR02"

/* Partition Access Types (ECMA-167 3/10.5.7) */
#define E167_PARTITION_ACCESS_NONE	0
#define E167_PARTITION_ACCESS_R		1
#define E167_PARTITION_ACCESS_WO	2
#define E167_PARTITION_ACCESS_RW	3
#define E167_PARTITION_ACCESS_OW	4

/* Logical Volume Descriptor (ECMA-167 3/10.6) */
struct E167LogicalVolDesc {
	struct E167tag descTag;
	__u32 volDescSeqNum;
	struct E167charspec descCharSet;
	dstring logicalVolIdent[128];
	__u32 logicalBlockSize;
	struct E167regid domainIdent;
	__u8 logicalVolContentsUse[16];
	__u32 mapTableLength;
	__u32 numPartitionMaps;
	struct E167regid impIdent;
	__u8 impUse[128];
	struct E167extent_ad integritySeqExt;
	__u8 partitionMaps[0];
};

/* Generic Partition Map (ECMA-167 3/10.7.1) */
struct E167GenericPartitionMap {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u8 partitionMapping[0];
};

/* Partition Map Type (ECMA-167 3/10.7.1.1) */
#define E167_PARTITION_MAP_TYPE_NONE		0
#define E167_PARTITION_MAP_TYPE_1		1
#define E167_PARTITION_MAP_TYPE_2		2

/* Type 1 Partition Map (ECMA-167 3/10.7.2) */
struct E167GenericPartitionMap1 {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u16 volSeqNum;
	__u16 partitionNum;
};

/* Type 2 Partition Map (ECMA-167 3/10.7.3) */
struct E167GenericPartitionMap2 {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u8 partitionIdent[62];
};

/* Unallocated Space Descriptor (ECMA-167 3/10.8) */
struct E167UnallocatedSpaceDesc {
	struct E167tag descTag;
	__u32 volDescSeqNum;
	__u32 numAllocDescs;
	struct E167extent_ad allocDescs[0];
};

/* Terminating Descriptor (ECMA 3/10.9) */
struct E167TerminatingDesc {
	struct E167tag descTag;
	__u8 reserved[4096];
};

/* Logical Volume Integrity Descriptor (ECMA-167 3/10.10) */
struct E167LogicalVolIntegrityDesc {
	struct E167tag descTag;
	struct E167timestamp recordingDateAndTime;
	__u32 integrityType;
	struct E167extent_ad nextIntegrityExt;
	__u8 logicalVolContentsUse[32];
	__u32 numOfPartitions;
	__u32 lengthOfImpUse;
	__u32 freeSpaceTable[0];
	__u32 sizeTable[0];
	__u8 impUse[0];
};

/* Integrity Types (ECMA-167 3/10.10.3) */
#define E167_INTEGRITY_TYPE_OPEN	0
#define E167_INTEGRITY_TYPE_CLOSE	1

/* Recorded Address (ECMA-167 4/7.1) */
struct E167lb_addr {
	__u32 logicalBlockNum;
	__u16 partitionReferenceNum;
};

/* Long Allocation Descriptor (ECMA-167 4/14.14.2) */
struct E167long_ad {
	__u32 extLength;
	struct E167lb_addr extLocation;
	__u8 impUse[6];
};

/* File Set Descriptor (ECMA-167 4/14.1) */
struct E167FileSetDesc {
	struct E167tag descTag;
	struct E167timestamp recordingDateandTime;
	__u16 interchangeLvl;
	__u16 maxInterchangeLvl;
	__u32 charSetList;
	__u32 maxCharSetList;
	__u32 fileSetNum;
	__u32 fileSetDescNum;
	struct E167charspec logicalVolIdentCharSet;
	dstring logicalVolIdent[128];
	struct E167charspec fileSetCharSet;
	dstring fileSetIdent[32];
	dstring copyrightFileIdent[32];
	dstring abstractFileIdent[32];
	struct E167long_ad rootDirectoryICB;
	struct E167regid domainIdent;
	struct E167long_ad nextExt;
	__u8 reserved[48];
};

/* Short Allocation Descriptor (ECMA-167 4/14.14.1) */
struct E167short_ad {
	__u32 extLength;
	__u32 extPosition;
};

/* Partition Header Descriptor (ECMA-167 4/14.3) */
struct E167PartitionHeaderDesc {
	struct E167short_ad unallocatedSpaceTable;
	struct E167short_ad unallocatedSpaceBitmap;
	struct E167short_ad partitionIntegrityTable;
	struct E167short_ad freedSpaceTable;
	struct E167short_ad freedSpaceBitmap;
	__u8 reserved[88];
};

/* File Identifier Descriptor (ECMA-167 4/14.4) */
struct E167FileIdentDesc {
	struct E167tag descTag;
	__u16 fileVersionNum;
	__u8 fileCharacteristics;
	__u8 lengthFileIdent;
	struct E167long_ad icb;
	__u16 lengthOfImpUse;
	__u8 impUse[0];
	char fileIdent[0];
	__u8 padding[0];
};

/* File Characteristics (ECMA-167 4/14.4.3) */
#define E167_FILE_EXISTENCE	0x01U
#define E167_FILE_DIRECTORY	0x02U
#define E167_FILE_DELETED	0x04U
#define E167_FILE_PARENT	0x08U

/* Allocation Ext Descriptor (ECMA-167 4/14.5) */
struct E167AllocExtDesc {
	struct E167tag descTag;
	__u32 previousAllocExtLocation;
	__u32 lengthAllocDescription;
};

/* ICB Tag (ECMA-167 4/14.6) */
struct E167icbtag {
	__u32 priorRecordedNumDirectEntries;
	__u16 strategyType;
	__u8 strategyParameter[2];
	__u16 numEntries;
	__u8 reserved;
	__u8 fileType;
	struct E167lb_addr parentICBLocation;
	__u16 flags;
};

/* ICB File Type (ECMA-167 4/14.6.6) */
#define E167_FILE_TYPE_NONE		0x00U
#define E167_FILE_TYPE_UNALLOC		0x01U
#define E167_FILE_TYPE_INTEGRITY	0x02U
#define E167_FILE_TYPE_INDIRECT		0x03U
#define E167_FILE_TYPE_DIRECTORY	0x04U
#define E167_FILE_TYPE_REGULAR		0x05U
#define E167_FILE_TYPE_BLOCK		0x06U
#define E167_FILE_TYPE_CHAR		0x07U
#define E167_FILE_TYPE_EXTENDED		0x08U
#define E167_FILE_TYPE_FIFO		0x09U
#define E167_FILE_TYPE_SOCKET		0x0aU
#define E167_FILE_TYPE_TERMINAL		0x0bU
#define E167_FILE_TYPE_SYMLINK		0x0cU

/* ICB Flags (ECMA-167 4/14.6.8) */
#define E167_ICB_FLAG_ALLOC_MASK	0x0007U
#define E167_ICB_FLAG_SORTED		0x0008U
#define E167_ICB_FLAG_NONRELOCATABLE	0x0010U
#define E167_ICB_FLAG_ARCHIVE		0x0020U
#define E167_ICB_FLAG_SETUID		0x0040U
#define E167_ICB_FLAG_SETGID		0x0080U
#define E167_ICB_FLAG_STICKY		0x0100U
#define E167_ICB_FLAG_CONTIGUOUS	0x0200U
#define E167_ICB_FLAG_SYSTEM		0x0400U
#define E167_ICB_FLAG_TRANSFORMED	0x0800U
#define E167_ICB_FLAG_MULTIVERSIONS	0x1000U

/* Indirect Entry (ECMA-167 4/14.7) */
struct E167IndirectEntry {
	struct E167tag descTag;
	struct E167icbtag icbTag;
	struct E167long_ad indirectICB;
};

/* Terminal Entry (ECMA-167 4/14.8) */
struct E167TerminalEntry {
	struct E167tag descTag;
	struct E167icbtag icbTag;
};

/* File Entry (ECMA-167 4/14.9) */
struct E167FileEntry {
	struct E167tag descTag;
	struct E167icbtag icbTag;
	__u32 uid;
	__u32 gid;
	__u32 permissions;
	__u16 fileLinkCount;
	__u8 recordFormat;
	__u8 recordDisplayAttr;
	__u32 recordLength;
	__u64 informationLength;
	__u64 logicalBlocksRecorded;
	struct E167timestamp accessTime;
	struct E167timestamp modificationTime;
	struct E167timestamp attrTime;
	__u32 checkpoint;
	struct E167long_ad extendedAttrICB;
	struct E167regid impIdent;
	__u64 uniqueID;
	__u32 lengthExtendedAttr;
	__u32 lengthAllocDescs;
	__u8 extendedAttr[0];
	__u8 allocDescs[0];
};

/* File Permissions (ECMA-167 4/14.9.5) */
#define E167_PERM_O_EXEC	0x00000001U
#define E167_PERM_O_WRITE	0x00000002U
#define E167_PERM_O_READ	0x00000004U
#define E167_PERM_O_CHATTR	0x00000008U
#define E167_PERM_O_DELETE	0x00000010U
#define E167_PERM_G_EXEC	0x00000020U
#define E167_PERM_G_WRITE	0x00000040U
#define E167_PERM_G_READ	0x00000080U
#define E167_PERM_G_CHATTR	0x00000100U
#define E167_PERM_G_DELETE	0x00000200U
#define E167_PERM_U_EXEC	0x00000400U
#define E167_PERM_U_WRITE	0x00000800U
#define E167_PERM_U_READ	0x00001000U
#define E167_PERM_U_CHATTR	0x00002000U
#define E167_PERM_U_DELETE	0x00004000U

/* File Record Format (ECMA-167 4/14.9.7) */
#define E167_RECORD_FMT_NONE		0
#define E167_RECORD_FMT_FIXED_PAD	1
#define E167_RECORD_FMT_FIXED		2
#define E167_RECORD_FMT_VARIABLE8	3
#define E167_RECORD_FMT_VARIABLE16	4
#define E167_RECORD_FMT_VARIABLE16_MSB	5
#define E167_RECORD_FMT_VARIABLE32	6
#define E167_RECORD_FMT_PRINT		7
#define E167_RECORD_FMT_LF		8
#define E167_RECORD_FMT_CR		9
#define E167_RECORD_FMT_CRLF		10
#define E167_RECORD_FMT_LFCR		10

/* Extended Attribute Header Descriptor (ECMA-167 4/14.10.1) */
struct E167ExtendedAttrHeaderDesc {
	struct E167tag descTag;
	__u32 impAttrLocation;
	__u32 appAttrLocation;
};

/* Generic Attribute Format (ECMA 4/14.10.2) */
struct E167GenericAttrFormat {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u8 attrData[0];
};

/* Character Set Attribute Format (ECMA 4/14.10.3) */
struct E167CharSetAttrFormat {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 escapeSeqLength;
	__u8 charSetType;
	__u8 escapeSeq[0];
};

/* Alternate Permissions (ECMA-167 4/14.10.4) */
struct E167AlternatePermissionsExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u16 ownerIdent;
	__u16 groupIdent;
	__u16 permission;
};

/* File Times Extended Attribute (ECMA-167 4/14.10.5) */
struct E167FileTimesExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 dataLength;
	__u32 fileTimeExistence;
	__u8 fileTimes;
};

/* FileTimeExistence (ECMA-167 4/14.10.5.6) */
#define E167_FTE_CREATION	0
#define E167_FTE_DELETION	2
#define E167_FTE_EFFECTIVE	3
#define E167_FTE_BACKUP		5

/* Information Times Extended Attribute (ECMA-167 4/14.10.6) */
struct E167InfoTimesExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 dataLength;
	__u32 infoTimeExistence;
	__u8 infoTimes[0];
};

/* Device Specification Extended Attribute (ECMA-167 4/14.10.7) */
struct E167DeviceSpecificationExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 impUseLength;
	__u32 majorDeviceIdent;
	__u32 minorDeviceIdent;
	__u8 impUse[0];
};

/* Implementation Use Extended Attr (ECMA-167 4/14.10.8) */
struct E167ImpUseExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 impUseLength;
	struct E167regid impIdent;
	__u8 impUse[0];
};

/* Application Use Extended Attribute (ECMA-167 4/14.10.9) */
struct E167AppUseExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 appUseLength;
	struct E167regid appIdent;
	__u8 appUse[0];
};

/* Unallocated Space Entry (ECMA-167 4/14.11) */
struct E167UnallocatedSpaceEntry {
	struct E167tag descTag;
	struct E167icbtag icbTag;
	__u32 lengthAllocDescs;
	__u8 allocDescs[0];
};

/* Space Bitmap Descriptor (ECMA-167 4/14.12) */
struct E167SpaceBitmap {
	struct E167tag descTag;
	__u32 numOfBits;
	__u32 numOfBytes;
	__u8 bitmap[0];
};

/* Partition Integrity Entry (ECMA-167 4/14.13) */
struct E167PartitionIntegrityEntry {
	struct E167tag descTag;
	struct E167icbtag icbTag;
	struct E167timestamp recordingDateAndTime;
	__u8 integrityType;
	__u8 reserved[175];
	struct E167regid impIdent;
	__u8 impUse[256];
};

#if 0
/* Extended Allocation Descriptor (ECMA-167 4/14.14.3) */
struct E167extent_ad {
	__u32 extLength;
	__u32 recordedLength;
	__u32 informationLength;
	struct E167lb_addr extLocation;
};
#endif

/* Logical Volume Header Descriptor (ECMA-167 4/14.5) */
struct E167LogicalVolHeaderDesc {
	__u64 uniqueId;
	__u8 reserved[24];
};

/* Path Component (ECMA-167 4/14.16.1) */
struct E167PathComponent {
	__u8 componentType;
	__u8 lengthComponentIdent;
	__u16 componentFileVersionNum;
	dstring componentIdent[0];
};

#endif /* !defined(_LINUX_ECMA167_H) */
