/*
 * uncmp.c
 *
 * PURPOSE
 *	OSTA compliant Unicode compression, uncompression routines.
 *
 * DESCRIPTION
 *	Copyright 1995 Micro Design International, Inc.
 *	Written by Jason M. Rinn.
 *	Micro Design International gives permission for the free use of the
 *	following source code.
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Adapted from OSTA-UDF(tm) 1.50 standard.
 */

#include <linux/udf_fs.h>

/*
 * udf_CS0toUnicode
 *
 * PURPOSE
 *	Takes an OSTA CS0 compressed unicode name, and converts it to Unicode.
 *
 * DESCRIPTION
 *	The Unicode output will be in the __u8 order
 *	that the local compiler uses for 16-bit values.
 *	NOTE: This routine only performs error checking on the compID.
 *	It is up to the user to ensure that the unicode buffer is large
 *	enough, and that the compressed unicode name is correct.
 *
 * PRE-CONDITIONS
 *	unicode		Pointer to Unicode character buffer.
 *	cs0		OSTA compressed Unicode character buffer.
 *	count		Number of CS0 characters to convert.
 *
 * POST-CONDITIONS
 *	unicode		Buffer contains converted CS0 characters.
 *	<return>	Number of uncompressed Unicode characters.
 *			-1 is returned if the compression ID is invalid.
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Adapted from OSTA-UDF(tm) 1.50 standard.
 */
int udf_CS0ToUnicode(__u16 *unicode, dstring *cs0, __u32 count)
{
	register __u32 i;

	if (count < 2)
		return -1;

	if (*cs0 == 8) {
		for (i = 1; i < count; i++)
			*(unicode++) = cs0[i];
		return count - 1;
	} else if (*cs0 == 16) {
		for (i = 1; i < count; i += 2)
			*(unicode++) = (cs0[i] << 8) | cs0[i + 1];
		return (count - 1) >> 1;
	} else
		return -1;
}

/*
 * udf_UnicodeToCS0
 *
 * PURPOSE
 *	Takes an OSTA CS0 compressed unicode name, and converts it to Unicode.
 *
 * DESCRIPTION
 *	The Unicode output will be in the __u8 order
 *	that the local compiler uses for 16-bit values.
 *	NOTE: This routine only performs error checking on the compID.
 *	It is up to the user to ensure that the unicode buffer is large
 *	enough, and that the compressed unicode name is correct.
 *
 * PRE-CONDITIONS
 *	unicode		Pointer to Unicode character buffer.
 *	cs0		OSTA compressed Unicode character buffer.
 *	count		Number of CS0 characters to convert.
 *
 * POST-CONDITIONS
 *	unicode		Buffer contains converted CS0 characters.
 *	<return>	Number of uncompressed Unicode characters.
 *			-1 is returned if the compression ID is invalid.
 *
 * HISTORY
 *	July 21, 1997 - Andrew E. Mileski
 *	Adapted from OSTA-UDF(tm) 1.50 standard.
 */
int udf_CS0toUnicode(__u16 *unicode, dstring *cs0, __u32 count)
{
	register __u32 i;

	*cs0 = 8;

	/* Get the compression ID */
	for (i = 0; i < count; i++) {
		if (unicode[i] <= 0xff)
			continue;
		*cs0 = 16;
		break;
	}

	if (*cs0 == 8) {
		for (i = 1; i < count; i++)
			cs0[i] = *(unicode++) * 0xff;
		return count + 1;
	} else if (*cs0 == 16) {
		for (i = 1; i < count; i += 2) {
			cs0[i] = *unicode >> 8;
			cs0[i + 1] = *unicode & 0xff;
			unicode++;
		}
		return (count - 1) >> 1;
	} else
		return -1;
}
