/*
 * unicode.c
 *
 * PURPOSE
 *	Routines for converting between UTF-8 and OSTA Compressed Unicode.
 *      Also handles filename mangling
 *
 * DESCRIPTION
 *	OSTA Compressed Unicode is explained in the OSTA UDF specification.
 *		http://www.osta.org/
 *	UTF-8 is explained in the IETF RFC XXXX.
 *		ftp://ftp.internic.net/rfc/rfcxxxx.txt
 *
 * CONTACTS
 *	E-mail regarding any portion of the Linux UDF file system should be
 *	directed to the development team's mailing list (run by majordomo):
 *		linux_udf@hootie.lvld.hp.com
 *
 * COPYRIGHT
 *	This file is distributed under the terms of the GNU General Public
 *	License (GPL). Copies of the GPL can be obtained from:
 *		ftp://prep.ai.mit.edu/pub/gnu/GPL
 *	Each contributing author retains all rights to their own work.
 */


#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>       /* for memset */
#else
#include <string.h>
#endif

#include "udfdecl.h"

/*
 * udf_build_ustr
 */
int udf_build_ustr(struct ustr *dest, dstring *ptr, int size)
{
    int usesize;

	if ( (!dest) || (!ptr) || (!size) )
		return -1;

	memset(dest, 0, sizeof(struct ustr));
	usesize= (size > UDF_NAME_LEN) ? UDF_NAME_LEN : size;
	dest->u_cmpID=ptr[0];
	dest->u_len=ptr[size-1];
	memcpy(dest->u_name, ptr+1, usesize-1);
	return 0;
}

/*
 * udf_build_ustr_exact
 */
int udf_build_ustr_exact(struct ustr *dest, dstring *ptr, int exactsize)
{
    if ( (!dest) || (!ptr) || (!exactsize) )
	return -1;

    memset(dest, 0, sizeof(struct ustr));
    dest->u_cmpID=ptr[0];
    dest->u_len=exactsize;
    memcpy(dest->u_name, ptr+1, exactsize-1);
    return 0;
}

/*
 * udf_ocu_to_udf8
 *
 * PURPOSE
 *	Convert OSTA Compressed Unicode to the UTF-8 equivalent.
 *
 * DESCRIPTION
 *	This routine is only called by udf_filldir().
 *
 * PRE-CONDITIONS
 *	utf			Pointer to UTF-8 output buffer.
 *	ocu			Pointer to OSTA Compressed Unicode input buffer
 *				of size UDF_NAME_LEN bytes.
 * 				both of type "struct ustr *"
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	November 12, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_CS0toUTF8(struct ustr *utf_o, struct ustr *ocu_i)
{
	char *ocu;
	unsigned c, cmp_id, ocu_len;
	int i;

	ocu = ocu_i->u_name;

	ocu_len = ocu_i->u_len;
	cmp_id = ocu_i->u_cmpID;
	utf_o->u_len = 0;

	if (ocu_len == 0)
	{
		memset(utf_o, 0, sizeof(struct ustr));
		utf_o->u_cmpID = 0;
		utf_o->u_len = 0;
		return 0;
	}

	if ((cmp_id != 8) && (cmp_id != 16))
	{
#ifdef __KERNEL__
		printk(KERN_ERR "udf: unknown compression code (%d) stri=%s\n", cmp_id, ocu_i->u_name);
#endif
		return -1;
	}

	for (i = 0; (i < ocu_len) && (utf_o->u_len < UDF_NAME_LEN) ;) {

		/* Expand OSTA compressed Unicode to Unicode */
		c = ocu[i++];
		if (cmp_id == 16)
		{
			c = (c << 8) | ocu[i++];
#ifdef __KERNEL__
			if (c & 0xFF00)
				printk(KERN_DEBUG "udf: udf_CS0toUTF8: cmd_id == 16 (0x%2x%2x)\n",
					((c >> 8) & 0xFF), (c & 0xFF));
#endif
		}

		/* Compress Unicode to UTF-8 */
		if (c < 0x80U)
			utf_o->u_name[utf_o->u_len++] = (char)c;
		else if (c < 0x800U) {
			utf_o->u_name[utf_o->u_len++] = (char)(0xc0 | (c >> 6));
			utf_o->u_name[utf_o->u_len++] = (char)(0x80 | (c & 0x3f));
#ifdef __KERNEL__
			printk(KERN_DEBUG "udf: udf_CS0toUTF8 (0x%2x%2x) -> (%2x) (%2x)\n",
				((c >> 8) & 0xFF), (c & 0xFF),
				utf_o->u_name[utf_o->u_len-2],
				utf_o->u_name[utf_o->u_len-1]);
#endif
		} else {
			utf_o->u_name[utf_o->u_len++] = (char)(0xc0 | (c >> 12));
			utf_o->u_name[utf_o->u_len++] = (char)(0x80 | ((c >> 6) & 0x3f));
			utf_o->u_name[utf_o->u_len++] = (char)(0x80 | (c & 0x3f));
#ifdef __KERNEL__
			printk(KERN_DEBUG "udf: udf_CS0toUTF8 (0x%2x%2x) -> (%2x) (%2x) (%2x)\n",
				((c >> 8) & 0xFF), (c & 0xFF),
				utf_o->u_name[utf_o->u_len-3],
				utf_o->u_name[utf_o->u_len-2],
				utf_o->u_name[utf_o->u_len-1]);
#endif
		}
	}
	utf_o->u_cmpID=8;
	utf_o->u_hash=0L;
	utf_o->padding=0;

	return 0;
}

