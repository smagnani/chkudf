#if !defined(_LINUX_UDF_FMT_H)
#define _LINUX_UDF_FMT_H
/*
 * udf_fmt.h
 *
 * DESCRIPTION
 *	UDF (Universal Disk Format) definitions.
 *
 *	These abbreviations are used to keep the symbols short:
 *		Alloc	Allocation
 *		App	Application
 *		Attr	Attribute
 *		Char	Characters
 *		Desc	Descriptor
 *		Descs	Descriptors
 *		Ext	Extent
 *		Ident	Identifier
 *		Imp	Implementation
 *		Lvl	Level
 *		Max	Maximum
 *		Num	Number
 *		Ptr	Pointer
 *		Seq	Sequence
 *		Std	Standard
 *		Struct	Structure
 *		Vol	Volume
 *	The symbols are otherwise identical to the standard, and the
 *	sections of the standard to refer to are indicated.
 *
 * HISTORY
 *	July 12, 1997 - Andrew E. Mileski
 *	Adapted from the OSTA-UDF(tm) 1.50, and ECMA-167 (ISO 13346) standards.
 */

#include <linux/udf_fs.h>

/* CS0 Charspec (ISO 13346 1/7.2.1) */
typedef struct {
	__u8 charSetType;
	__u8 charSetInfo[63];
} charspec;

/* UDF character set (UDF 1.50 2.1.2) */
#define UDF_CHAR_SET_TYPE	0
#define UDF_CHAR_SET_INFO	"OSTA Compressed Unicode"

/* Timestamp (ISO 13346 1/7.3) */
typedef struct {
	__u16 typeAndTimezone;
	__u16 year;
	__u8 month;
	__u8 day;
	__u8 hour;
	__u8 minute;
	__u8 second;
	__u8 centiseconds;
	__u8 hundredsOfMicroseconds;
	__u8 microseconds;
} timestamp;

/* Timestamp types (ISO 13346 1/7.3.1) */
#define TIMESTAMP_TYPE_CUT		0x0000U
#define TIMESTAMP_TYPE_LOCAL		0x0001U
#define TIMESTAMP_TYPE_AGREEMENT	0x0002U

/* Entity Identifier (ISO 13346 1/7.4) */
typedef struct {
	__u8 flags;
	__u8 ident[23];
	__u8 identSuffix[8];
} regid;

/* Entity identifier flags (ISO 13346 1/7.4.1) */
#define REGID_FLAGS_DIRTY	0x01U
#define REGID_FLAGS_PROTECTED	0x02U

/* Entity Identifiers (UDF 1.50 6.1) */
#define UDF_ID_DEVELOPER	"*Linux OSTA UDF 1.0"
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

/* Volume Structure Descriptor (ISO 13346 2/9.1) */
struct VolStructDesc {
	__u8 structType;
	__u8 stdIdent[5];
	__u8 structVersion;
	__u8 structData[2041];
};

/* Std structure identifiers (ISO 13346 2/9.1.2) */
#define STD_ID_LEN	5
#define STD_ID_BEA01	"BEA01"
#define STD_ID_BOOT2	"BOOT2"
#define STD_ID_CD001	"CD001"
#define STD_ID_CDW02	"CDW02"
#define STD_ID_NSR02	"NSR02"
#define STD_ID_TEA01	"TEA01"

/* Beginning Extended Area Descriptor (ISO 13346 2/9.2) */
struct BeginningExtendedAreaDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 structData[2041];
};

/* Terminating Extended Area Descriptor (ISO 13346 2/9.3) */
struct TerminatingExtendedAreaDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 structData[2041];
};

/* Boot Descriptor (ISO 13346 2/9.4) */
struct BootDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 reserved1;
	regid architectureType;
	regid bootIdent;
	__u32 bootExtLocation;
	__u32 bootExtLength;
	__u64 loadAddress;
	__u64 startAddress;
	timestamp descCreationDateAndTime;
	__u16 flags;
	__u8 reserved2[32];
	__u8 bootUse[1906];
};

/* Boot flags (ISO 13346 2/9.4.12) */
#define BOOT_FLAGS_ERASE	1

/* Extent Descriptor (ISO 13346 3/7.1) */
typedef struct {
	__u32 extLength;
	__u32 extLocation;
} extent_ad;

