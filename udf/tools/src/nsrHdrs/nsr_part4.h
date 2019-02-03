#ifndef __NSRPART4H__
#define __NSRPART4H__

#include "nsr_part1.h"
#include "nsr_part3.h"

/* [4/7.1] -------------------------------------------------------------------
 *                             WARNING
 *                          struct lb_addr
 *                             WARNING
 *
 * struct lb_addr is NOT recommended for use. The size of this structure
 * will be rounded up to 8 bytes on some machines (e.g. HP-PA)!
 *
 */

struct lb_addr {
    uint32_t        LBN;         /* partition relative block # */
    uint16_t        PartNo;
};

/* ==========================================================================
 *           Allocation Descriptors - See ECMA-167 P4-14
 *
 * struct short_ad: This is used for a single partition
 * struct long_ad: This is used for multiple partitions
 * struct ext_ad: This is used for compression
 */

/* [4/14.14.1.1] ------------------------------------------------------------*/

/*
 * Extent Type Constants
 */

#define E_RECORDED    0 /* extent allocated and recorded */
#define E_ALLOCATED   1 /* extent allocated but unrecorded */
#define E_UNALLOCATED 2 /* extent unallocated and unrecorded */
#define E_ALLOCEXTENT 3 /* extent is next extent of ADs */

/* [4/14.14.1] short allocation descriptor (8 bytes) -----------------------*/
struct short_ad {
    uint32_t        ExtentLengthAndType;
    uint32_t        Location;         /* logical block address */
};

/* [4/14.14.2] long allocation descriptor (16 bytes) -----------------------*/
struct long_ad {
    uint32_t        ExtentLengthAndType;
    uint32_t        Location_LBN;     /* partition relative block # */
    uint16_t        Location_PartNo;  /* partition number */
    uint16_t        uUdfFlags;        /* See UDF1.01 2.3.10.1 */
    uint8_t         aImpUse[4];
};

#define EXTENT_LENGTH(extentLengthAndType)    (U_endian32(extentLengthAndType) & 0x3FFFFFFF)
#define EXTENT_TYPE(extentLengthAndType)      (U_endian32(extentLengthAndType) >> 30)

/* udf flag definitions */
#define EXT_ERASED ((uint16_t)BITZERO) /* Only valid for ALLOCATED ADs */

/* [4/14.14.3] extended allocation descriptor (20 bytes) -------------------*/
struct ext_ad {
    uint32_t        ExtentLengthAndType;
    uint32_t        RecordedLength;   /* in bytes too... */
    uint32_t        InfoLength;       /* bytes */
    uint32_t        Location_LBN;     /* partition relative block # */
    uint16_t        Location_PartNo;  /* partition number */
    uint8_t         aImpUse[2];
};

/* [4/14.1] File Set Descriptor --------------------------------------------*/
struct FileSetDesc {
    struct tag            sTag;          /* uTagID = 256 */
    struct timestamp      sRecordingTime;
    uint16_t              uInterchangeLev;
    uint16_t              uMaxInterchangeLev;
    uint32_t              uCharSetList;
    uint32_t              uMaxCharSetList;
    uint32_t              uFileSetNum;
    uint32_t              uFileSetDescNum;
    struct charspec       sLogVolIDCharSet;
    dstring               aLogVolID[128];
    struct charspec       sFileSetCharSet;
    dstring               aFileSetID[32];
    dstring               aCopyrightFileID[32];
    dstring               aAbstractFileID[32];
    struct long_ad        sRootDirICB;
    struct domainEntityId DomainID;
    struct long_ad        sNextExtent;
    struct long_ad        sStreamDirICB;
    uint8_t               aReserved[32];
};

/* [4/14.3] Partition Header Descriptor ------------------------------------*/
/* This structure is recorded in the Partition Contents Use    */
/* field [3/10.5.6] of the Partition Descriptor [3/10.5].  The */
/* Partition Descriptor shall have "+NSR02" recorded in the    */
/* Partition Contents field.                                   */

struct PartHeaderDesc {
    struct short_ad UST;            /* unallocated space table */
    struct short_ad USB;            /* unallocated space bitmap */
    struct short_ad PIT;            /* partition integrity table */
    struct short_ad FST;            /* freed space table */
    struct short_ad FSB;            /* freed space bitmap */
    uint8_t aReserved[88];
};

/* [4/14.4] File Identifier Descriptor ---------------------------------------
 * constant length part of a ECMA 167 File Identifier Desc
 *
 * define a constant here to represent the constant size of the
 * file identifier descriptor since sizeof(FileIDDesc) may be
 * erroneous on risc machines that pad to a multiple of 4 bytes.
 * To obtain length of entire FID, add L_FI, L_IU, and the following
 * constant and then round up to a multiple of four (for padding).
 */
#define FILE_ID_DESC_CONSTANT_LEN       38

/* WARNING - Do not use sizeof(FileIDDesc) !!!
 *  This is placing a struct at a non-multiple-of-four location!!!
 *  There is not much we can do about this, since UDF1.01 mandates it.
 *  There are no apparent problems on g++/hp-ux, since the structure
 *  itself does not contain any word-long pieces.
 */