/*
 *
 * udf_utf8_to_ocu
 *
 * PURPOSE
 *	Convert UTF-8 to the OSTA Compressed Unicode equivalent.
 *
 * DESCRIPTION
 *	This routine is only called by udf_lookup().
 *
 * PRE-CONDITIONS
 *	ocu			Pointer to OSTA Compressed Unicode output
 *				buffer of size UDF_NAME_LEN bytes.
 *	utf			Pointer to UTF-8 input buffer.
 *	utf_len			Length of UTF-8 input buffer in bytes.
 *
 * POST-CONDITIONS
 *	<return>		Zero on success.
 *
 * HISTORY
 *	November 12, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int udf_UTF8toCS0(dstring *ocu, struct ustr *utf, int length)
{
	unsigned c, i, max_val, utf_char;
	int utf_cnt;
	int u_len = 0;

	memset(ocu, 0, sizeof(dstring) * length);
	ocu[0] = 8;
	max_val = 0xffU;

try_again:
	utf_char = 0U;
	utf_cnt = 0U;
	for (i = 0U; i < utf->u_len; i++) {
		c = (unsigned)utf->u_name[i];

		/* Complete a multi-byte UTF-8 character */
		if (utf_cnt) {
			utf_char = (utf_char << 6) | (c & 0x3fU);
			if (--utf_cnt)
				continue;
		} else {
			/* Check for a multi-byte UTF-8 character */
			if (c & 0x80U) {
				/* Start a multi-byte UTF-8 character */
				if ((c & 0xe0U) == 0xc0U) {
					utf_char = c & 0x1fU;
					utf_cnt = 1;
				} else if ((c & 0xf0U) == 0xe0U) {
					utf_char = c & 0x0fU;
					utf_cnt = 2;
				} else if ((c & 0xf8U) == 0xf0U) {
					utf_char = c & 0x07U;
					utf_cnt = 3;
				} else if ((c & 0xfcU) == 0xf8U) {
					utf_char = c & 0x03U;
					utf_cnt = 4;
				} else if ((c & 0xfeU) == 0xfcU) {
					utf_char = c & 0x01U;
					utf_cnt = 5;
				} else
					goto error_out;
				continue;
			} else
				/* Single byte UTF-8 character (most common) */
				utf_char = c;
		}

		/* Choose no compression if necessary */
		if (utf_char > max_val) {
			if ( 0xffU == max_val ) {
				max_val = 0xffffU;
				ocu[0] = (Uint8)0x10U;
				goto try_again;
			}
			goto error_out;
		}

		if (max_val == 0xffffU)
			ocu[++u_len] = (Uint8)(utf_char >> 8);
		ocu[++u_len] = (Uint8)(utf_char & 0xffU);
	}

	if (utf_cnt) {
error_out:
#ifdef __KERNEL__
		printk(KERN_ERR "udf: bad UTF-8 character\n");
#endif
		return -1;
	}

	ocu[length - 1] = (Uint8)u_len;
	return 0;
}

