/*
 * super.c
 *
 * PURPOSE
 *	Super block routines for UDF filesystem.
 *
 * DESCRIPTION
 *	OSTA-UDF(tm) = Optical Storage Technology Association
 *	Universal Disk Format.
 *
 *	This code is based on version 1.50 of the UDF specification,
 *	and revision 2 of the ECMA 167 standard [equivalent to ISO 13346].
 *	http://www.osta.org/	http://www.ecma.ch/	http://www.iso.org/
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
 *
 * 9/24/98 dgb:  changed to allow compiling outside of kernel, and
 *               added some debugging.
 * 10/1/98 dgb:  updated to allow (some) possibility of compiling w/2.0.34
 * 10/16/98      attempting some multi-session support
 * 10/17/98      added freespace count for "df"
 * 11/11/98 gr:  added novrs option
 * 11/26/98 dgb  added fileset,anchor mount options
 * 12/06/98 blf  really hosed things royally. vat/sparing support. sequenced vol descs
 *               rewrote option handling based on isofs
 * 12/20/98      find the free space bitmap (if it exists)
 */

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include "udfdecl.h"    

#include <linux/blkdev.h>
#include <linux/malloc.h>
#include <linux/kernel.h>
#include <linux/locks.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/cdrom.h>
#include <linux/nls.h>
#include <asm/byteorder.h>

#include <linux/udf_fs.h>
#include "udf_sb.h"

#include <linux/init.h>
#include <asm/uaccess.h>

/* These are the "meat" - everything else is stuffing */
static struct super_block *udf_read_super(struct super_block *, void *, int);
static void udf_put_super(struct super_block *);
static int udf_remount_fs(struct super_block *, int *, char *);
static int udf_check_valid(struct super_block *sb, int);
static int udf_vrs(struct super_block *sb, int silent);
static int udf_load_partition(struct super_block *sb, struct AnchorVolDescPtr *, lb_addr *);
static int udf_load_logicalvol(struct super_block *sb, struct buffer_head *, lb_addr *);
static void udf_load_logicalvolint(struct super_block *, extent_ad);
static int udf_find_anchor(struct super_block *,long,struct AnchorVolDescPtr *);
static int udf_find_fileset(struct super_block *, lb_addr *, lb_addr *);
static void udf_load_pvoldesc(struct super_block *sb, struct buffer_head *);
static void udf_load_fileset(struct super_block *sb, struct buffer_head *, lb_addr *);
static void udf_load_partdesc(struct super_block *sb, struct buffer_head *);
static void udf_open_lvid(struct super_block *sb);
static void udf_close_lvid(struct super_block *sb);
static unsigned int udf_count_free(struct super_block *sb);

/* version specific functions */
static int udf_statfs(struct super_block *, struct statfs *, int);

/* UDF filesystem type */
static struct file_system_type udf_fstype = {
	"udf",			/* name */
	FS_REQUIRES_DEV,	/* fs_flags */
	udf_read_super,		/* read_super */
	NULL			/* next */
};

/* Superblock operations */
static struct super_operations udf_sb_ops =
{
	udf_read_inode,		/* read_inode */
#ifdef CONFIG_UDF_RW
	udf_write_inode,	/* write_inode */
#else
	NULL,				/* write_inode */
#endif
	udf_put_inode,		/* put_inode */
#ifdef CONFIG_UDF_RW
	udf_delete_inode,	/* delete_inode */
#else
	NULL,				/* delete_inode */
#endif
	NULL,				/* notify_change */
	udf_put_super,		/* put_super */
	NULL,				/* write_super */
	udf_statfs,			/* statfs */
	udf_remount_fs,		/* remount_fs */
	NULL,				/* clear_inode */
	NULL,				/* umount_begin */
};

struct udf_options
{
	unsigned char novrs;
	unsigned char utf8;
	unsigned int blocksize;
	unsigned int session;
	unsigned int lastblock;
	unsigned int anchor;
	unsigned int volume;
	unsigned short partition;
	unsigned int fileset;
	unsigned int rootdir;
	unsigned int flags;
	mode_t umask;
	gid_t gid;
	uid_t uid;
	char *iocharset;
};

#if defined(MODULE)
	
/*
 * cleanup_module
 *
 * PURPOSE
 *	Unregister the UDF filesystem type.
 *
 * DESCRIPTION
 *	Clean-up before the module is unloaded.
 *	This routine only applies when compiled as a module.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int
cleanup_module(void)
{
	printk(KERN_NOTICE "udf: unregistering filesystem\n");
	return unregister_filesystem(&udf_fstype);
}

/*
 * init_module / init_udf_fs
 *
 * PURPOSE
 *	Register the UDF filesystem type.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
int init_module(void)
#else /* if !defined(MODULE) */
__initfunc(int init_udf_fs(void))
#endif
{
	printk(KERN_NOTICE "udf: registering filesystem\n");
	{
		struct super_block sb;
		int size;

		size = sizeof(struct super_block) +
			(int)&sb.u - (int)&sb;
		if ( size < sizeof(struct udf_sb_info) )
		{
			printk(KERN_ERR "udf: Danger! Kernel was compiled without enough room for udf_sb_info\n");
			printk(KERN_ERR "udf: Kernel has room for %u bytes, udf needs %u\n",
				size, sizeof(struct udf_sb_info));
			return 0;
		}
#ifdef VDEBUG
		udf_debug("sizeof(udf_sb_info) = %u, kernel has %u\n",
			sizeof(struct udf_sb_info), size);
#endif
	}
	return register_filesystem(&udf_fstype);
}

