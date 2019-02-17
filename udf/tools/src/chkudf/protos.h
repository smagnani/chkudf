#include "chkudf.h"
/* 
 * Function prototypes for all files 
 */

/*****************************************************************************
 * build_scsi.c
 * 
 * This module contains routines to build SCSI CDBs.
 *
 * the scsi_modesense10 function builds a mode sense scsi command in a
 * buffer passed to the routine.  
 *
 * The scsi_read10 command builds a read command in the preallocated
 * buffer.  It returns a pointer to the buffer.
 ****************************************************************************/ 
uint8_t *scsi_modesense10(uint8_t *buffer, int DBD, int PC, int pagecode,
                          int pagelength);

uint8_t *scsi_read10(uint8_t *buffer, int LBA, int length, int sectorsize,
                     int DPO, int FUA, int RelAdr);



/*****************************************************************************
 * checkTag.c
 * 
 * Verifies that the structure is a tag, is in uTagLoc, and has the
 * requested TagID.  The CRC length must be between min and max inclusive.
 * If the TagID is -1, everything but the TagID is checked.  The return
 * value is CHECKTAG_NOT_TAG, CHECKTAG_TAG_DAMAGED, or CHECKTAG_TAG_GOOD to
 * indicate that the routine thinks the data is unlikely to be a tag, 
 * likely to be a tag but has a small problem, or is a good tag.
 ****************************************************************************/

int CheckTag(const struct tag *TagPtr, uint32_t uTagLoc, uint16_t TagID,
             int crc_min, int crc_max);

/*****************************************************************************
 * chkudf.c
 *
 * This module contains no exported functions (just main).
 ****************************************************************************/


/*****************************************************************************
 * cleanup.c
 *
 * The cleanup command frees all memory allocated in the course of execution.
 ****************************************************************************/

void cleanup(void);


/*****************************************************************************
 * display_dirs.c
 *
 * The display_dirs function shows a recursive listing of the directory
 *  structure.
 ****************************************************************************/

int GetRootDir(void);
int DisplayDirs(void);
int GetFID(struct FileIDDesc *FID, const struct FE_or_EFE *fe, uint16_t part,
           uint64_t offset);

/*****************************************************************************
 * do_scsi.c
 *
 * This function issues SCSI commands
 ****************************************************************************/

bool do_scsi(uint8_t *command, int cmd_len, void *buffer, uint32_t in_len,
             uint32_t out_len, uint8_t *sense, int sense_len);

/*****************************************************************************
 * errors.c
 *
 * This function dumps the coded error from the error structure to the
 * display in a human readable form.
 ****************************************************************************/

void DumpError(void);
void ClearError(void);


/*****************************************************************************
 * filespace.c
 *
 * This routine checks file space assignments
 ****************************************************************************/

int track_freespace(uint16_t ptn, uint32_t Location, uint32_t numBlocks);
int track_filespace(uint16_t ptn, uint32_t Location, uint32_t numBytes);
int check_filespace(void);
int check_uniqueid(void);

/*****************************************************************************
 * getMap.c
 *
 * This routine loads the sparing maps if appropriate.
 ****************************************************************************/

void GetMap(void);


/*****************************************************************************
 * getVAT.c
 *
 * This routine loads the VAT if a virtual partition exists.
 ****************************************************************************/

void GetVAT(void);


/*****************************************************************************
 * globals.c
 *
 * The following are global variables used by chkudf.
 ****************************************************************************/

extern uint32_t       blocksize;
extern uint_least8_t  bdivshift;
extern uint32_t       secsize;
extern uint_least8_t  sdivshift;
extern uint_least8_t  s_per_b;
extern uint32_t       packet_size;
extern bool           scsi;
extern int            device;
extern uint32_t       LastSector;
extern bool           LastSectorAccurate;
extern uint32_t       lastSessionStartLBA;
extern bool           isType5;
extern bool           isCDRW;

extern uint8_t        *scsibuf;
extern uint32_t       scsibufsize;
extern uint8_t        cdb[];
extern uint8_t        sensedata[];
extern int            sensebufsize;

extern sCacheData     Cache[];
extern uint_least8_t  bufno;
extern sError         Error;
extern char          *Error_Msgs[];


extern uint16_t       UDF_Version;
extern bool           Version_OK;
extern uint16_t       Serial_No;
extern bool           Serial_OK;
extern bool           Fatal;

extern uint32_t       VDS_Loc, VDS_Len, RVDS_Loc, RVDS_Len;
extern sPart_Info     Part_Info[];
extern uint16_t       PTN_no;
extern dstring        LogVolID[];      // The logical volume ID
extern uint_least32_t VolSpaceListLen;
extern struct extent_ad_name VolSpace[];
extern uint32_t      *VAT;
extern uint32_t       VATLength;

extern struct long_ad FSD;
extern struct long_ad RootDirICB;
extern struct long_ad StreamDirICB;
extern sICB_trk      *ICBlist;
extern uint_least32_t ICBlist_len;
extern uint_least32_t ICBlist_alloc;
extern uint32_t       ID_Dirs;
extern uint32_t       ID_Files;
extern uint64_t       ID_UID;
extern unsigned int   Num_Dirs;
extern unsigned int   Num_Files;
extern unsigned int   Num_Type_Err;
extern unsigned int   FID_Loc_Wrong;


/*****************************************************************************
 * icbspace.c
 *
 * This routine tracks ICBs, link counts, and file space
 ****************************************************************************/

int read_icb(struct FE_or_EFE *FE, uint16_t, uint32_t Location, uint32_t Length,
             int FID, uint16_t characteristics, uint16_t* pPrevCharacteristics);


