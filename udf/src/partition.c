#include "udfdecl.h"
#include "udf_sb.h"
#include "udf_i.h"

#ifdef __linux__
#include <linux/fs.h>
#include <linux/string.h>
#else
#include <string.h>
#endif

extern Uint32 udf_get_pblock(struct super_block *sb, Uint32 block, Uint16 partition, Uint32 offset)
{
	if (partition >= UDF_SB_NUMPARTS(sb))
	{
		printk(KERN_DEBUG "udf: udf_get_pblock(%p,%d,%d,%d) invalid partition\n",
			sb, block, partition, offset);
		return 0xFFFFFFFF;
	}
#ifdef VDEBUG
	else
		printk(KERN_DEBUG "udf: udf_get_pblock(%p,%d,%d,%d) type %d\n",
			sb, block, partition, offset, UDF_SB_PARTTYPE(sb, partition));
#endif
	switch (UDF_SB_PARTTYPE(sb, partition))
	{
		case UDF_TYPE1_MAP15:
		{
			return UDF_SB_PARTROOT(sb, partition) + block + offset;
		}
		case UDF_VIRTUAL_MAP15:
		case UDF_VIRTUAL_MAP20:
		{
			struct buffer_head *bh;
			Uint32 newblock;
			Uint32 index;
			Uint32 loc;

			index = (sb->s_blocksize - UDF_SB_TYPEVIRT(sb,partition).s_start_offset) / sizeof(Uint32);

			if (block >= index)
			{
				block -= index;
				newblock = (block / (sb->s_blocksize / sizeof(Uint32)));
				index = block % (sb->s_blocksize / sizeof(Uint32));
			}
			else
			{
				newblock = 0;
				index = UDF_SB_TYPEVIRT(sb,partition).s_start_offset / sizeof(Uint32) + block;
			}

			loc = udf_bmap(UDF_SB_TYPEVIRT(sb,partition).s_vat, newblock);

			if (!(bh = bread(sb->s_dev, loc, sb->s_blocksize)))
			{
				printk(KERN_DEBUG "udf: get_pblock(UDF_VIRTUAL_MAP:%p,%d,%d) VAT: %d[%d]\n",
					sb, block, partition, loc, index);
				return 0xFFFFFFFF;
			}

			loc = ((Uint32 *)bh->b_data)[index];
			udf_release_data(bh);

			if (UDF_I_LOCATION(UDF_SB_TYPEVIRT(sb,partition).s_vat).partitionReferenceNum == partition)
			{
				printk(KERN_DEBUG "udf: recursive call to udf_get_pblock!\n");
				return 0xFFFFFFFF;
			}

			return udf_get_pblock(sb, loc, UDF_I_LOCATION(UDF_SB_TYPEVIRT(sb,partition).s_vat).partitionReferenceNum, offset);
		}
		case UDF_SPARABLE_MAP15:
		{
			Uint32 newblock = UDF_SB_PARTROOT(sb, partition) + block + offset;
			Uint32 spartable = UDF_SB_TYPESPAR(sb, partition).s_spar_loc;
			Uint32 plength = UDF_SB_TYPESPAR(sb,partition).s_spar_plen;
			Uint32 packet = (block + offset) & (~(plength-1));
			struct buffer_head *bh;
			struct SparingTable *st;
			SparingEntry *se;

			bh = udf_read_tagged(sb, spartable, spartable);

			if (!bh)
			{
				printk(KERN_ERR "udf: udf_read_tagged(%p,%d,%d)\n",
					sb, spartable, spartable);
				return 0xFFFFFFFF;
			}

			st = (struct SparingTable *)bh->b_data;
			if (st->descTag.tagIdent == 0)
			{
				if (!strncmp(st->sparingIdent.ident, UDF_ID_SPARING, strlen(UDF_ID_SPARING)))
				{
					Uint16 rtl = st->reallocationTableLen;
					Uint16 index;

					/* If the sparing table span multiple blocks, find out which block we are on */

					se = &(st->mapEntry[0]);

					if (rtl * sizeof(SparingEntry) + sizeof(struct SparingTable) > sb->s_blocksize)
					{
						index = (sb->s_blocksize - sizeof(struct SparingTable)) / sizeof(SparingEntry);
						if (se[index-1].origLocation == packet)
						{
							udf_release_data(bh);
							return se[index].mappedLocation | (newblock & (plength-1));
						}
						else if (se[index-1].origLocation < packet)
						{
							do
							{
								udf_release_data(bh);
								bh = bread(sb->s_dev, spartable, sb->s_blocksize);
								if (!bh)
									return 0xFFFFFFFF;
								se = (SparingEntry *)bh->b_data;
								spartable ++;
								rtl -= index;
								index = sb->s_blocksize / sizeof(SparingEntry);

								if (se[index].origLocation == packet)
								{
									udf_release_data(bh);
									return se[index].mappedLocation | (newblock & (plength-1));
								}
							} while (rtl * sizeof(SparingEntry) > sb->s_blocksize && 
								se[index-1].origLocation < packet);
						}
					}
			
					for (index=0; index<rtl; index++)
					{
						if (se[index].origLocation == packet)
						{
							udf_release_data(bh);
							return se[index].mappedLocation | (newblock & (plength-1));
						}
						else if (se[index].origLocation > packet)
						{
							udf_release_data(bh);
							return newblock;
						}
					}

					udf_release_data(bh);
					return newblock;
				}
			}
			udf_release_data(bh);
		}
	}
	return 0xFFFFFFFF;
}

extern Uint32 udf_get_lb_pblock(struct super_block *sb, lb_addr loc, Uint32 offset)
{
	return udf_get_pblock(sb, loc.logicalBlockNum, loc.partitionReferenceNum, offset);
}