/*
 * udf_parse_options
 *
 * PURPOSE
 *	Parse mount options.
 *
 * DESCRIPTION
 *	The following mount options are supported:
 *
 *	bs=		Set the block size.
 *	fixed		Disable removable media checks.
 *	gid=		Set the default group.
 *	umask=		Set the umask
 *	strict		Set strick conformance.
 *	uid=		Set the default user.
 *	session=	Set the CDROM session sector(default= last)
 *  lastblock=	Override the last sector
 *	anchor=		Override standard anchor location.
 *	volume=		Override the VolumeDesc location.
 *	partition=	Override the PartitionDesc location.
 *	fileset=	Override the fileset block location.
 *	rootdir=	Override the root directory location.
 *      unhide
 *      undelete
 *      novrs           Skip volume sequence recognition (my RW cd recorded by 
 *                       ADAPTEC DirectCD 2.5 has no such sequence but otherwise is fine
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *	opts			Pointer to mount options string.
 *
 * POST-CONDITIONS
 *	<return>	0	Mount options parsed okay.
 *	<return>	-1	Error parsing mount options.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

static int
udf_parse_options(char *options, struct udf_options *uopt)
{
	char *opt, *val;

	uopt->novrs = 0;
	uopt->blocksize = 2048;
	uopt->partition = 0xFFFF;
	uopt->session = 0xFFFFFFFF;
	uopt->lastblock = 0xFFFFFFFF;
	uopt->anchor = 0xFFFFFFFF;
	uopt->volume = 0xFFFFFFFF;
	uopt->rootdir = 0xFFFFFFFF;
	uopt->fileset = 0xFFFFFFFF;
	uopt->iocharset = NULL;

	if (!options)
		return 1;

        for (opt = strtok(options, ","); opt; opt = strtok(NULL, ","))
	{
		/* Make "opt=val" into two strings */
		val = strchr(opt, '=');
		if (val)
			*(val++) = 0;
		if (!strcmp(opt, "novrs") && !val)
			uopt->novrs = 1;
		else if (!strcmp(opt, "utf8") && !val)
			uopt->utf8 = 1;
		else if (!strcmp(opt, "bs") && val)
			uopt->blocksize = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "unhide") && !val)
			uopt->flags |= UDF_FLAG_UNHIDE;
		else if (!strcmp(opt, "undelete") && !val)
			uopt->flags |= UDF_FLAG_UNDELETE;
		else if (!strcmp(opt, "gid") && val)
			uopt->gid = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "umask") && val)
			uopt->umask = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "strict") && !val)
			uopt->flags |= UDF_FLAG_STRICT;
		else if (!strcmp(opt, "uid") && val)
			uopt->uid = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "session") && val)
			uopt->session = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "lastblock") && val)
			uopt->lastblock = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "anchor") && val)
			uopt->anchor = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "volume") && val)
			uopt->volume = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "partition") && val)
			uopt->partition = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "fileset") && val)
			uopt->fileset = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "rootdir") && val)
			uopt->rootdir = simple_strtoul(val, NULL, 0);
		else if (!strcmp(opt, "iocharset") && val)
		{
			uopt->iocharset = val;
			while (*val && *val != ',')
				val ++;
			if (val == uopt->iocharset)
				return 0;
			*val = 0;
		}
		else if (val)
		{
			printk(KERN_ERR "udf: bad mount option \"%s=%s\"\n",
				opt, val);
			return 0;
		}
		else
		{
			printk(KERN_ERR "udf: bad mount option \"%s\"\n",
				opt);
			return 0;
		}
	}
	return 1;
}

static int
udf_remount_fs(struct super_block *sb, int *flags, char *options)
{
	struct udf_options uopt;

	uopt.flags =    UDF_SB(sb)->s_flags ;
	uopt.uid =      UDF_SB(sb)->s_uid ;
	uopt.gid =      UDF_SB(sb)->s_gid ;
	uopt.umask =    UDF_SB(sb)->s_umask ;
	uopt.utf8 =     UDF_SB(sb)->s_utf8 ;

	if ( !udf_parse_options(options, &uopt) )
		return -EINVAL;

	UDF_SB(sb)->s_flags =   uopt.flags;
	UDF_SB(sb)->s_uid =     uopt.uid;
	UDF_SB(sb)->s_gid =     uopt.gid;
	UDF_SB(sb)->s_umask =   uopt.umask;
	UDF_SB(sb)->s_utf8 =    uopt.utf8;

	if ((*flags & MS_RDONLY) == (sb->s_flags & MS_RDONLY))
		return 0;
	if (*flags & MS_RDONLY)
		udf_close_lvid(sb);
	else
		udf_open_lvid(sb);

	return 0;
}

/*
 * udf_set_blocksize
 *
 * PURPOSE
 *	Set the block size to be used in all transfers.
 *
 * DESCRIPTION
 *	To allow room for a DMA transfer, it is best to guess big when unsure.
 *	This routine picks 2048 bytes as the blocksize when guessing. This
 *	should be adequate until devices with larger block sizes become common.
 *
 *	Note that the Linux kernel can currently only deal with blocksizes of
 *	512, 1024, 2048, 4096, and 8192 bytes.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *
 * POST-CONDITIONS
 *	sb->s_blocksize		Blocksize.
 *	sb->s_blocksize_bits	log2 of blocksize.
 *	<return>	0	Blocksize is valid.
 *	<return>	1	Blocksize is invalid.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_set_blocksize(struct super_block *sb, int bsize)
{
	/* Use specified block size if specified */
	sb->s_blocksize = get_hardblocksize(sb->s_dev);
	sb->s_blocksize = sb->s_blocksize ? sb->s_blocksize : 2048;
	if (bsize > sb->s_blocksize)
		sb->s_blocksize = bsize;

	/* Block size must be an even multiple of 512 */
	switch (sb->s_blocksize) {
		case 512: sb->s_blocksize_bits = 9;	break;
		case 1024: sb->s_blocksize_bits = 10; break;
		case 2048: sb->s_blocksize_bits = 11; break;
		case 4096: sb->s_blocksize_bits = 12; break;
		case 8192: sb->s_blocksize_bits = 13; break;
		default:
		{
			udf_debug("Bad block size (%ld)\n", sb->s_blocksize);
			printk(KERN_ERR "udf: bad block size (%ld)\n", sb->s_blocksize);
			return 0;
		}
	}

	/* Set the block size */
	set_blocksize(sb->s_dev, sb->s_blocksize);
	return sb->s_blocksize;
}