/* Descriptor Tag (ISO 13346 3/7.2) */
typedef struct {
	__u16 tagIdent;
	__u16 descVersion;
	__u8 tagChecksum;
	__u8 reserved;
	__u16 tagSerialNum;
	__u16 descCRC;
	__u16 descCRCLength;
	__u32 tagLocation;
} tag;

/* Tag Identifiers (ISO 13346 3/7.2.1) */
#define PRIMARY_VOL_DESC		0x0001U
#define ANCHOR_VOL_DESC_PTR		0x0002U
#define VOL_DESC_PTR			0x0003U
#define IMP_USE_VOL_DESC		0x0004U
#define PARTITION_DESC			0x0005U
#define LOGICAL_VOL_DESC		0x0006U
#define UNALLOC_SPACE_DESC		0x0007U
#define TERMINATING_DESC		0x0008U
#define LOGICAL_VOL_INTEGRITY_DESC	0x0009U

/* Tag Identifiers (ISO 13346 4/7.2.1) */
#define FILE_SET_DESC			0x0100U
#define FILE_IDENT_DESC			0x0101U
#define ALLOC_EXTENT_DESC		0x0102U
#define INDIRECT_ENTRY			0x0103U
#define TERMINAL_ENTRY			0x0104U
#define FILE_ENTRY			0x0105U
#define EXTENDED_ATTRE_HEADER_DESC	0x0106U
#define UNALLOCATED_SPACE_ENTRY		0x0107U
#define SPACE_BITMAP_DESC		0x0108U
#define PARTITION_INTEGRITY_ENTRY	0x0109U

/* NSR Descriptor (ISO 13346 3/9.1) */
struct NSRDesc {
	__u8 structType;
	__u8 stdIdent;
	__u8 structVersion;
	__u8 reserved;
	__u8 structData[2040];
};
	
/* Primary Volume Descriptor (ISO 13346 3/10.1) */
struct PrimaryVolDesc {
	tag descTag;
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
	charspec descCharSet;
	charspec explanatoryCharSet;
	extent_ad volAbstract;
	extent_ad volCopyrightNotice;
	regid appIdent;
	timestamp recordingDateAndTIme;
	regid impdent;
	__u8 impUse[64];
	__u32 predcessorVolDescSeqLocation;
	__u32 flags;
	__u8 reserved[22];
};

/* Primary volume descriptor flags (ISO 13346 3/10.1.21) */
#define VOL_SET_IDENT	1

/* Anchor Volume Descriptor Pointer (ISO 13346 3/10.2) */
struct AnchorVolDescPtr {
	tag descTag;
	extent_ad mainVolDescSeqExt;
	extent_ad reserveVolDescSeqExt;
	__u8 reserved[480];
};

/* Volume Descriptor Pointer (ISO 13346 3/10.3) */
struct VolDescPtr {
	tag descTag;
	__u32 volDescSeqNum;
	extent_ad nextVolDescSeqExt;
	__u8 reserved[484];
};

/* Implementation Use Volume Descriptor (ISO 13346 3/10.4) */
struct ImpUseVolDesc {
	tag descTag;
	__u32 volDescSeqNum;
	regid impIdent;
	__u8 impUse[460];
};

/* Partition Descriptor (ISO 13346 3/10.5) */
struct PartitionDesc {
	tag descTag;
	__u32 volDescSeqNum;
	__u16 partitionFlags;
	__u16 partitionNumber;
	regid partitionContents;
	__u8 partitionContentsUse[128];
	__u32 accessType;
	__u32 partitionStartingLocation;
	__u32 partitionLength;
	regid impIdent;
	__u8 impUse[128];
	__u8 reserved[156];
};

/* Partition Flags (ISO 13346 3/10.5.3) */
#define PARTITION_FLAGS_ALLOC	1

/* Partition Contents (ISO 13346 3/10.5.5) */
#define PARTITION_CONTENTS_FDC01	"+FDC01"
#define PARTITION_CONTENTS_CD001	"+CD001"
#define PARTITION_CONTENTS_CDW02	"+CDW02"
#define PARTITION_CONTENTS_NSR02	"+NSR02"

/* Partition Access Types (ISO 13346 3/10.5.7) */
#define PARTITION_ACCESS_NONE	0
#define PARTITION_ACCESS_R	1
#define PARTITION_ACCESS_WO	2
#define PARTITION_ACCESS_RW	3
#define PARTITION_ACCESS_OW	4

