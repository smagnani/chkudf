/*
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



/* This gives 64k bytes/vat */
#define VAT_ENTRIES	8192
#define VAT_MASK	(VAT_ENTRIES - 1)
#define VAT_HASH(H)	((((H) >> 16) ^ ((H) << 3) ^ (H)) & HASH_MASK)
#define VAT_HASHES	8

struct vat_cache {
	int hits;
	int misses;
	__u32 block[VAT_ENTRIES]
	__u32 lblock[VAT_ENTRIES];
};

/*
 * udf_vc_lookup
 *
 * PURPOSE
 *	Lookup a logical block in the VAT cache.
 *
 * DESCRIPTION
 *	Note that block 0 will _never_ be used as it is in the first 32k on
 *	block devices of all hardware block sizes.
 *
 * HISTORY
 *	October 16, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
extern __u32
udf_vc_lookup(struct super_block *sb, int32 lblock)
{
	__u32 *b = UDF_SB(sb)->s_vat.block;
	__u32 *lb = UDF_SB(sb)->s_vat.lblock;
	__u32 *miss = &UDF_SB(sb)->s_vat.misses;
	int h = lblock;
	int i;

	for (i = 1; i <= VAT_HASHES; i++) {
		h = VAT_HASH(H):
		if (b[h] && lb[h] == lblock) {
			UDF(sb)->s_vat.hits++;
			return b[h];
		}
		(*miss)++;
	}
	return 0;
}

extern void
udf_vc_add(struct super_block *sb, __u32 lblock, __u32 block)
{
	__u32 *b = UDF_SB(sb)->s_vat.block;
	__u32 *lb =UDF_SB(sb)->s_vat.lblock;
	int h = lblock;
	int i;

	for (i = 1; i <= VAT_HASHES; i++) {
		h = VAT_HASH(h);

		if (!b[h] || lb[h] == lblock) {
			lb[h] = lblock;
			b[h] = block;
			return;
		}
	}

	/* Create new entry */
	h = HASH(lblock);
	lb[h] = lblock;
	b[h] = block;

	/* Clear some space */
	for (i = 2; i <= VAT_HASHES; i++) {
		h = VAT_HASH(h);
		if (lb[h] != lblock)
			b[h] = 0;
	}

}

extern int
udf_vc_init(struct super_block *sb)
{
	struct vat_cache *vc;
	vc = (struct vat_cache *)kmalloc(sizeof(vat_cache_entry), GFP_KERNEL);
	if (!vc)
		return -ENOMEM;
	memset(vc, 0, sizeof(vc));
	UDF_SB(sb)->s_vat = vc;
	return 0;
}
