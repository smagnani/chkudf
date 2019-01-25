#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include <malloc.h>
#include <stdio.h>

/*****************************************************************************
 * Device operating parameters
 *
 * These are globals used when referencing the device or image file.
 ---------------------------------------------------------------------------*/

UINT32  blocksize = 0;               // bytes per sector
MUINT8  bdivshift = 0;               // log2(blocksize)
UINT32  secsize = 0;                 // bytes per sector
MUINT8  sdivshift = 0;               // log2(secsize)
MUINT8  s_per_b = 1;                 // blocksize/secsize
UINT32  packet_size;                 // blocking factor for read operations
BOOL    scsi = FALSE;                // Boolean for command selection
int     device = 0;                  // Device/file handle for operations
UINT32  LastSector = 0;              // Location of the last readable sector
BOOL    LastSectorAccurate = FALSE;  // Indication of confidence
UINT32  lastSessionStartLBA = 0;     // Start of last session
BOOL    isType5 = FALSE;             // Is a CD or DVD drive
BOOL    isCDRW = FALSE;

UINT8   *scsibuf = NULL;             // Used for scsi scratchpad
UINT32  scsibufsize = 0;             // amount allocated for SCSI buffer
UINT8   cdb[12];                     // command buffer
UINT8   sensedata[18];               // Sense data buffer
int     sensebufsize = 18;           // Sense data buffer size

/*****************************************************************************
 * chkudf operating parameters
 *
 * These are globals used by low level routines within chkudf, and aren't
 * directly related to the file system.
 ---------------------------------------------------------------------------*/

sCacheData Cache[NUM_CACHE];
MUINT8     bufno = 0;
sError     Error = {0, 0, 0, 0};

char *Error_Msgs[] = {
/*  1 */    "Expected Tag ID of %lld, found %lld",
            "Expected Tag location of %08llx, read %08llx",
            "Expected Tag checksum of %02llx, computed %02llx",
            "Expected Tag CRC of %04llx, found %04llx",
/*  5 */    "Not an Anchor Volume Descriptor Pointer",
            "%lld sectors did not contain a volume descriptor matching %lld",
            "Either a non-valid structure or terminating descriptor was encountered",
            "This program can handle %lld partitions and the logical volume has %lld",
            "Error reading sector",
/* 10 */    "No VAT present",
            "Not able to allocate memory for VAT",
            "No virtual space described",
            "No file set descriptor found",
            "Tag CRC length limit is %04llx, found %04llx",
/* 15 */    "Volume Descriptor Sequences are not equivalent",
            "Anchor Volume Descriptor Pointers are not equivalent",
            "Volume Space overlap detected",
            "No sparable partition present",
            "NSR descriptor version should be %lld, was %lld",
/* 20 */    "Not able to allocate memory for Sparing Map",
            "Specified location does not contain a Sparing Map",
            "Volume Descriptor Sequence not found",
            "Can't allocate memory for Volume Descriptors",
            "Partition Space overlap detected",
/* 25 */    "No more memory for ICB tracking available",
            "Expected Allocation Descriptors for %lld bytes, found %lld",
            "%lld Partitions found, Partition Reference Number %lld out of range",
            "%lld blocks in Partition, Logical Block Number %lld out of range",
            "Adjacent Allocation Descriptors found (descriptor for %lld)",
/* 30 */    "Expected Serial number of %lld, found %lld. (disabling reporting)",
            "Expected Extent Type %lld, found prohibited type %lld",
            "Expected AD Type %lld, found prohibited type %lld",
            "Unallocated extents not sorted in ascending order",
            "Found VAT ICB. Unfortunately, code to process it does not yet exist."
/* 35 */    "Expected AD length %lld, but found unexpected zero-length extent at offset %lld.",
};



/*****************************************************************************
 * UDF basics
 *
 * These are globals used by the bottom level checking; used for both parts
 * 3 and 4.
 ---------------------------------------------------------------------------*/

UINT16     UDF_Version;
BOOL       Version_OK = FALSE;
UINT16     Serial_No;
BOOL       Serial_OK = FALSE;
BOOL       Fatal = FALSE;


/*****************************************************************************
 * Volume information
 *
 * These are globals used by the Volume Space checker and are also used
 * to locate the file system when part 4 is checked.
 ---------------------------------------------------------------------------*/

UINT32       VDS_Loc, VDS_Len, RVDS_Loc, RVDS_Len;
sPart_Info   Part_Info[NUM_PARTS];
UINT16       PTN_no;             // The number of partition maps in the volume
dstring      LogVolID[128];      // The logical volume ID
MUINT32      VolSpaceListLen = 0; 
struct extent_ad_name VolSpace[MAX_VOL_EXTS];
UINT32      *VAT;
UINT32       VATLength;

/*****************************************************************************
 * File System information
 *
 * These are globals used by the File System checker
 ---------------------------------------------------------------------------*/
struct long_ad FSD;
struct long_ad RootDirICB;
struct long_ad StreamDirICB;
sICB_trk      *ICBlist = NULL;
MUINT32        ICBlist_len = 0;
MUINT32        ICBlist_alloc = 0;
UINT32         ID_Dirs = 0;           // Number of dirs according to LVID
UINT32         ID_Files = 0;          // Number of files according to LVID
UINT32         ID_UID = 0;            // Highest Unique ID according to LVID
unsigned int   Num_Dirs = 0;          // Number of dirs by our count
unsigned int   Num_Files = 0;         // Number of files by our count
unsigned int   Num_Type_Err = 0;
unsigned int   FID_Loc_Wrong = 0;