static int
udf_vrs(struct super_block *sb, int silent)
{
	struct VolStructDesc *vsd = NULL;
	int block_inc = 2048 >> sb->s_blocksize_bits;
	int sector = 32768 >> sb->s_blocksize_bits; 

	struct buffer_head *bh;
	int iso9660=0;
	int nsr02=0;
	int nsr03=0;

	/* Block size must be a multiple of 512 */
	if (sb->s_blocksize & 511)
		return sector;

	sector += UDF_SB_SESSION(sb);

	udf_debug("Starting at sector %u (%ld byte sectors)\n", sector, sb->s_blocksize);
	/* Process the sequence (if applicable) */
	for (;!nsr02 && !nsr03;sector += block_inc)
	{
		/* Read a block */
		bh = udf_bread(sb, sector, sb->s_blocksize);
		if (!bh)
			break;

		/* Look for ISO  descriptors */
		vsd = (struct VolStructDesc *)bh->b_data;

		if (vsd->stdIdent[0] == 0)
			break;
		else if (!strncmp(vsd->stdIdent, STD_ID_CD001, STD_ID_LEN))
		{
			iso9660 = sector;
			switch (vsd->structType)
			{
				case 0: 
					udf_debug("ISO9660 Boot Record found\n");
					break;
				case 1: 
					udf_debug("ISO9660 Primary Volume Descriptor found\n");
					break;
				case 2: 
					udf_debug("ISO9660 Supplementary Volume Descriptor found\n");
					break;
				case 3: 
					udf_debug("ISO9660 Volume Partition Descriptor found\n");
					break;
				case 255: 
					udf_debug("ISO9660 Volume Descriptor Set Terminator found\n");
					break;
				default: 
					udf_debug("ISO9660 VRS (%u) found\n", vsd->structType);
					break;
			}
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_BEA01, STD_ID_LEN))
		{
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_TEA01, STD_ID_LEN))
		{
			break;
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_NSR02, STD_ID_LEN))
		{
			nsr02 = sector;
		}
		else if (!strncmp(vsd->stdIdent, STD_ID_NSR03, STD_ID_LEN))
		{
			nsr03 = sector;
		}
		brelse(bh);
	}

	if (nsr03)
		return nsr03;
	else if (nsr02)
		return nsr02;
	else
		return 0;
}

/*
 * udf_find_anchor
 *
 * PURPOSE
 *	Find an anchor volume descriptor.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *	lastblock		Last block on media.
 *
 * POST-CONDITIONS
 *	<return>		1 if not found, 0 if ok
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_find_anchor(struct super_block *sb, long lastblock, 
	struct AnchorVolDescPtr *ap)
{
	struct buffer_head *bh = NULL;
	Uint16 ident;
	Uint32 anchorlist[4];
	int i;

	anchorlist[0] = 256 + UDF_SB_SESSION(sb);
	anchorlist[1] = 512 + UDF_SB_SESSION(sb);
	anchorlist[2] = lastblock;
	anchorlist[3] = lastblock - 256;

	/* Search for an anchor volume descriptor pointer */

	/*  according to spec, anchor is in either:
	 *     block 256
	 *     lastblock-256
	 *     lastblock
	 *  however, if the disc isn't closed, it could be 512 */

	for (i=0; i<4; i++)
	{
		if (bh)
			udf_release_data(bh);
		bh = udf_read_tagged(sb, anchorlist[i], anchorlist[i], &ident);
		if (ident == TID_ANCHOR_VOL_DESC_PTR)
			break;
	}

	if (!bh || ident != TID_ANCHOR_VOL_DESC_PTR)
	{
		udf_release_data(bh);
		udf_debug("Couldn't find an anchor\n");
		return 1;
	}
	UDF_SB_ANCHOR(sb) = anchorlist[i];
	memcpy(ap, bh->b_data, sizeof(struct AnchorVolDescPtr));
	udf_release_data(bh);
	return 0;
}

static int 
udf_find_fileset(struct super_block *sb, lb_addr *fileset, lb_addr *root)
{
	struct buffer_head *bh=NULL;
	long lastblock;
	Uint16 ident;

	if (fileset->logicalBlockNum != 0xFFFFFFFF ||
		fileset->partitionReferenceNum != 0xFFFF)
	{
		udf_debug("Fileset at block=%d, partition=%d\n",
				fileset->logicalBlockNum, fileset->partitionReferenceNum);

		bh = udf_read_ptagged(sb, *fileset, 0, &ident);

		if (!bh)
			return 1;
		else if (ident != TID_FILE_SET_DESC)
		{
			udf_release_data(bh);
			return 1;
		}
			
	}
	else /* Search backwards through the partitions */
	{
		lb_addr newfileset;

		return 1;
		
		for (newfileset.partitionReferenceNum=UDF_SB_NUMPARTS(sb)-1;
			(newfileset.partitionReferenceNum != 0xFFFF &&
				fileset->logicalBlockNum == 0xFFFFFFFF &&
				fileset->partitionReferenceNum == 0xFFFF);
			newfileset.partitionReferenceNum--)
		{
			lastblock = UDF_SB_PARTLEN(sb, newfileset.partitionReferenceNum);
			newfileset.logicalBlockNum = 0;

			do
			{
				bh = udf_read_ptagged(sb, newfileset, 0, &ident);
				if (!bh)
				{
					newfileset.logicalBlockNum ++;
					continue;
				}

				switch (ident)
				{
					case TID_SPACE_BITMAP_DESC:
					{
						struct SpaceBitmapDesc *sp;
						sp = (struct SpaceBitmapDesc *)bh->b_data;
						newfileset.logicalBlockNum += 1 +
							((le32_to_cpu(sp->numOfBytes) + sizeof(struct SpaceBitmapDesc) - 1)
								>> sb->s_blocksize_bits);
						udf_release_data(bh);
						break;
					}
					case TID_FILE_SET_DESC:
					{
						*fileset = newfileset;
						break;
					}
					default:
					{
						newfileset.logicalBlockNum ++;
						udf_release_data(bh);
						bh = NULL;
						break;
					}
				}
			}
			while (newfileset.logicalBlockNum < lastblock &&
				fileset->logicalBlockNum == 0xFFFFFFFF &&
				fileset->partitionReferenceNum == 0xFFFF);
		}
	}
	if ((fileset->logicalBlockNum != 0xFFFFFFFF ||
		fileset->partitionReferenceNum != 0xFFFF) && bh)
	{
		udf_debug("Fileset at block=%d, partition=%d\n",
			fileset->logicalBlockNum, fileset->partitionReferenceNum);

		UDF_SB_PARTITION(sb) = fileset->partitionReferenceNum;
		udf_load_fileset(sb, bh, root);
		udf_release_data(bh);
		return 0;
	}
	return 1;
}