struct FileIDDesc {
    struct tag          sTag;   /* uTagID = 257 */
    uint16_t            VersionNum;
    uint8_t             Characteristics;
    uint8_t             L_FI;
    struct long_ad      ICB;
    uint16_t            L_IU;
    struct implEntityId sImplementationID;
    /* Implementation Use Falls here */
    /* File ID falls here */
    /* padding falls here */
};

/* File Characteristics [4/14.4.4] */

#define FILE_ATTR           ((uint8_t) BITZERO)   /* attribute for a file     */
#define HIDDEN_ATTR         ((uint8_t) BITZERO)   /* ... existence bit        */
#define DIR_ATTR            ((uint8_t) BITONE)    /* ... for a directory      */
#define DELETE_ATTR         ((uint8_t) BITTWO)    /* ... for a deleted file   */
#define PARENT_ATTR         ((uint8_t) BITTHREE)  /* ... for a parent         */

/* [4/14.5] Allocation Extent Descriptor -----------------------------------*/
struct AllocationExtentDesc {
    struct tag sTag;         /* uTagID = 258 */
    uint32_t   prevAllocExtLoc;
    uint32_t   L_AD;
    /* allocation descriptors go here: ADMacros at end of file */
};

/* [4/14.6] Information Control Block Details ------------------------------*/
struct ICBTag {
    uint32_t PriorDirects;
    uint16_t StrategyType;
    uint8_t  StrategyParm[2];
    uint16_t NumberEntries;
    uint8_t  Reserved;
    uint8_t  FileType;
    /* lb_addr   sParentICB; */
    uint32_t sParentICB_LBN;       /* partition relative block # */
    uint16_t sParentICB_PartNo;    /* partition number */
    uint16_t Flags;
};

/* ICB file types */
#define FILE_TYPE_UNSPECIFIED          0
#define FILE_TYPE_UNALLOC_SP_ENTRY     1
#define FILE_TYPE_PARTITION_INTEGRITY  2
#define FILE_TYPE_INDIRECT_ENTRY       3
#define FILE_TYPE_DIRECTORY            4
#define FILE_TYPE_RAW                  5
#define FILE_TYPE_BDEV                 6
#define FILE_TYPE_CDEV                 7
#define FILE_TYPE_EXT_ATTR             8
#define FILE_TYPE_FIFO                 9
#define FILE_TYPE_SOCKET              10
#define FILE_TYPE_TERMINAL_ENTRY      11
#define FILE_TYPE_SYMLINK             12
#define FILE_TYPE_STREAMDIR           13

/* ICB Flags Constants */

/* Bits 0,1,2 - ADType */
#define ADTYPEMASK     UINT16_C(0x0007)   /* allocation descriptor type */
                                          /* mask for ICBTag flags field */
#define ADSHORT        UINT16_C(0x00)     /* Short ADs */
#define ADLONG         UINT16_C(0x01)     /* Long ADs */
#define ADEXTENDED     UINT16_C(0x02)     /* Extended ADs */
#define ADNONE         UINT16_C(0x03)     /* Data replaces ADs */

/* Bits 3-9 : Miscellaneous */
#define SORTED_DIRECTORY   0x0008 /* This should be cleared. */
#define NON_RELOCATABLE    0x0010 /* We don't set this. */
#define ARCHIVE            0x0020 /* Should always be set when written or modified. */
#define ICBF_S_ISUID       0x0040
#define ICBF_S_ISGID       0x0080
#define ICBF_C_ISVTX       0x0100
#define CONTIGUOUS         0x0200
#define FLAG_SYSTEM        0x0400
#define FLAG_TANSFORMED    0x0800
#define FLAG_MULTIVERS     0x1000


/* [4/14.7] Indirect Entry -------------------------------------------------*/
struct IndirectEntry {
    struct tag sTag;       /* uTagID = 259 */
    struct ICBTag sICBTag; /* FileType = 3 */
    struct long_ad sIndirectICB;
};

/* [4/14.8] Terminal Entry -------------------------------------------------*/
struct TerminalEntry {
    struct tag sTag;        /* uTagID = 260 */
    struct ICBTag sICBTag;  /* FileType = 11 */
};

/* [4/14.9] File Entry -----------------------------------------------------*/
struct FileEntry {
    struct tag sTag;        /* uTagID = 261 */
    struct ICBTag sICBTag;  /* FileType = [4-10] */
    uint32_t UID;
    uint32_t GID;
    uint32_t Permissions;
    uint16_t LinkCount;
    uint8_t  RecFormat;
    uint8_t  RecDisplayAttr;
    uint32_t RecLength;
    uint64_t InfoLength;
    uint64_t LogBlocks;
    struct timestamp sAccessTime;   /* each timestamp is 12 bytes long */
    struct timestamp sModifyTime;
    struct timestamp sAttrTime;
    uint32_t Checkpoint;
    struct long_ad sExtAttrICB;     /* 16 bytes */
    struct implEntityId sImpID;
    uint64_t UniqueId;              /* for 13346 */
    uint32_t L_EA;
    uint32_t L_AD;
    /* allocation descriptors go here: ADMacros at end of file */
};

