#if !defined(DEBUG_H)
/*
 * debug.h
 *
 * PURPOSE
 * 	Debugging code.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

#if defined(DEBUG)

#define DEBUG_CRUMB	printk(KERN_NOTICE "udf/%s:%d\n",__FILE__, __LINE__)
#define DEBUG_DUMP(X)	_debug_dump_block(X)
#define PRINTK(X)	printk X

static void _debug_dump_block(struct buffer_head *bh)
{
	__u8 *b = (__u8 *)bh->b_data;
	int i, t;
	for (i = 0; i < 64; i += 16) {
		printk(KERN_NOTICE "%04lx:%04x ", bh->b_blocknr, i);
		for (t = 0; t < 7; t++)
			printk("%02x ", b[i + t]);
		printk("%02x-", b[i + t]);
		for (; t < 16; t++)
			printk("%02x ", b[i + t]);
		for (t = 0; t < 16; t++) {
			if (b[i + t] >= 0x20U && b[i + t] <= 0x7d)
				printk("%c", b[i + t]);
			else
				printk(".");
		}
		printk("\n");
	}
}

#else /* !defined(DEBUG) */

#define DEBUG_CRUMB	do { } while(0)
#define DEBUG_DUMP(X)	do { } while(0)
#define PRINTK(X)	do { } while (0)

#endif /* !defined(DEBUG) */
#endif /* !defined(DEBUG_H) */
