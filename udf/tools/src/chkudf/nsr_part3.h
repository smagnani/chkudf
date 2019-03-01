#ifndef __NSRPART3H__
#define __NSRPART3H__

#include "nsr_part1.h"

/* [3/7.1] Extent Descriptor -----------------------------------*/
struct extent_ad {
    uint32_t Length;
    uint32_t Location;
};

/* [3/7.2] [4/7.2] Descriptor Tag --------------------------------------*/
struct tag {
    uint16_t uTagID;
    uint16_t uDescriptorVersion;
    uint8_t  uTagChecksum;
    uint8_t  uReserved;
    uint16_t uTagSerialNum;
    uint16_t uDescriptorCRC;
    uint16_t uCRCLen;
    uint32_t uTagLoc;
};

/* [3/10.1] Primary Volume Descriptor --------------------------*/
struct PrimaryVolDes {
    struct tag sTag;       /* uTagID = 1 */
    uint32_t uVolDescSeqNum;
    uint32_t uPrimVolDesNum;
    dstring  aVolID[32];
    uint16_t uVSN;
    uint16_t uMaxVSN;
    uint16_t uInterchangeLev;
    uint16_t uMaxInterchangeLev;
    uint32_t uCharSetList;
    uint32_t uMaxCharSetList;
    dstring  aVolSetID[128];
    struct charspec  sDesCharSet;
    struct charspec  sExplanatoryCharSet;
    struct extent_ad sVolAbstract;
    struct extent_ad sVolCopyrightNotice;
    struct regid     sApplicationID;
    struct timestamp sRecordingTime;
    struct implEntityId sImplementationID;
    uint8_t  aImplementationUse[64];
    uint32_t uPredecessorVDSLoc;
    uint16_t uFlags;
    uint8_t  aReserved[22];
};

/* PrimaryVolDesc Flags Constants */

/* Bit 0 : Only one defined so far. */
#define COMMON_VOLSET_IDENTIFICATION ((uint16_t)BITZERO)

/* [3/10.2] Anchor Volume Descriptor Pointer -------------------*/
struct AnchorVolDesPtr {
    struct tag sTag;       /* uTagID = 2 */
    struct extent_ad sMainVDSAdr;
    struct extent_ad sReserveVDSAdr;
    uint8_t Reserved[480];
};

/* [3/10.3] Volume Descriptor Pointer --------------------------*/
struct VolDescPtr {
    struct tag sTag;      /* uTagID = 3 */
    uint32_t uVolDescSeqNum;
    struct extent_ad sNextVDS;
    uint8_t aReserved[484];
};

/* [3/10.4] Implementation Use Descriptor ----------------------*/
struct ImpUseDesc {
    struct tag sTag;      /* uTagID = 4 */
    uint32_t uVolDescSeqNum;
    struct udfEntityId sImplementationIdentifier;
    uint8_t aReserved[460];
};

/* OSTA Volume Descriptor (fits inside of implUseDesc.aReserved). */
struct LVInformation {
    struct charspec sLVICharset;
    dstring aLogicalVolumeIdentifier[128];
    dstring aLVInfo1[36];
    dstring aLVInfo2[36];
    dstring aLVInfo3[36];
    struct implEntityId sImplementationID;
    uint8_t aImplementationUse[128];
};

/* [3/10.5] Partition Descriptor ----------------------------------------*/
struct PartDesc {
    struct tag sTag;        /* uTagID = 5 */
    uint32_t uVolDescSeqNum;
    uint16_t uPartFlags;
    uint16_t uPartNumber;
    struct regid sPartContents;
    uint8_t  aPartContentsUse[128];
    uint32_t uAccessType;
    uint32_t uPartStartingLoc;
    uint32_t uPartLength;
    struct implEntityId sImplementationID;
    uint8_t  aImplementationUse[128];
    uint8_t  aReserved[156];
};

/* Partition Flags definition */
#define PARTITION_ALLOCATED ((uint16_t) BITZERO)

/* Access Type */
#define ACCESS_UNSPECIFIED      0
#define ACCESS_READ_ONLY        1
#define ACCESS_WORM             2
#define ACCESS_REWRITABLE       3
#define ACCESS_OVERWRITABLE     4

