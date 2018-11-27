#ifndef NSR_H
#define NSR_H
/********************************************************************/
/*  nsr.h - NSR structures, tags and definitions                    */
/*                                                                  */
/*      © Copyright 1996 Hewlett-Packard Development Company, L.P   */                         
/*                                                                  */
/*  Permission is hereby granted, free of charge, to any person     */    
/*  obtaining a copy of this software and associated documentation  */
/*  files (the "Software"), to deal in the Software without         */
/*  restriction, including without limitation the rights to use,    */
/*  copy, modify, merge, publish, distribute, sublicense, and/or    */
/*  sell copies of the Software, and to permit persons to whom the  */
/*  Software is furnished to do so, subject to the following        */
/*  conditions:                                                     */
/*                                                                  */
/*  The above copyright notice and this permission notice shall     */
/*  be included in all copies or substantial portions of the        */
/*  Software.                                                       */
/*                                                                  */
/*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, */
/*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES */
/*  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND        */
/*  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT     */
/*  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,    */
/*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    */
/*  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR   */
/*  OTHER DEALINGS IN THE SOFTWARE.                                 */
/*                                                                  */
/********************************************************************/

#include "nsr_sys.h"
#include "nsr_part1.h"
#include "nsr_part2.h"
#include "nsr_part3.h"
#include "nsr_part4.h"
#include "udf.h"

#define byteClear   0x00

#define BITZERO     0x01
#define BITONE      0x02
#define BITTWO      0x04
#define BITTHREE    0x08
#define BITFOUR     0x10
#define BITFIVE     0x20
#define BITSIX      0x40
#define BITSEVEN    0x80
#define BITEIGHT    0x100
#define BITNINE     0x200
#define BITTEN      0x400
#define BITELEVEN   0x800
#define BITTWELVE   0x1000
#define BITTHIRTEEN 0x2000
#define BITFOURTEEN 0x4000
#define BITFIFTEEN  0x8000


#define bitsPerByte 0x08

#define FALSE 0
#define TRUE  1

/* tag id's for ISO/IEC 13346 structures */

#define TAGID_NONE             (UINT16) 0   /* no tag */
#define TAGID_PVD              (UINT16) 1   /* primary volume desc */
#define TAGID_ANCHOR           (UINT16) 2   /* anchor desc */
#define TAGID_POINTER          (UINT16) 3   /* pointer desc */
#define TAGID_IUD              (UINT16) 4   /* implementation use desc */
#define TAGID_PD               (UINT16) 5   /* volume partition desc */
#define TAGID_LVD              (UINT16) 6   /* logical volume desc */
#define TAGID_USD              (UINT16) 7   /* unallocated volume space desc */
#define TAGID_TERM_DESC        (UINT16) 8   /* terminator desc */
#define TAGID_LVID             (UINT16) 9   /* logical volume integrity desc */
#define TAGID_FSD              (UINT16) 256  /* file set desc */
#define TAGID_FILE_ID          (UINT16) 257  /* file identifier desc */
#define TAGID_ALLOC_EXTENT     (UINT16) 258  /* Allocation extent desc */
#define TAGID_INDIRECT         (UINT16) 259  /* Indirect entry */
#define TAGID_TERM_ENTRY       (UINT16) 260  /* Terminal entry */
#define TAGID_FILE_ENTRY       (UINT16) 261  /* File entry */
#define TAGID_EXT_ATTR         (UINT16) 262  /* Extended attribute desc */
#define TAGID_UNALLOC_SP_ENTRY (UINT16) 263  /* Unallocated space entry (WORM)*/
#define TAGID_SPACE_BMAP       (UINT16) 264  /* Space bitmap desc */
#define TAGID_PART_INTEGRITY   (UINT16) 265  /* Partition integrity desc */
#define TAGID_EXT_FILE_ENTRY   (UINT16) 266  /* Extended File entry */

/* Stuff for display tool */

#define OSTA_LVINFO_ID ("*UDF LV Info")
#define USE_HdrLen     (sizeof(struct UnallocSpEntry))
#define USE_ADStart(x) (void *)((char *)(x)+USE_HdrLen)
#define FE_EAStart(x)  (void *)((char *)(x)+(sizeof(struct FileEntry)))
#define FE_HdrLen(x)   (sizeof(struct FileEntry)+(x)->L_EA)
#define FE_ADStart(x)  (void *)((char *)(x)+FE_HdrLen(x))
#define AE_HdrLen      (sizeof(struct AllocationExtentDesc))
#define AE_ADStart(x)  (void *)((char *)(x)+AE_HdrLen)

#define DIRTYREGID   BITZERO
#define PROTECTREGID BITONE

#define WRPROTECT_HARD BITZERO
#define WRPROTECT_SOFT BITONE

#endif  /* NSR_H */