/* Logical Volume Descriptor (ISO 13346 3/10.6) */
struct LogicalVolDesc {
	tag descTag;
	__u32 volDescSeqNum;
	charspec descCharSet;
	dstring logicalVolIdent[128];
	__u32 logicalBlockSize;
	regid domainIdent;
	__u8 logicalVolContentsUse[16];
	__u32 mapTableLength;
	__u32 numPartitionMaps;
	regid impIdent;
	__u8 impUse[128];
	extent_ad integritySeqExt;
	__u8 partitionMaps[0];
};

/* Generic Partition Map (ISO 13346 3/10.7.1) */
struct GenericPartitionMap {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u8 partitionMapping[0];
};

/* Partition Map Type (ISO 13346 3/10.7.1.1) */
#define PARTITION_MAP_TYPE_NONE		0
#define PARTITION_MAP_TYPE_1		1
#define PARTITION_MAP_TYPE_2		2

/* Type 1 Partition Map (ISO 13346 3/10.7.2) */
struct GenericPartitionMap1 {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u16 volSeqNum;
	__u16 partitionNum;
};

/* Type 2 Partition Map (ISO 13346 3/10.7.3) */
struct GenericPartitionMap2 {
	__u8 partitionMapType;
	__u8 partitionMapLength;
	__u8 partitionIdent[62];
};

/* Unallocated Space Descriptor (ISO 13346 3/10.8) */
struct UnallocatedSpaceDesc {
	tag descTag;
	__u32 volDescSeqNum;
	__u32 numAllocDescs;
	extent_ad allocDescs[0];
};

/* Terminating Descriptor (ISO13346 3/10.9) */
struct TerminatingDesc {
	tag descTag;
	__u8 reserved[4096];
};

/* Logical Volume Integrity Descriptor (ISO 13346 3/10.10) */
struct LogicalVolIntegrityDesc {
	tag descTag;
	timestamp recordingDateAndTime;
	__u32 integrityType;
	extent_ad nextIntegrityExt;
	__u8 logicalVolContentsUse[32];
	__u32 numOfPartitions;
	__u32 lengthOfImpUse;
	__u32 freeSpaceTable[0];
	__u32 sizeTable[0];
	__u8 impUse[0];
};

/* Integrity Types (ISO 13346 3/10.10.3) */
#define INTEGRITY_TYPE_OPEN	0
#define INTEGRITY_TYPE_CLOSE	1

/* Recorded Address (ISO 13346 4/7.1) */
typedef struct {
	__u32 logicalBlockNum;
	__u16 partitionReferenceNum;
} lb_addr;

/* Long Allocation Descriptor (ISO 13346 4/14.14.2) */
typedef struct {
	__u32 extLength;
	lb_addr extLocation;
	__u8 impUse[6];
} long_ad;

/* File Set Descriptor (ISO 13346 4/14.1) */
struct FileSetDesc {
	tag descTag;
	timestamp recordingDateandTime;
	__u16 interchangeLvl;
	__u16 maxInterchangeLvl;
	__u32 charSetList;
	__u32 maxCharSetList;
	__u32 fileSetNum;
	__u32 fileSetDescNum;
	charspec logicalVolIdentCharSet;
	dstring logicalVolIdent[128];
	charspec fileSetCharSet;
	dstring fileSetIdent[32];
	dstring copyrightFileIdent[32];
	dstring abstractFileIdent[32];
	long_ad rootDirectoryICB;
	regid domainIdent;
	long_ad nextExt;
	__u8 reserved[48];
};

/* Short Allocation Descriptor (ISO 13346 4/14.14.1) */
typedef struct {
	__u32 extLength;
	__u32 extPosition;
} short_ad;

/* Partition Header Descriptor (ISO 13346 4/14.3) */
struct PartitionHeaderDesc {
	short_ad unallocatedSpaceTable;
	short_ad unallocatedSpaceBitmap;
	short_ad partitionIntegrityTable;
	short_ad freedSpaceTable;
	short_ad freedSpaceBitmap;
	__u8 reserved[88];
};

#define UDF_NAME_LEN	255
#define UDF_PATH_LEN	1023

/* File Identifier Descriptor (ISO 13346 4/14.4) */
struct FileIdentDesc {
	tag descTag;
	__u16 fileVersionNum;
	__u8 fileCharacteristics;
	__u8 lengthFileIdent;
	long_ad icb;
	__u16 lengthOfImpUse;
	__u8 impUse[0];
	char fileIdent[0];
	__u8 padding[0];
};

