/*
 * chkudf.h configuration parameters
 * NUM_PARTS - Maximum number of partition maps
 * MAX_DEPTH - Maximum depth for recursive directory listing
 * MAX_SECTOR_SIZE - bytes per sector
 * NUM_CACHE - number of cache segments
 * MAX_VOL_EXTS - maximum number of entries in the volume space table
 * ICB_Alloc - number of ICB tracking entries to allocate each time there's
 *             no more space.
 */

#ifndef __CHKUDF_H__
#define __CHKUDF_H__
#define NUM_PARTS       4
#define MAX_DEPTH       16
#define MAX_SECTOR_SIZE 65536
#define NUM_CACHE       4
#define MAX_VOL_EXTS	100
#define ICB_Alloc       1000

/*
 * common inline functions
 */

#define MAX(a,b)((a)>(b)?(a):(b))
#define MIN(a,b)((a)<(b)?(a):(b))
#define BITMAP_NUM_BYTES(numBits)    (((numBits) + 7) >> 3)

#include "../nsrHdrs/nsr.h"
/*----------------------------------------------------------------------------
 * Read cache management - the checker makes no effort to be efficient, so 
 * some read caching is done.
 */

typedef struct _sCacheData {
    UINT8 *Buffer;
    UINT32 Address;
    UINT32 Count;
    UINT32 Allocated;
} sCacheData;


/*----------------------------------------------------------------------------
 * Error reporting
 *   Since some errors are reported and others not (e.g. a "wrong tag" when 
 *   asking for an ICB is different than a "wrong tag" when searching the 
 *   VDS), the error is recorded and the caller can decide to ignore or 
 *   report.
 */

typedef struct _sError {
    int    Code;
    UINT32 Sector;
    int    Expected;
    int    Found;
} sError;

/* From CheckTag */
#define CHECKTAG_TAG_GOOD      0   /* The tag is good in every way */
#define CHECKTAG_TAG_DAMAGED   1   /* The tag is probably the intended one, but wasn't right */
#define CHECKTAG_OK_LIMIT      10  /* Less than this value, tag is usable. */
#define CHECKTAG_WRONG_TAG     11
#define CHECKTAG_NOT_TAG       12

/*----------------------------------------------------------------------------
 * Partition management
 */

typedef struct _sPart_Info {
                int     type;    //One of PTN_TYP
                UINT16  Num;     //Physical partition number
                UINT32  Offs;    //Offset of physical partition
                UINT32  Len;     //Length (for error checking)
                UINT16  SpaceTag;//Tag expected for space map
                UINT8   FinalMapByteMask; // Valid bits in final space map byte
                UINT32  Space;   //Address of space map/list
                UINT32  SpLen;   //Number of bytes in space map/list
                UINT32 *Extra;   //Pointer to VAT or sparing table, or ??.
                UINT8  *SpMap;   //Space Allocation map
                UINT8  *MyMap;   //Space Allocation map generated by chkudf
} sPart_Info;

#define PTN_TYP_NONE    0
#define PTN_TYP_REAL    1
#define PTN_TYP_VIRTUAL 2
#define PTN_TYP_SPARE   3

typedef struct _sMap_Entry {
		UINT32  Original;
                UINT32  Mapped;
} sMap_Entry;

typedef struct _sST_desc {
                UINT32  Size;        //Number of bytes allocated, changed to
                                     // number of sectors by getMap.
                int     Count;       //Number of sparing tables
                UINT16  Extent;      //Number of sectors relocated per entry
                UINT32  Location[4]; //Location(s) of sparing table(s)
                sMap_Entry *Map;     //A copy of one of the sparing tables
} sST_desc;


/*----------------------------------------------------------------------------
 * File space and ICB management
 */

typedef struct _sICB_trk {
                UINT32 LBN;
                UINT16 Ptn;
                UINT16 Link;
                UINT16 LinkRec;
                UINT32 UniqueID_L;
                UINT32 FE_LBN;
                UINT16 FE_Ptn;
} sICB_trk;

/*----------------------------------------------------------------------------
 * Volume space management
 */
struct extent_ad_name {
    UINT32 Length;
    UINT32 Location;
    char * Name;
};

/* 
 * for errors.c ------------------------------------------------------------
 */

#define ERR_NONE             0
#define ERR_TAGID            1
#define ERR_TAGLOC           2
#define ERR_TAGCHECKSUM      3
#define ERR_TAGCRC           4
#define ERR_NOAVDP           5
#define ERR_NO_VD            6
#define ERR_END_VDS          7
#define ERR_TOO_MANY_PARTS   8
#define ERR_READ             9
#define ERR_NOVAT           10
#define ERR_NOVATMEM        11
#define ERR_NO_VIRT_PTN     12
#define ERR_NO_FSD          13
#define ERR_CRC_LENGTH      14
#define ERR_VDS_NOT_EQUIVALENT     15
#define ERR_AVDP_NOT_EQUIVALENT    16
#define ERR_VOL_SPACE_OVERLAP      17
#define ERR_NO_SPARE_PTN           18
#define ERR_NSR_VERSION            19
#define ERR_NOMAPMEM               20
#define ERR_NO_MAP                 21
#define ERR_NO_VDS                 22
#define ERR_NO_VD_MEM              23
#define ERR_FILE_SPACE_OVERLAP     24
#define ERR_NO_ICB_MEM             25
#define ERR_BAD_AD                 26
#define ERR_BAD_PTN                27
#define ERR_BAD_LBN		   28
#define ERR_SEQ_ALLOC              29
#define ERR_SERIAL                 30
#define ERR_PROHIBITED_EXTENT_TYPE 31
#define ERR_PROHIBITED_AD_TYPE     32
#define ERR_UNSORTED_EXTENTS       33
#endif
