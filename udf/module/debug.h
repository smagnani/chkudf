#if !defined(_DEBUG_H)
#define _DEBUG_H
/*
 * debug.h
 *
 * PURPOSE
 *	UDF debugging stuff.
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

/* Debugging levels */
#define DEBUG_NONE	0
#define DEBUG_LVL1	1
#define DEBUG_LVL2	2
#define DEBUG_LVL3	3
#define DEBUG_CRUMB	4
#define DEBUG_LVL5	5
#define DEBUG_COOKIE	6
#define DEBUG_LVL7	7
#define DEBUG_LVL8	8
#define DEBUG_LVL9	9
#define DEBUG_DUMP	10

extern int udf_debuglvl;
extern void udf_dump(struct buffer_head *);

#define DPRINTK(X,Y)	do { if (udf_debuglvl >= X) printk Y ; } while(0)
#define PRINTK(X)	do { if (udf_debuglvl >= DEBUG_LVL1) printk X ; } while(0)

#define CRUMB		DPRINTK(DEBUG_CRUMB, ("udf: file \"%s\" line %d\n", __FILE__, __LINE__))
#define COOKIE(X)	DPRINTK(DEBUG_COOKIE, X)

#define DUMP(X)		do { if (udf_debuglvl >= DEBUG_DUMP) udf_dump(X); } while(0)

#endif /* !defined(_DEBUG_H) */