/* File Characteristics (ISO 13346 4/14.4.3) */
#define FILE_EXISTENCE	1
#define FILE_DIRECTORY	2
#define FILE_DELETED	4
#define FILE_PARENT	8

/* Allocation Ext Descriptor (ISO 13346 4/14.5) */
struct AllocExtDesc {
	tag descTag;
	__u32 previousAllocExtLocation;
	__u32 lengthAllocDescription;
};

/* ICB Tag (ISO 13346 4/14.6) */
typedef struct {
	__u32 priorRecordedNumDirectEntries;
	__u16 strategyType;
	__u8 strategyParameter[2];
	__u16 numEntries;
	__u8 reserved;
	__u8 fileType;
	lb_addr parentICBLocation;
	__u16 flags;
} icbtag;

/* ICB File Type (ISO 13346 4/14.6.6) */
#define FILE_TYPE_NONE		0x00U
#define FILE_TYPE_UNALLOC	0x01U
#define FILE_TYPE_INTEGRITY	0x02U
#define FILE_TYPE_INDIRECT	0x03U
#define FILE_TYPE_DIRECTORY	0x04U
#define FILE_TYPE_REGULAR	0x05U
#define FILE_TYPE_BLOCK		0x06U
#define FILE_TYPE_CHAR		0x07U
#define FILE_TYPE_EXTENDED	0x08U
#define FILE_TYPE_FIFO		0x09U
#define FILE_TYPE_SOCKET	0x0aU
#define FILE_TYPE_TERMINAL	0x0bU
#define FILE_TYPE_SYMLINK	0x0cU

/* ICB Flags (ISO 13346 4/14.6.8) */
#define ICB_FLAG_ALLOC_MASK	0x0007U
#define ICB_FLAG_SORTED		0x0008U
#define ICB_FLAG_NONRELOCATABLE	0x0010U
#define ICB_FLAG_ARCHIVE	0x0020U
#define ICB_FLAG_SETUID		0x0040U
#define ICB_FLAG_SETGID		0x0080U
#define ICB_FLAG_STICKY		0x0100U
#define ICB_FLAG_CONTIGUOUS	0x0200U
#define ICB_FLAG_SYSTEM		0x0400U
#define ICB_FLAG_TRANSFORMED	0x0800U
#define ICB_FLAG_MULTIVERSIONS	0x1000U

/* Indirect Entry (ISO 13346 4/14.7) */
struct IndirectEntry {
	tag descTag;
	icbtag icbTag;
	long_ad indirectICB;
};

/* Terminal Entry (ISO 13346 4/14.8) */
struct TerminalEntry {
	tag descTag;
	icbtag icbTag;
};

/* File Entry (ISO 13346 4/14.9) */
struct FileEntry {
	tag descTag;
	icbtag icbTag;
	__u32 uid;
	__u32 gid;
	__u32 permissions;
	__u16 fileLinkCount;
	__u8 recordFormat;
	__u8 recordDisplayAttr;
	__u32 recordLength;
	__u64 informationLength;
	__u64 logicalBlocksRecorded;
	timestamp accessTime;
	timestamp modificationTime;
	timestamp attrTime;
	__u32 checkpoint;
	long_ad extendedAttrICB;
	regid impIdent;
	__u64 uniqueID;
	__u32 lengthExtendedAttr;
	__u32 lengthAllocDescs;
	__u8 extendedAttr[0];
	__u8 allocDescs[0];
};

/* File Permissions (ISO 13346 4/14.9.5) */
#define PERM_O_EXEC	0x00000001U
#define PERM_O_WRITE	0x00000002U
#define PERM_O_READ	0x00000004U
#define PERM_O_CHATTR	0x00000008U
#define PERM_O_DELETE	0x00000010U
#define PERM_G_EXEC	0x00000020U
#define PERM_G_WRITE	0x00000040U
#define PERM_G_READ	0x00000080U
#define PERM_G_CHATTR	0x00000100U
#define PERM_G_DELETE	0x00000200U
#define PERM_U_EXEC	0x00000400U
#define PERM_U_WRITE	0x00000800U
#define PERM_U_READ	0x00001000U
#define PERM_U_CHATTR	0x00002000U
#define PERM_U_DELETE	0x00004000U

