/*
 * debug.c
 *
 * PURPOSE
 * 	Debugging code.
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
 *	10/4/98 dgb: moved into library
 */



#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/udf_fs.h>

#ifdef __linux__
#define PRINT1(X)	printk(KERN_DEBUG X )
#define PRINT2(X,Y)	printk(KERN_DEBUG X,Y )
#endif

#else
#include <sys/types.h>
#include <stdio.h>
#include <linux/udf_fs.h>
#define PRINT1(X)	fprintf(stderr, X )
#define PRINT2(X,Y)	fprintf(stderr, X,Y )
#endif


void
udf_dump(char * buffer, int size)
{
	char *b;
	int i, t;

	if (!buffer)
		return;

	b = buffer;

	for (i = 0; i < size; i += 16) {
		PRINT2(":%04x ", i);
		for (t = 0; t < 7; t++)
			PRINT2("%02x ", b[i + t]);
		PRINT2("%02x-", b[i + t]);
		for (; t < 16; t++)
			PRINT2("%02x ", b[i + t]);
		for (t = 0; t < 16; t++) {
			if (b[i + t] >= 0x20U && b[i + t] <= 0x7eU)
				PRINT2("%c", b[i + t]);
			else
				PRINT1(".");
		}
		PRINT1("\n");
	}
}