/* [3/10.7.2] Type 1 Partition Map */
struct PartMap1 {
    uint8_t  uPartMapType;
    uint8_t  uPartMapLen;
    uint16_t uVSN;
    uint16_t uPartNum;
};

/* [3/10.7.3] Type 2 Partition Map */
struct PartMap2 {
    uint8_t uPartMapType;
    uint8_t uPartMapLen;
    uint8_t uPartID[62];
};

struct PartMapVAT {
    uint8_t  uPartMapType;
    uint8_t  uPartMapLen;
    uint8_t  uReserved[2];
    struct udfEntityId sVATIdentifier;
    uint16_t uVSN;
    uint16_t uPartNum;
    uint8_t  uReserved2[24];
};

struct PartMapSP {
    uint8_t  uPartMapType;
    uint8_t  uPartMapLen;
    uint8_t  uReserved[2];
    struct udfEntityId sSPIdentifier;
    uint16_t uVSN;
    uint16_t uPartNum;
    uint16_t uPacketLength;
    uint8_t  N_ST;
    uint8_t  Reserved2;
    uint32_t SpareSize;
    uint32_t SpareLoc[4];
};

/* [3/10.6] Logical Volume Descriptor --------------------------*/
struct LogVolDesc {
    struct tag sTag;       /* uTagID = 6 */
    uint32_t uVolDescSeqNum;
    struct charspec sDesCharSet;
    dstring  uLogVolID[128];
    uint32_t uLogBlkSize;
    struct domainEntityId sDomainID;
    uint8_t  uLogVolUse[16];
    uint32_t uMapTabLen;
    uint32_t uNumPartMaps;
    struct implEntityId sImplementationID;
    uint8_t aImplementationUse[128];
    struct extent_ad integritySeqExtent;
/* Variable-length stuff follows: (see Macros below) */
/*    struct PartMap1 sPartMaps[uNumPartMaps];       */
/* Or:                                               */
/*    struct PartMap2 sPartMaps[uNumPartMaps];       */
};

/* Macros for LVDs */
#define LVD_HdrLen      (sizeof(struct LogVolDesc))
#define LVD_PM1Start(x) (struct PartMap1 *)((char *)(x)+LVD_HdrLen)
#define LVD_PM2Start(x) (struct PartMap2 *)((char *)(x)+LVD_HdrLen)

/* [3/10.8] Unallocated Space (volume) Entry -------------------*/
struct UnallocSpDesHead {
    struct tag sTag;          /* uTagID = 7 */
    uint32_t uVolDescSeqNum;
    uint32_t uNumAllocationDes;
};

/* [3/10.9] Terminator Descriptor */
struct Terminator {
    struct tag sTag;          /* uTagID = 8 */
    uint8_t aReserved[496];
};

/* [3/10.10] Logical Volume Integrity Descriptor ---------------*/
struct LogicalVolumeIntegrityDesc {
    struct tag          sTag;          /* uTagID = 9 */
    struct timestamp    sRecordingTime;
    uint32_t            integrityType;
    struct extent_ad    nextIntegrityExtent;
    uint64_t            UniqueId;
    uint8_t             reserved[24];
    uint32_t            N_P;    /* num Partitions */
    uint32_t            L_IU;   /* Len implement use */
/* Variable-length stuff follows: (see Macros below) */
/*    uint32_t            FreeSpaceTable[N_P];       */
/*    uint32_t            SizeTable[N_P];            */
/*    struct LVIDImplUse  ImplementUse;              */
};


/* Logical Volume Integrity Descriptor Implementation Use area */
struct LVIDImplUse {
    struct implEntityId implementationID;
    uint32_t            numFiles;
    uint32_t            numDirectories;
    uint16_t            MinUDFRead;
    uint16_t            MinUDFWrite;
    uint16_t            MaxUDFWrite;
};

/* Macros for LVIDImplUse */
/* NOTE that LVIDIU_IUStart() needs the LVID_IUStart() as its
   argument. Thus, the supported use model is for the two structs to
   be instantiated separately, and the LVIDIU to be cast to the
   implementation use pointer supplied by the LVID. */
#define LVIDIU_HdrLen     (sizeof(struct LVIDImplUse))
#define LVIDIU_IUStart(x) (uint8_t *)((char *)(x)+LVIDIU_HdrLen)

#endif