/* File Record Format (ISO 13346 4/14.9.7) */
#define RECORD_FMT_NONE			0
#define RECORD_FMT_FIXED_PAD		1
#define RECORD_FMT_FIXED		2
#define RECORD_FMT_VARIABLE8		3
#define RECORD_FMT_VARIABLE16		4
#define RECORD_FMT_VARIABLE16_MSB	5
#define RECORD_FMT_VARIABLE32		6
#define RECORD_FMT_PRINT		7
#define RECORD_FMT_LF			8
#define RECORD_FMT_CR			9
#define RECORD_FMT_CRLF			10
#define RECORD_FMT_LFCR			10

/* Extended Attribute Header Descriptor (ISO 13346 4/14.10.1) */
struct ExtendedAttrHeaderDesc {
	tag descTag;
	__u32 impAttrLocation;
	__u32 appAttrLocation;
};

/* Generic Attribute Format (ISO9660 4/14.10.2) */
struct GenericAttrFormat {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u8 attrData[0];
};

/* Character Set Attribute Format (ISO9660 4/14.10.3) */
struct CharSetAttrFormat {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 escapeSeqLength;
	__u8 charSetType;
	__u8 escapeSeq[0];
};

/* Alternate Permissions (ISO 13346 4/14.10.4) */
struct AlternatePermissionsExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u16 ownerIdent;
	__u16 groupIdent;
	__u16 permission;
};

/* File Times Extended Attribute (ISO 13346 4/14.10.5) */
struct FileTimesExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 dataLength;
	__u32 fileTimeExistence;
	__u8 fileTimes;
};

/* FileTimeExistence (ISO 13346 4/14.10.5.6) */
#define FTE_CREATION	0
#define FTE_DELETION	2
#define FTE_EFFECTIVE	3
#define FTE_BACKUP	5

/* Information Times Extended Attribute (ISO 13346 4/14.10.6) */
struct InfoTimesExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 dataLength;
	__u32 infoTimeExistence;
	__u8 infoTimes[0];
};

/* Device Specification Extended Attribute (ISO 13346 4/14.10.7) */
struct DeviceSpecificationExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 impUseLength;
	__u32 majorDeviceIdent;
	__u32 minorDeviceIdent;
	__u8 impUse[0];
};

/* Implementation Use Extended Attr (ISO 13346 4/14.10.8) */
struct ImpUseExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 impUseLength;
	regid impIdent;
	__u8 impUse[0];
};


/* Application Use Extended Attribute (ISO 13346 4/14.10.9) */
struct AppUseExtendedAttr {
	__u32 attrType;
	__u8 attrSubtype;
	__u8 reserved[3];
	__u32 attrLength;
	__u32 appUseLength;
	regid appIdent;
	__u8 appUse[0];
};

/* Unallocated Space Entry (ISO 13346 4/14.11) */
struct UnallocatedSpaceEntry {
	tag descTag;
	icbtag icbTag;
	__u32 lengthAllocDescs;
	__u8 allocDescs[0];
};

/* Space Bitmap Descriptor (ISO 13346 4/14.12) */
struct SpaceBitmap {
	tag descTag;
	__u32 numOfBits;
	__u32 numOfBytes;
	__u8 bitmap[0];
};

/* Partition Integrity Entry (ISO 13346 4/14.13) */
struct PartitionIntegrityEntry {
	tag descTag;
	icbtag icbTag;
	timestamp recordingDateAndTime;
	__u8 integrityType;
	__u8 reserved[175];
	regid impIdent;
	__u8 impUse[256];
};

/* Extended Allocation Descriptor (ISO 13346 4/14.14.3) */
typedef struct { /* ISO 13346 4/14.14.3 */
	__u32 extLength;
	__u32 recordedLength;
	__u32 informationLength;
	lb_addr extLocation;
} ext_ad;

/* Logical Volume Header Descriptor (ISO 13346 4/14.5) */
struct LogicalVolHeaderDesc {
	__u64 uniqueId;
	__u8 reserved[24];
};

/* Path Component (ISO 13346 4/14.16.1) */
struct PathComponent {
	__u8 componentType;
	__u8 lengthComponentIdent;
	__u16 componentFileVersionNum;
	dstring componentIdent[0];
};

#endif /* !defined(_LINUX_UDF_FMT_H) */
