#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include <malloc.h>
#include <stdio.h>

/*****************************************************************************
 * Device operating parameters
 *
 * These are globals used when referencing the device or image file.
 ---------------------------------------------------------------------------*/

uint32_t       blocksize = 0;               // bytes per sector
uint_least8_t  bdivshift = 0;               // log2(blocksize)
uint32_t       secsize = 0;                 // bytes per sector
uint_least8_t  sdivshift = 0;               // log2(secsize)
uint_least8_t  s_per_b = 1;                 // blocksize/secsize
uint32_t       packet_size;                 // blocking factor for read operations
bool           scsi = false;                // Boolean for command selection
int            device = 0;                  // Device/file handle for operations
uint32_t       LastSector = 0;              // Location of the last readable sector
bool           LastSectorAccurate = false;  // Indication of confidence
uint32_t       lastSessionStartLBA = 0;     // Start of last session
bool           isType5 = false;             // Is a CD or DVD drive
bool           isCDRW = false;

uint8_t        *scsibuf = NULL;             // Used for scsi scratchpad
uint32_t       scsibufsize = 0;             // amount allocated for SCSI buffer
uint8_t        cdb[12];                     // command buffer
uint8_t        sensedata[18];               // Sense data buffer
int            sensebufsize = 18;           // Sense data buffer size

/*****************************************************************************
 * chkudf operating parameters
 *
 * These are globals used by low level routines within chkudf, and aren't
 * directly related to the file system.
 ---------------------------------------------------------------------------*/

char          g_defaultAnswer;
bool          g_bVerbose;
bool          g_bDebug;
uint8_t       g_exitStatus;
sCacheData    Cache[NUM_CACHE];
uint_least8_t bufno = 0;
sError        Error = {0, 0, 0, 0};

ErrorSeverity Error_Msgs[] = {
/*  1 */  { "Expected Tag ID of %lld, found %lld",               EXIT_UNCORRECTED_ERRORS },
          { "Expected Tag location of %08llx, read %08llx",      EXIT_UNCORRECTED_ERRORS },
          { "Expected Tag checksum of %02llx, found %02llx",     EXIT_UNCORRECTED_ERRORS },
          { "Expected Tag CRC of %04llx, found %04llx",          EXIT_UNCORRECTED_ERRORS },
/*  5 */  { "Not an Anchor Volume Descriptor Pointer",           EXIT_UNCORRECTED_ERRORS },
          { "UNUSED", 0 },
          { "UNUSED", 0 },
          { "This program can handle %lld partitions and the logical volume has %lld", EXIT_OPERATIONAL_ERROR },
          { "Error reading sector",                              EXIT_OPERATIONAL_ERROR  },
/* 10 */  { "No VAT present",                                    EXIT_UNCORRECTED_ERRORS },
          { "Not able to allocate memory for VAT",               EXIT_OPERATIONAL_ERROR  },
          { "UNUSED", 0 },
          { "No file set descriptor found",                      EXIT_UNCORRECTED_ERRORS },
          { "Tag CRC length limit is %04llx, found %04llx",      EXIT_UNCORRECTED_ERRORS },
/* 15 */  { "Volume Descriptor Sequences are not equivalent",    EXIT_MINOR_UNCORRECTED_ERRORS },
          { "Anchor Volume Descriptor Pointers are not equivalent", EXIT_MINOR_UNCORRECTED_ERRORS },
          { "Volume Space overlap detected",                     EXIT_UNCORRECTED_ERRORS },
          { "UNUSED", 0},
          { "NSR descriptor version should be %lld, was %lld",   EXIT_UNCORRECTED_ERRORS },
/* 20 */  { "Not able to allocate memory for Sparing Map",       EXIT_OPERATIONAL_ERROR  },
          { "Specified location does not contain a Sparing Map", EXIT_UNCORRECTED_ERRORS },
          { "UNUSED", 0 },
          { "Can't allocate memory for Volume Descriptors",      EXIT_OPERATIONAL_ERROR  },
          { "Partition Space overlap detected",                  EXIT_UNCORRECTED_ERRORS },
/* 25 */  { "No more memory for ICB tracking available",         EXIT_OPERATIONAL_ERROR  },
          { "Expected Allocation Descriptors for %lld bytes, found %lld",           EXIT_UNCORRECTED_ERRORS },
          { "%lld Partitions found, Partition Reference Number %lld out of range",  EXIT_UNCORRECTED_ERRORS },
          { "%lld blocks in Partition, Logical Block Number %lld out of range",     EXIT_UNCORRECTED_ERRORS },
          { "Adjacent Allocation Descriptors found (descriptor for %lld)",          EXIT_MINOR_UNCORRECTED_ERRORS },
/* 30 */  { "Expected Serial number of %lld, found %lld. (disabling reporting)",    EXIT_UNCORRECTED_ERRORS },
          { "Expected Extent Type %lld, found prohibited type %lld",                EXIT_UNCORRECTED_ERRORS },
          { "Expected AD Type %lld, found prohibited type %lld",                    EXIT_UNCORRECTED_ERRORS },
          { "Unallocated extents not sorted in ascending order",                    EXIT_MINOR_UNCORRECTED_ERRORS },
          { "Found VAT ICB. Unfortunately, code to process it does not yet exist.", EXIT_OPERATIONAL_ERROR },
/* 35 */  { "Expected AD length %lld, but found unexpected zero-length extent at offset %lld.", EXIT_UNCORRECTED_ERRORS },
};



/*****************************************************************************
 * UDF basics
 *
 * These are globals used by the bottom level checking; used for both parts
 * 3 and 4.
 ---------------------------------------------------------------------------*/

uint16_t   UDF_Version;
bool       Version_OK = false;
uint16_t   Serial_No;
bool       Serial_OK = false;
bool       Fatal = false;


/*****************************************************************************
 * Volume information
 *
 * These are globals used by the Volume Space checker and are also used
 * to locate the file system when part 4 is checked.
 ---------------------------------------------------------------------------*/

uint32_t       VDS_Loc, VDS_Len, RVDS_Loc, RVDS_Len;
sPart_Info     Part_Info[NUM_PARTS];
uint16_t       PTN_no;             // The number of partition maps in the volume
dstring        LogVolID[128];      // The logical volume ID
uint_least32_t VolSpaceListLen = 0; 
struct extent_ad_name VolSpace[MAX_VOL_EXTS];
uint32_t      *VAT;
uint32_t       VATLength;

/*****************************************************************************
 * File System information
 *
 * These are globals used by the File System checker
 ---------------------------------------------------------------------------*/
struct long_ad FSD;
struct long_ad RootDirICB;
struct long_ad StreamDirICB;
sICB_trk      *ICBlist = NULL;
uint_least32_t ICBlist_len = 0;
uint_least32_t ICBlist_alloc = 0;
uint32_t       ID_Dirs = 0;           // Number of dirs according to LVID
uint32_t       ID_Files = 0;          // Number of files according to LVID
uint64_t       ID_UID = 0;            // Next Unique ID according to LVID
unsigned int   Num_Dirs = 0;          // Number of dirs by our count
unsigned int   Num_Files = 0;         // Number of files by our count
unsigned int   Num_Type_Err = 0;
unsigned int   FID_Loc_Wrong = 0;