static void 
udf_load_pvoldesc(struct super_block *sb, struct buffer_head *bh)
{
	struct PrimaryVolDesc *pvoldesc;
	time_t recording;
	struct ustr instr;
	struct ustr outstr;

	pvoldesc = (struct PrimaryVolDesc *)bh->b_data;

	if ( udf_stamp_to_time(&recording, lets_to_cpu(pvoldesc->recordingDateAndTime)) )
	{
	    timestamp ts;
	    ts = lets_to_cpu(pvoldesc->recordingDateAndTime);
		udf_debug("recording time %ld, %u/%u/%u %u:%u (%x)\n",
			recording, ts.year, ts.month, ts.day, ts.hour, ts.minute,
			ts.typeAndTimezone);
	    UDF_SB_RECORDTIME(sb) = recording;
	}

	if ( !udf_build_ustr(&instr, pvoldesc->volIdent, 32) )
	{
		if (!udf_CS0toUTF8(&outstr, &instr))
		{
			udf_debug("volIdent[] = '%s'\n", outstr.u_name);
			strncpy( UDF_SB_VOLIDENT(sb), outstr.u_name, outstr.u_len);
		}
	}

	if ( !udf_build_ustr(&instr, pvoldesc->volSetIdent, 128) )
	{
		if (!udf_CS0toUTF8(&outstr, &instr))
			udf_debug("volSetIdent[] = '%s'\n", outstr.u_name);
	}
}

static void 
udf_load_fileset(struct super_block *sb, struct buffer_head *bh, lb_addr *root)
{
	struct FileSetDesc *fset;

	fset = (struct FileSetDesc *)bh->b_data;

	*root = lelb_to_cpu(fset->rootDirectoryICB.extLocation);

	udf_debug("Rootdir at block=%d, partition=%d\n", 
		root->logicalBlockNum, root->partitionReferenceNum);
}

static void 
udf_load_partdesc(struct super_block *sb, struct buffer_head *bh)
{
	struct PartitionDesc *p;
	int i;

	p=(struct PartitionDesc *)bh->b_data;

	for (i=0; i<UDF_SB_NUMPARTS(sb); i++)
	{
		udf_debug("Searching map: (%d == %d)\n", 
			UDF_SB_PARTMAPS(sb)[i].s_partition_num, le16_to_cpu(p->partitionNumber));
		if (UDF_SB_PARTMAPS(sb)[i].s_partition_num == le16_to_cpu(p->partitionNumber))
		{
			UDF_SB_PARTLEN(sb,i) = le32_to_cpu(p->partitionLength); /* blocks */
			UDF_SB_PARTROOT(sb,i) = le32_to_cpu(p->partitionStartingLocation) + UDF_SB_SESSION(sb);
			UDF_SB_PARTMAPS(sb)[i].s_uspace_bitmap = 0xFFFFFFFF;

			if (!strcmp(p->partitionContents.ident, PARTITION_CONTENTS_NSR02) ||
				!strcmp(p->partitionContents.ident, PARTITION_CONTENTS_NSR03))
			{
				struct PartitionHeaderDesc *phd;

				phd = (struct PartitionHeaderDesc *)(p->partitionContentsUse);
				if (phd->unallocatedSpaceTable.extLength)
					udf_debug("unallocatedSpaceTable (part %d)\n", i);
				if (phd->unallocatedSpaceBitmap.extLength)
				{
					UDF_SB_PARTMAPS(sb)[i].s_uspace_bitmap =
						le32_to_cpu(phd->unallocatedSpaceBitmap.extPosition);
					udf_debug("unallocatedSpaceBitmap (part %d) @ %d\n",
						i, UDF_SB_PARTMAPS(sb)[i].s_uspace_bitmap);
				}
				if (phd->partitionIntegrityTable.extLength)
					udf_debug("partitionIntegrityTable (part %d)\n", i);
				if (phd->freedSpaceTable.extLength)
					udf_debug("freedSpaceTable (part %d)\n", i);
				if (phd->freedSpaceBitmap.extLength)
					udf_debug("freedSpaceBitmap (part %d\n", i);
			}
			break;
		}
	}
	if (i == UDF_SB_NUMPARTS(sb))
	{
		udf_debug("Partition (%d) not found in partition map\n", le16_to_cpu(p->partitionNumber));
	}
	else
	{
		udf_debug("Partition (%d:%d type %x) starts at physical %d, block length %d\n",
			le16_to_cpu(p->partitionNumber), i, UDF_SB_PARTTYPE(sb,i),
			UDF_SB_PARTROOT(sb,i), UDF_SB_PARTLEN(sb,i));
	}
}

