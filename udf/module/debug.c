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
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include "debug.h"

void
udf_dump(struct buffer_head *bh)
{
	__u8 *b;
	int i, t;

	if (!bh)
		return;

	b = (__u8 *)bh->b_data;

	for (i = 0; i < bh->b_size; i += 16) {
		printk(KERN_DEBUG "%04lx:%04x ", bh->b_blocknr, i);
		for (t = 0; t < 7; t++)
			printk("%02x ", b[i + t]);
		printk("%02x-", b[i + t]);
		for (; t < 16; t++)
			printk("%02x ", b[i + t]);
		for (t = 0; t < 16; t++) {
			if (b[i + t] >= 0x20U && b[i + t] <= 0x7eU)
				printk("%c", b[i + t]);
			else
				printk(".");
		}
		printk("\n");
	}
}