/* [4/14.11] Unallocated Space Entry ---------------------------------------*/
struct UnallocSpEntry {
    struct tag sTag;   /* uTagID = 263 */
    struct ICBTag sICBTag;  /* FileType = 1 */
    uint32_t L_AD;
    /* allocation descriptors go here: ADMacros at end of file */
};

/* [4/14.12] Space Bitmap Entry --------------------------------------------*/
struct SpaceBitmapHdr {
    struct tag sTag;   /* uTagID = 264 */
    uint32_t   N_Bits;
    uint32_t   N_Bytes;
};
struct SpaceBitmapEntry {
    struct SpaceBitmapHdr hdr;
    uint8_t  bitmap[0]; /* bitmap */
};

/* [4/14.13] Partition Integrity Entry -------------------------------------*/
struct PartIntegrityEntry {
    struct tag sTag;         /* uTagID = 265 */
    struct ICBTag sICBTag;   /* FileType = 2 */
    struct timestamp sRecordingTime;
    uint8_t  IntegrityType;
    uint8_t  reserved[175];
    struct regid sImpID;
    uint8_t  aImpUse[256];
};

/* [4/14.17] Extended File Entry -------------------------------------------*/
struct ExtFileEntry {
    struct tag sTag;        /* uTagID = 266 */
    struct ICBTag sICBTag;  /* FileType = [4-10] */
    uint32_t UID;
    uint32_t GID;
    uint32_t Permissions;
    uint16_t LinkCount;
    uint8_t  RecFormat;
    uint8_t  RecDisplayAttr;
    uint32_t RecLength;
    uint64_t InfoLength;
    uint64_t ObjectSize;
    uint64_t LogBlocks;
    struct timestamp sAccessTime;   /* each timestamp is 12 bytes long */
    struct timestamp sModifyTime;
    struct timestamp sCreationTime;
    struct timestamp sAttrTime;
    uint32_t Checkpoint;
    uint32_t Reserved;
    struct long_ad sExtAttrICB;     /* 16 bytes */
    struct long_ad sStreamDirICB;   /* 16 bytes */
    struct implEntityId sImpID;
    uint64_t UniqueId;              /* for 13346 */
    uint32_t L_EA;
    uint32_t L_AD;
    /* allocation descriptors go here: ADMacros at end of file */
};

struct FE_or_EFE {
    struct tag sTag;        /* uTagID = 261 (FE) or 266 (EFE) */
    struct ICBTag sICBTag;  /* FileType = [4-10] */
    uint32_t UID;
    uint32_t GID;
    uint32_t Permissions;
    uint16_t LinkCount;
    uint8_t  RecFormat;
    uint8_t  RecDisplayAttr;
    uint32_t RecLength;
    uint64_t InfoLength;

    union {
      struct FE {
        uint64_t LogBlocks;
        struct timestamp sAccessTime;   /* each timestamp is 12 bytes long */
        struct timestamp sModifyTime;
        struct timestamp sAttrTime;
        uint32_t Checkpoint;
        struct long_ad sExtAttrICB;     /* 16 bytes */
        struct implEntityId sImpID;
        uint64_t UniqueId;              /* for 13346 */
        uint32_t L_EA;
        uint32_t L_AD;
        /* allocation descriptors go here: ADMacros at end of file */
      } FE;
      struct EFE {
        uint64_t ObjectSize;
        uint64_t LogBlocks;
        struct timestamp sAccessTime;   /* each timestamp is 12 bytes long */
        struct timestamp sModifyTime;
        struct timestamp sCreationTime;
        struct timestamp sAttrTime;
        uint32_t Checkpoint;
        uint32_t Reserved;
        struct long_ad sExtAttrICB;     /* 16 bytes */
        struct long_ad sStreamDirICB;   /* 16 bytes */
        struct implEntityId sImpID;
        uint64_t UniqueId;              /* for 13346 */
        uint32_t L_EA;
        uint32_t L_AD;
        /* allocation descriptors go here: ADMacros at end of file */
      } EFE;
    };
};

/* INTEGRITY CONSTANTS
   The first two are valid for LVIDs as well as PIEs.
   The final one, INTEGRITY_STABLE, is ONLY valid for PIE.
   */

#define INTEGRITY_OPEN     0
#define INTEGRITY_CLOSE    1
#define INTEGRITY_STABLE   2

/* Extended Attributes =====================================================*/

/* [4/14.10.1] Extended Attribute Header Descriptor ------------------------*/
struct ExtAttrHeaderDesc {
    struct tag sTag;   /* uTagID = 262 */
    uint32_t ImpAttrLoc;
    uint32_t AppAttrLoc;
};

struct ImplUseEA {
    uint32_t AttrType;
    uint8_t  AttrSubType;
    uint8_t  Reserved[3];
    uint32_t AttrLen;
    uint32_t AppUseLen;
    struct udfEntityId EA_ID;
};
#endif