static int 
udf_load_logicalvol(struct super_block *sb, struct buffer_head * bh, lb_addr *fileset)
{
	struct LogicalVolDesc *lvd;
	int i, offset;
	Uint8 type;

	lvd = (struct LogicalVolDesc *)bh->b_data;

	UDF_SB_NUMPARTS(sb) = le32_to_cpu(lvd->numPartitionMaps);
	UDF_SB_ALLOC_PARTMAPS(sb, UDF_SB_NUMPARTS(sb));

	for (i=0,offset=0;
		 i<UDF_SB_NUMPARTS(sb) && offset<le32_to_cpu(lvd->mapTableLength);
		 i++,offset+=((struct GenericPartitionMap *)&(lvd->partitionMaps[offset]))->partitionMapLength)
	{
		type = ((struct GenericPartitionMap *)&(lvd->partitionMaps[offset]))->partitionMapType;
		udf_debug("Partition (%d) type %d\n", i, type);
		if (type == 1)
		{
			struct GenericPartitionMap1 *gpm1 = (struct GenericPartitionMap1 *)&(lvd->partitionMaps[offset]);
			UDF_SB_PARTTYPE(sb,i) = UDF_TYPE1_MAP15;
			UDF_SB_PARTVSN(sb,i) = le16_to_cpu(gpm1->volSeqNum);
			UDF_SB_PARTNUM(sb,i) = le16_to_cpu(gpm1->partitionNum);
		}
		else if (type == 2)
		{
			struct UdfPartitionMap2 *upm2 = (struct UdfPartitionMap2 *)&(lvd->partitionMaps[offset]);
			if (!strncmp(upm2->partIdent.ident, UDF_ID_VIRTUAL, strlen(UDF_ID_VIRTUAL)))
			{
				if (le16_to_cpu(((Uint16 *)upm2->partIdent.identSuffix)[0]) == 0x0150)
					UDF_SB_PARTTYPE(sb,i) = UDF_VIRTUAL_MAP15;
				else if (le16_to_cpu(((Uint16 *)upm2->partIdent.identSuffix)[0]) == 0x0200)
					UDF_SB_PARTTYPE(sb,i) = UDF_VIRTUAL_MAP20;
			}
			else if (!strncmp(upm2->partIdent.ident, UDF_ID_SPARABLE, strlen(UDF_ID_SPARABLE)))
			{
				struct SparablePartitionMap *spm = (struct SparablePartitionMap *)&(lvd->partitionMaps[offset]);
				UDF_SB_PARTTYPE(sb,i) = UDF_SPARABLE_MAP15;
				UDF_SB_TYPESPAR(sb,i).s_spar_plen = le16_to_cpu(spm->packetLength);
				UDF_SB_TYPESPAR(sb,i).s_spar_loc = le32_to_cpu(spm->locSparingTable[0]);
			}
			else
			{
				udf_debug("Unknown ident: %s\n", upm2->partIdent.ident);
				continue;
			}
			UDF_SB_PARTVSN(sb,i) = le16_to_cpu(upm2->volSeqNum);
			UDF_SB_PARTNUM(sb,i) = le16_to_cpu(upm2->partitionNum);
		}
	}

	if (fileset)
	{
		long_ad *la = (long_ad *)&(lvd->logicalVolContentsUse[0]);

		*fileset = lelb_to_cpu(la->extLocation);
		udf_debug("FileSet found in LogicalVolDesc at block=%d, partition=%d\n",
			fileset->logicalBlockNum,
			fileset->partitionReferenceNum);
	}
	if (lvd->integritySeqExt.extLength)
		udf_load_logicalvolint(sb, leea_to_cpu(lvd->integritySeqExt));
	return 0;
}

/*
 * udf_load_logicalvolint
 *
 */
static void
udf_load_logicalvolint(struct super_block *sb, extent_ad loc)
{
	struct buffer_head *bh;
	Uint16 ident;

	while ((bh = udf_read_tagged(sb, loc.extLocation,
			loc.extLocation, &ident)) &&
		ident == TID_LOGICAL_VOL_INTEGRITY_DESC && loc.extLength > 0)
	{
		UDF_SB_LVIDBH(sb) = bh;
		
		if (UDF_SB_LVID(sb)->nextIntegrityExt.extLength)
			udf_load_logicalvolint(sb, leea_to_cpu(UDF_SB_LVID(sb)->nextIntegrityExt));
		
		if (UDF_SB_LVIDBH(sb) != bh)
			udf_release_data(bh);
		loc.extLength -= sb->s_blocksize;
		loc.extLocation ++;
	}
	if (UDF_SB_LVIDBH(sb) != bh)
		udf_release_data(bh);
}

/*
 * udf_process_sequence
 *
 * PURPOSE
 *	Process a main/reserve volume descriptor sequence.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to _locked_ superblock.
 *	block			First block of first extent of the sequence.
 *	lastblock		Lastblock of first extent of the sequence.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static  int
udf_process_sequence(struct super_block *sb, long block, long lastblock, lb_addr *fileset)
{
	struct buffer_head *bh;
	struct udf_vds_record	vds[VDS_POS_LENGTH];
	struct GenericDesc *gd;
	int done=0;
	int i,j;
	Uint32 vdsn;
	Uint16 ident;

	memset(vds, 0, sizeof(struct udf_vds_record) * VDS_POS_LENGTH);

	/* Read the main descriptor sequence */
	for (;(!done && block <= lastblock); block++)
	{

		bh = udf_read_tagged(sb, block, block, &ident);
		if (!bh) 
			break;

		/* Process each descriptor (ISO 13346 3/8.3-8.4) */
		gd = (struct GenericDesc *)bh->b_data;
		vdsn = le32_to_cpu(gd->volDescSeqNum);
		switch (ident)
		{
			case TID_PRIMARY_VOL_DESC: /* ISO 13346 3/10.1 */
				if (vdsn >= vds[VDS_POS_PRIMARY_VOL_DESC].volDescSeqNum)
				{
					vds[VDS_POS_PRIMARY_VOL_DESC].volDescSeqNum = vdsn;
					vds[VDS_POS_PRIMARY_VOL_DESC].block = block;
				}
				break;
			case TID_VOL_DESC_PTR: /* ISO 13346 3/10.3 */
				if (vdsn >= vds[VDS_POS_VOL_DESC_PTR].volDescSeqNum)
				{
					vds[VDS_POS_VOL_DESC_PTR].volDescSeqNum = vdsn;
					vds[VDS_POS_VOL_DESC_PTR].block = block;
				}
				break;
			case TID_IMP_USE_VOL_DESC: /* ISO 13346 3/10.4 */
				if (vdsn >= vds[VDS_POS_IMP_USE_VOL_DESC].volDescSeqNum)
				{
					vds[VDS_POS_IMP_USE_VOL_DESC].volDescSeqNum = vdsn;
					vds[VDS_POS_IMP_USE_VOL_DESC].block = block;
				}
				break;
			case TID_PARTITION_DESC: /* ISO 13346 3/10.5 */
				if (!vds[VDS_POS_PARTITION_DESC].block)
					vds[VDS_POS_PARTITION_DESC].block = block;
				break;
			case TID_LOGICAL_VOL_DESC: /* ISO 13346 3/10.6 */
				if (vdsn >= vds[VDS_POS_LOGICAL_VOL_DESC].volDescSeqNum)
				{
					vds[VDS_POS_LOGICAL_VOL_DESC].volDescSeqNum = vdsn;
					vds[VDS_POS_LOGICAL_VOL_DESC].block = block;
				}
				break;
			case TID_UNALLOC_SPACE_DESC: /* ISO 13346 3/10.8 */
				if (vdsn >= vds[VDS_POS_UNALLOC_SPACE_DESC].volDescSeqNum)
				{
					vds[VDS_POS_UNALLOC_SPACE_DESC].volDescSeqNum = vdsn;
					vds[VDS_POS_UNALLOC_SPACE_DESC].block = block;
				}
				break;
			case TID_TERMINATING_DESC: /* ISO 13346 3/10.9 */
				vds[VDS_POS_TERMINATING_DESC].block = block;
				done = 1;
				break;
		}
		udf_release_data(bh);
	}
	for (i=0; i<VDS_POS_LENGTH; i++)
	{
		if (vds[i].block)
		{
			bh = udf_read_tagged(sb, vds[i].block, vds[i].block, &ident);

			if (i == VDS_POS_PRIMARY_VOL_DESC)
				udf_load_pvoldesc(sb, bh);
			else if (i == VDS_POS_LOGICAL_VOL_DESC)
				udf_load_logicalvol(sb, bh, fileset);
			else if (i == VDS_POS_PARTITION_DESC)
			{
				struct buffer_head *bh2;
				udf_load_partdesc(sb, bh);
				for (j=vds[i].block+1; j<vds[VDS_POS_TERMINATING_DESC].block; j++)
				{
					bh2 = udf_read_tagged(sb, j, j, &ident);
					gd = (struct GenericDesc *)bh2->b_data;
					if (ident == TID_PARTITION_DESC)
						udf_load_partdesc(sb, bh2);
					udf_release_data(bh2);
				}
			}
			udf_release_data(bh);
		}
	}

	return 0;
}