/*****************************************************************************
 * init.c
 *
 * This routine initializes any global variables that need it.
 ****************************************************************************/

void initialize(void);


/*****************************************************************************
 * linkcount.c
 *
 * This routine checks the link count of the file entries.
 ****************************************************************************/

int TestLinkCount(void);


/*****************************************************************************
 * readSpMap.c
 *
 * This routine reads and verifies a space allocation bitmap or table
 ****************************************************************************/

int ReadPartitionUnallocatedSpaceDescs(void);

/*****************************************************************************
 * read_udf.c
 *
 * This routine is the start of the logical checks.
 ****************************************************************************/

void Check_UDF(void);


/*****************************************************************************
 * setSectorSize.c
 *
 * This routine attempts to determine the sector size, and sets the secsize
 * and sdivshift globals.
 ****************************************************************************/

void SetSectorSize(void);


/*****************************************************************************
 * setFirstSector.c
 *
 * SetFirstSector obtains the start address of the last session for CD/DVD
 * media.
 ****************************************************************************/

void SetFirstSector(void);

/*****************************************************************************
 * setLastSector.c
 *
 * SetLastSector tries a bunch of tricks to find the last sector.  It sets
 * the LastSector and LastSectorAccurate global variables.
 *
 * SetLastSectorAccurate adjusts LastSector after probing the media a bit.
 * It looks for AVDP in a variety of locations, and attempts to identify
 * CD-RW media formatted with fixed packets.  This routine also sets the
 * isCDRW flag based on its guesses.
 ****************************************************************************/

void SetLastSector(void);

void SetLastSectorAccurate(void);


/*****************************************************************************
 * utils.c
 *
 * Miscellaneous small routines.
 * endian32 swaps a 32 bit integer from big endian to little or vice versa.
 ****************************************************************************/

uint32_t endian32(uint32_t toswap);
uint16_t endian16(uint16_t toswap);
uint16_t doCRC(uint8_t *buffer, int n);
int Is_Charspec(const struct charspec *chars);
void printDstring(const uint8_t *start, uint8_t fieldLen);
void printDchars(const uint8_t *start, uint8_t length);
void printCharSpec(struct charspec chars);
void printTimestamp( struct timestamp x);
void printExtentAD(struct extent_ad extent);
void printLongAd(struct long_ad *longad);
unsigned int countSetBits(unsigned int value);

/*****************************************************************************
 * utils_read.c
 *
 * The ReadSectors command performs reading from a logical device or
 * file.  It depends on several global variables, including secsize.
 *
 * The ReadLBlocks command reads blocks from a partition.  It relies on 
 * ReadSectors and several globals, including blocksize.
 *
 * The ReadFileData command reads data from a file.  It relies on 
 * ReadLBlocks.
 ****************************************************************************/

int ReadSectors(void *buffer, uint32_t address, uint32_t Count);

int ReadLBlocks(void *buffer, uint32_t address, uint16_t partition, uint32_t Count);

unsigned int ReadFileData(void *buffer, const struct FE_or_EFE *ICB, uint16_t part,
                          uint64_t startOffset, unsigned int bytesRequested,
                          uint32_t *data_start_loc);

/*****************************************************************************
 * verifyAVDP.c
 *
 * These routines read and verify the AVDP.
 ****************************************************************************/

void VerifyAVDP(void);


/*****************************************************************************
 * verifyICB.c
 *
 * These routines read and verify file ICBs.
 ****************************************************************************/

int checkICB(struct FE_or_EFE *fe, struct long_ad FE, int dir);


/*****************************************************************************
 * verifyLVID.c
 *
 * This routine verifies an LVID sequence.
 ****************************************************************************/

int verifyLVID(uint32_t loc, uint32_t len);


/*****************************************************************************
 * verifyRegid.c
 *
 * These routines read and verify various registered identifiers.
 ****************************************************************************/
int CheckRegid(const struct udfEntityId *reg, const char *ID);
void DisplayImplID(struct implEntityId * ieip);
void DisplayUdfID(struct udfEntityId * ueip);
void DisplayRegIDID( struct regid *RegIDp);
void DisplayAppID(struct regid *pAppID);

//void printOSInfo( uint8_t osClass, uint8_t osIdentifier );


/*****************************************************************************
 * verifyVD.c
 *
 * These routines verify the various Volume Descriptors.
 ****************************************************************************/

int checkIUVD(struct ImpUseDesc *mIUVD, struct ImpUseDesc *rIUVD);
int checkLVD(struct LogVolDesc *mLVD, struct LogVolDesc *rLVD);
int checkPD(struct PartDesc *mPD, struct PartDesc *rPD);
int checkPVD(struct PrimaryVolDes *mPVD, struct PrimaryVolDes *rPVD);
int checkUSD(struct UnallocSpDesHead *mUSD, struct UnallocSpDesHead *rUSD);


/*****************************************************************************
 * verifyVDS.c
 *
 * This routine checks the VDS and its contents for validity.  It will store
 * usefule file system information along the way, i.e. partition info.
 * 
 * The name in CheckSequence is for printing to the display.
 ****************************************************************************/

int ReadVDS(uint8_t *VDS, char *name, uint32_t loc, uint32_t len);

int VerifyVDS(void);

/*****************************************************************************
 * verifyVRS.c
 *
 * This routine checks for ISO 9660 and ECMA 167 recognition structures.
 ****************************************************************************/

int VerifyVRS(void);


/*****************************************************************************
 * volspace.c
 *
 * This routine checks for overlapping volume space assignments
 ****************************************************************************/

int track_volspace(uint32_t Location, uint32_t Length, char *Name);