#ifdef __KERNEL__
int udf_get_filename(char *sname, char *dname, int flen)
{
	struct ustr filename;
	struct ustr unifilename;
	int len;

	if (udf_build_ustr_exact(&unifilename, sname, flen))
	{
		return 0;
	}

	if (udf_CS0toUTF8(&filename, &unifilename) )
	{
		printk(KERN_DEBUG "udf: udf_CS0toUTF8 failed in udf_get_filename: sname = %s\n", sname);
		return 0;
	}

	if ((len = udf_translate_to_linux(dname, filename.u_name, filename.u_len-1,
		unifilename.u_name, unifilename.u_len)))
	{
		return len;
	}
	return 0;
}
#endif

#define ILLEGAL_CHAR_MARK	'_'
#define EXT_MARK			'.'
#define CRC_MARK			'#'
#define EXT_SIZE			5

int udf_translate_to_linux(char *newName, char *udfName, int udfLen, char *fidName, int fidNameLen)
{
	int index, newIndex = 0, needsCRC = 0;	
	int extIndex = 0, newExtIndex = 0, hasExt = 0;
	unsigned short valueCRC;
	char current;
	const char hexChar[] = "0123456789ABCDEF";

	if (udfName[0] == '.' && (udfLen == 1 ||
		(udfLen == 2 && udfName[1] == '.')))
	{
		needsCRC = 1;
		newIndex = udfLen;
		memcpy(newName, udfName, udfLen);
	}
	else
	{	
		for (index = 0; index < udfLen; index++)
		{
			current = udfName[index];
			if (current == '/' || current == 0)
			{
				needsCRC = 1;
				current = ILLEGAL_CHAR_MARK;
				while (index+1 < udfLen && (udfName[index+1] == '/' ||
					udfName[index+1] == 0))
					index++;
			}
			if (current == EXT_MARK && (udfLen - index - 1) <= EXT_SIZE)
			{
				if (udfLen == index + 1)
					hasExt = 0;
				else
				{
					hasExt = 1;
					extIndex = index;
					newExtIndex = newIndex;
				}
			}
			if (newIndex < 256)
				newName[newIndex++] = current;
			else
				needsCRC = 1;
		}
	}
	if (needsCRC)
	{
		char ext[EXT_SIZE];
		int localExtIndex = 0;

		if (hasExt)
		{
			int maxFilenameLen;
			for(index = 0; index<EXT_SIZE && extIndex + index +1 < udfLen;
				index++ )
			{
				current = udfName[extIndex + index + 1];

				if (current == '/' || current == 0)
				{
					needsCRC = 1;
					current = ILLEGAL_CHAR_MARK;
					while(extIndex + index + 2 < udfLen && (index + 1 < EXT_SIZE
						&& (udfName[extIndex + index + 2] == '/' ||
							udfName[extIndex + index + 2] == 0)))
						index++;
				}
				ext[localExtIndex++] = current;
			}
			maxFilenameLen = 250 - localExtIndex;
			if (newIndex > maxFilenameLen)
				newIndex = maxFilenameLen;
			else
				newIndex = newExtIndex;
		}
		else if (newIndex > 250)
			newIndex = 250;
		newName[newIndex++] = CRC_MARK;
		valueCRC = udf_crc(fidName, fidNameLen, 0);
		newName[newIndex++] = hexChar[(valueCRC & 0xf000) >> 12];
		newName[newIndex++] = hexChar[(valueCRC & 0x0f00) >> 8];
		newName[newIndex++] = hexChar[(valueCRC & 0x00f0) >> 4];
		newName[newIndex++] = hexChar[(valueCRC & 0x000f)];

		if (hasExt)
		{
			newName[newIndex++] = EXT_MARK;
			for (index = 0;index < localExtIndex ;index++ )
				newName[newIndex++] = ext[index];
		}
	}
	return newIndex;
}