/*
 * udf_check_valid()
 */
static int
udf_check_valid(struct super_block *sb, int silent)
{
	long block;

	/* Check that it is NSR02 compliant */
	/* Process any "CD-ROM Volume Descriptor Set" (ECMA 167 2/8.3.1) */
	block = udf_vrs(sb, silent);
	if (!block) 	/* block = begining of extended area block */
		return 1;
	return 0;
}

static int
udf_load_partition(struct super_block *sb, struct AnchorVolDescPtr *anchor, lb_addr *fileset)
{
	long main_s, main_e, reserve_s, reserve_e;
	int i;

	if (!sb)
	    return 1;
	/* Locate the main sequence */
	main_s = le32_to_cpu( anchor->mainVolDescSeqExt.extLocation );
	main_e = le32_to_cpu( anchor->mainVolDescSeqExt.extLength );
	main_e = main_e >> sb->s_blocksize_bits;
	main_e += main_s;


	/* Locate the reserve sequence */
	reserve_s = le32_to_cpu(anchor->mainVolDescSeqExt.extLocation);
	reserve_e = le32_to_cpu(anchor->mainVolDescSeqExt.extLength);
	reserve_e = reserve_e >> sb->s_blocksize_bits;
	reserve_e += reserve_s;

#if 0
	UDF_SB_LASTBLOCK(sb)= (main_e > reserve_e) ? main_e : reserve_e;
#endif

	/* Process the main & reserve sequences */
	/* responsible for finding the PartitionDesc(s) */
	if ( udf_process_sequence(sb, main_s, main_e, fileset) &&
			udf_process_sequence(sb, reserve_s, reserve_e, fileset) )
		return 1;

	for (i=0; i<UDF_SB_NUMPARTS(sb); i++)
	{
		switch UDF_SB_PARTTYPE(sb, i)
		{
			case UDF_VIRTUAL_MAP15:
			case UDF_VIRTUAL_MAP20:
			{
				lb_addr ino;

				if (i == 0)
					ino.partitionReferenceNum = i+1;
				else
					ino.partitionReferenceNum = i-1;

				ino.logicalBlockNum = UDF_SB_LASTBLOCK(sb) - UDF_SB_PARTROOT(sb,ino.partitionReferenceNum);

				UDF_SB_VAT(sb) = udf_iget(sb, ino);

				if (UDF_SB_PARTTYPE(sb,i) == UDF_VIRTUAL_MAP15)
				{
					UDF_SB_TYPEVIRT(sb,i).s_start_offset = 0;
					UDF_SB_TYPEVIRT(sb,i).s_num_entries = (UDF_SB_VAT(sb)->i_size - 36) / sizeof(Uint32);
				}
				else if (UDF_SB_PARTTYPE(sb,i) == UDF_VIRTUAL_MAP20)
				{
					struct buffer_head *bh;
					Uint32 pos;

					pos = udf_bmap(UDF_SB_VAT(sb), 0);
					bh = udf_bread(sb, pos, sb->s_blocksize);
					UDF_SB_TYPEVIRT(sb,i).s_start_offset = le16_to_cpu(((struct VirtualAllocationTable20 *)bh->b_data)->lengthHeader);
					UDF_SB_TYPEVIRT(sb,i).s_num_entries = (UDF_SB_VAT(sb)->i_size -
						UDF_SB_TYPEVIRT(sb,i).s_start_offset) / sizeof(Uint32);
					udf_release_data(bh);
				}
				UDF_SB_PARTROOT(sb,i) = udf_get_pblock(sb, 0, i, 0);
				UDF_SB_PARTLEN(sb,i) = UDF_SB_PARTLEN(sb,ino.partitionReferenceNum);
				UDF_SB_PARTMAPS(sb)[i].s_uspace_bitmap = 0xFFFFFFFF;
			}
		}
	}
	return 0;
}

static void udf_open_lvid(struct super_block *sb)
{
#ifdef CONFIG_UDF_RW
	if (UDF_SB_LVIDBH(sb))
	{
		int i;
		timestamp cpu_time;

		UDF_SB_LVIDIU(sb)->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
		UDF_SB_LVIDIU(sb)->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
		if (udf_time_to_stamp(&cpu_time, CURRENT_TIME))
			UDF_SB_LVID(sb)->recordingDateAndTime = cpu_to_lets(cpu_time);
		UDF_SB_LVID(sb)->integrityType = INTEGRITY_TYPE_OPEN;

		UDF_SB_LVID(sb)->descTag.descCRC =
			cpu_to_le16(udf_crc((char *)UDF_SB_LVID(sb) + sizeof(tag),
			le16_to_cpu(UDF_SB_LVID(sb)->descTag.descCRCLength), 0));

		UDF_SB_LVID(sb)->descTag.tagChecksum = 0;
		for (i=0; i<16; i++)
			if (i != 4)
				UDF_SB_LVID(sb)->descTag.tagChecksum +=
					((Uint8 *)&(UDF_SB_LVID(sb)->descTag))[i];

		mark_buffer_dirty(UDF_SB_LVIDBH(sb), 1);
	}
#endif
}

