#if !defined(_LINUX_UDF_UDF_H)
#define _LINUX_UDF_UDF_H
/*
 * udf_udf.h
 *
 * PURPOSE
 *	OSTA-UDF(tm) format specification [based on ECMA 167 standard].
 *	http://www.osta.org/
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team mailing list (run by majordomo):
 *		linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 * 10/2/98 dgb	changed UDF_ID_DEVELOPER
 */

/* -------- Basic types and constants ----------- */

/* UDF character set (UDF 1.50 2.1.2) */
#define UDF_CHAR_SET_TYPE	0
#define UDF_CHAR_SET_INFO	"OSTA Compressed Unicode"

#define UDF_ID_DEVELOPER	"*linux_udf@hootie.lvld.hp.com"

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

#define UDF_NAME_LEN	253
#define UDF_PATH_LEN	1023

#endif /* !defined(_LINUX_UDF_FMT_H) */