static void udf_close_lvid(struct super_block *sb)
{
#ifdef CONFIG_UDF_RW
	if (UDF_SB_LVIDBH(sb) &&
		UDF_SB_LVID(sb)->integrityType == INTEGRITY_TYPE_OPEN)
	{
		int i;
		timestamp cpu_time;

		UDF_SB_LVIDIU(sb)->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
		UDF_SB_LVIDIU(sb)->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
		if (udf_time_to_stamp(&cpu_time, CURRENT_TIME))
			UDF_SB_LVID(sb)->recordingDateAndTime = cpu_to_lets(cpu_time);
		UDF_SB_LVID(sb)->integrityType = INTEGRITY_TYPE_CLOSE;

		UDF_SB_LVID(sb)->descTag.descCRC =
			cpu_to_le16(udf_crc((char *)UDF_SB_LVID(sb) + sizeof(tag),
			le16_to_cpu(UDF_SB_LVID(sb)->descTag.descCRCLength), 0));

		UDF_SB_LVID(sb)->descTag.tagChecksum = 0;
		for (i=0; i<16; i++)
			if (i != 4)
				UDF_SB_LVID(sb)->descTag.tagChecksum +=
					((Uint8 *)&(UDF_SB_LVID(sb)->descTag))[i];

		mark_buffer_dirty(UDF_SB_LVIDBH(sb), 1);
	}
#endif
}

/*
 * udf_read_super
 *
 * PURPOSE
 *	Complete the specified super block.
 *
 * PRE-CONDITIONS
 *	sb			Pointer to superblock to complete - never NULL.
 *	sb->s_dev		Device to read suberblock from.
 *	options			Pointer to mount options.
 *	silent			Silent flag.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static struct super_block *
udf_read_super(struct super_block *sb, void *options, int silent)
{
	struct inode *inode=NULL;
	struct AnchorVolDescPtr anchor;
	struct udf_options uopt;
	lb_addr rootdir, fileset;
	int i;

	uopt.flags = 0;
	uopt.uid = 0;
	uopt.gid = 0;
	uopt.umask = 0;
	uopt.utf8 = 0;

	/* Lock the module in memory (if applicable) */
	MOD_INC_USE_COUNT;

	lock_super(sb);

	UDF_SB_ALLOC(sb); /* kmalloc, if needed */
	UDF_SB_PARTMAPS(sb) = NULL;
	UDF_SB_LVIDBH(sb) = NULL;
	UDF_SB_VAT(sb) = NULL;

	if (!udf_parse_options((char *)options, &uopt))
		goto error_out;

	UDF_SB_ANCHOR(sb)=0;
	fileset.logicalBlockNum = 0xFFFFFFFF;
	fileset.partitionReferenceNum = 0xFFFF;
	UDF_SB_RECORDTIME(sb)=0;
	UDF_SB_VOLIDENT(sb)[0]=0;

	UDF_SB(sb)->s_flags = uopt.flags;
	UDF_SB(sb)->s_uid = uopt.uid;
	UDF_SB(sb)->s_gid = uopt.gid;
	UDF_SB(sb)->s_umask = uopt.umask;
	UDF_SB(sb)->s_utf8 = uopt.utf8;

	/* Set the block size for all transfers */
	if (!udf_set_blocksize(sb, uopt.blocksize))
		goto error_out;

	if ( uopt.session == 0xFFFFFFFF )
		UDF_SB_SESSION(sb) = udf_get_last_session(sb->s_dev);
	else
		UDF_SB_SESSION(sb) = uopt.session;

	udf_debug("Multi-session=%d\n", UDF_SB_SESSION(sb));

	if ( uopt.lastblock == 0xFFFFFFFF )
		UDF_SB_LASTBLOCK(sb) = udf_get_last_block(sb->s_dev, &(UDF_SB(sb)->s_flags));
	else
		UDF_SB_LASTBLOCK(sb) = uopt.lastblock;

	udf_debug("Lastblock=%d\n", UDF_SB_LASTBLOCK(sb));

	if (!uopt.novrs)
	{
		if (udf_check_valid(sb, silent)) /* read volume recognition sequences */
		{
			udf_debug("No VRS found\n");
 			goto error_out;
		}
	}
	else
	{
		udf_debug("Validity check skipped because of novrs option\n");
	}

	if (uopt.anchor == 0xFFFFFFFF)
	{
		/* Find an anchor volume descriptor pointer */
		if (udf_find_anchor(sb, UDF_SB_LASTBLOCK(sb), &anchor))
		{
			udf_debug("No Anchor block found\n");
			goto error_out;
		}
	}
	else
		UDF_SB_ANCHOR(sb) = uopt.anchor;

	UDF_SB_CHARSET(sb) = NULL;

	if (uopt.utf8 == 0)
	{
		char *p = uopt.iocharset ? uopt.iocharset : "iso8859-1";
		UDF_SB_CHARSET(sb) = load_nls(p);
		if (!UDF_SB_CHARSET(sb))
			if (uopt.iocharset)
				goto error_out;
		UDF_SB_CHARSET(sb) = load_nls_default();
	}

	/* Fill in the rest of the superblock */
	sb->s_op = &udf_sb_ops;
	sb->s_time = 0;
	sb->dq_op = NULL;
	sb->s_dirt = 0;
	sb->s_magic = UDF_SUPER_MAGIC;
	sb->s_flags |= MS_NODEV | MS_NOSUID; /* should be overridden by mount */

	for (i=0; i<UDF_MAX_BLOCK_LOADED; i++)
	{
		UDF_SB_BLOCK_BITMAP_NUMBER(sb,i) = 0;
		UDF_SB_BLOCK_BITMAP(sb,i) = NULL;
	}
	UDF_SB_LOADED_BLOCK_BITMAPS(sb) = 0;

	if (udf_load_partition(sb, &anchor, &fileset))
	{
		udf_debug("No partition found (1)\n");
		goto error_out;
	}

	if ( !UDF_SB_NUMPARTS(sb) )
	{
		udf_debug("No partition found (2)\n");
		goto error_out;
	}

	if ( udf_find_fileset(sb, &fileset, &rootdir) )
	{
		udf_debug("No fileset found\n");
		goto error_out;
	}

	if (!silent)
	{
		timestamp ts;
		udf_time_to_stamp(&ts, UDF_SB_RECORDTIME(sb));
		udf_info("Mounting volume '%s', timestamp %u/%02u/%u %02u:%02u\n",
			UDF_SB_VOLIDENT(sb), ts.year, ts.month, ts.day, ts.hour, ts.minute);
	}
	if (!(sb->s_flags & MS_RDONLY))
		udf_open_lvid(sb);
	unlock_super(sb);

	/* Assign the root inode */
	/* assign inodes by physical block number */
	/* perhaps it's not extensible enough, but for now ... */
	inode = udf_iget(sb, rootdir); 
	if (!inode)
	{
		udf_debug("Error in udf_iget, block=%d, partition=%d\n",
			rootdir.logicalBlockNum, rootdir.partitionReferenceNum);
		goto error_out;
	}

#if LINUX_VERSION_CODE > 0x020170
	/* Allocate a dentry for the root inode */
	sb->s_root = d_alloc_root(inode, NULL);
	if (!sb->s_root)
	{
		iput(inode);
		udf_debug("Couldn't allocate root dentry\n");
		goto error_out;
	}
#endif

	return sb;

error_out:
	sb->s_dev = NODEV;
	if (UDF_SB_VAT(sb))
		iput(UDF_SB_VAT(sb));
	udf_close_lvid(sb);
	udf_release_data(UDF_SB_LVIDBH(sb));
	UDF_SB_FREE(sb);
	unlock_super(sb);
	MOD_DEC_USE_COUNT;
	return NULL;
}

/*
 * udf_put_super
 *
 * PURPOSE
 *	Prepare for destruction of the superblock.
 *
 * DESCRIPTION
 *	Called before the filesystem is unmounted.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
static void
udf_put_super(struct super_block *sb)
{
	int i;

	if (UDF_SB_VAT(sb))
		iput(UDF_SB_VAT(sb));
	udf_close_lvid(sb);
	udf_release_data(UDF_SB_LVIDBH(sb));
	for (i=0; i<UDF_MAX_BLOCK_LOADED; i++)
		udf_release_data(UDF_SB_BLOCK_BITMAP(sb, i));
	UDF_SB_FREE(sb);

	MOD_DEC_USE_COUNT;
}

/*
 * udf_stat_fs
 *
 * PURPOSE
 *	Return info about the filesystem.
 *
 * DESCRIPTION
 *	Called by sys_statfs()
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */
#if LINUX_VERSION_CODE > 0x020100
static int
#else
static void
#endif
udf_statfs(struct super_block *sb, struct statfs *buf, int bufsize)
{
	int size;
	struct statfs tmp;
	int rc;

	size = (bufsize < sizeof(tmp)) ? bufsize: sizeof(tmp);

	memset(&tmp, 0, sizeof(tmp));
	tmp.f_type = UDF_SUPER_MAGIC;
	tmp.f_bsize = sb->s_blocksize;
	tmp.f_blocks = UDF_SB_PARTLEN(sb, UDF_SB_PARTITION(sb));
	tmp.f_bfree = udf_count_free(sb);
	tmp.f_bavail = tmp.f_bfree;
	tmp.f_files = UDF_SB_LVIDBH(sb) ? le32_to_cpu(UDF_SB_LVIDIU(sb)->numFiles) : 0;
	tmp.f_ffree = 0L;
	/* __kernel_fsid_t f_fsid */
	tmp.f_namelen = UDF_NAME_LEN;

	rc= copy_to_user(buf, &tmp, size) ? -EFAULT: 0;
#if LINUX_VERSION_CODE > 0x020100
	return rc;
#endif
}

static unsigned char udf_bitmap_lookup[16] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};
	
static unsigned int
udf_count_free(struct super_block *sb)
{
	struct buffer_head *bh;
	unsigned int accum=0;
	int index;
	int block=0, newblock;
	lb_addr loc;
	Uint32  bytes;
	Uint8   value;
	Uint8 * ptr;
	Uint16 ident;

	if (UDF_SB_PARTMAPS(sb)[UDF_SB_PARTITION(sb)].s_uspace_bitmap == 0xFFFFFFFF)
	{
		if (UDF_SB_LVIDBH(sb))
		{
			if (le32_to_cpu(UDF_SB_LVID(sb)->numOfPartitions) > UDF_SB_PARTITION(sb))
				accum = le32_to_cpu(UDF_SB_LVID(sb)->freeSpaceTable[UDF_SB_PARTITION(sb)]);

			if (accum == 0xFFFFFFFF)
				accum = 0;

			return accum;
		}
		else
			return 0;
	}
	else
	{
		struct SpaceBitmapDesc *bm;
		loc.logicalBlockNum = UDF_SB_PARTMAPS(sb)[UDF_SB_PARTITION(sb)].s_uspace_bitmap;
		loc.partitionReferenceNum = UDF_SB_PARTITION(sb);
		bh = udf_read_ptagged(sb, loc, 0, &ident);

		if (!bh)
		{
			printk(KERN_ERR "udf: udf_count_free failed\n");
			return 0;
		}
		else if (ident != TID_SPACE_BITMAP_DESC)
		{
			udf_release_data(bh);
			printk(KERN_ERR "udf: udf_count_free failed\n");
			return 0;
		}

		bm = (struct SpaceBitmapDesc *)bh->b_data;
		bytes = bm->numOfBytes;
		index = sizeof(struct SpaceBitmapDesc); /* offset in first block only */
		ptr = (Uint8 *)bh->b_data;

		while ( bytes > 0 )
		{
			while ((bytes > 0) && (index < sb->s_blocksize))
			{
				value = ptr[index];
				accum += udf_bitmap_lookup[ value & 0x0f ];
				accum += udf_bitmap_lookup[ value >> 4 ];
				index++;
				bytes--;
			}
			if ( bytes )
			{
				udf_release_data(bh);
				newblock = udf_get_lb_pblock(sb, loc, ++block);
				bh = udf_bread(sb, newblock, sb->s_blocksize);
				if (!bh)
				{
					udf_debug("read failed\n");
					return accum;
				}
				index = 0;
				ptr = (Uint8 *)bh->b_data;
			}
		}
		udf_release_data(bh);
		return accum;
	}
}
