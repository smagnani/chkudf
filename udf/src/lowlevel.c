/*
 * lowlevel.c
 *
 * PURPOSE
 *  Low Level Device Routines for the UDF filesystem
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
 *  (C) 1999 Ben Fennema
 *
 * HISTORY
 *
 *  03/26/99 blf  Created.
 */

#include "udfdecl.h"

#include <linux/blkdev.h>
#include <linux/cdrom.h>
#include <asm/uaccess.h>
#include <scsi/scsi.h>

typedef struct scsi_device Scsi_Device;
typedef struct scsi_cmnd   Scsi_Cmnd;

#include <scsi/scsi_ioctl.h>

#include <linux/udf_fs.h>
#include "udf_sb.h"

unsigned int 
udf_get_last_session(kdev_t dev)
{
	struct cdrom_multisession ms_info;
	unsigned int vol_desc_start;
	struct inode inode_fake;
	extern struct file_operations * get_blkfops(unsigned int);
	int i;

	vol_desc_start=0;
	if (get_blkfops(MAJOR(dev))->ioctl!=NULL)
	{
		/* Whoops.  We must save the old FS, since otherwise
		 * we would destroy the kernels idea about FS on root
		 * mount in read_super... [chexum]
		 */
		mm_segment_t old_fs=get_fs();
		inode_fake.i_rdev=dev;
		ms_info.addr_format=CDROM_LBA;
		set_fs(KERNEL_DS);
		i=get_blkfops(MAJOR(dev))->ioctl(&inode_fake,
						NULL,
						CDROMMULTISESSION,
						(unsigned long) &ms_info);
		set_fs(old_fs);

#define WE_OBEY_THE_WRITTEN_STANDARDS 1

		if (i == 0)
		{
			udf_debug("XA disk: %s, vol_desc_start=%d\n",
				(ms_info.xa_flag ? "yes" : "no"), ms_info.addr.lba);
#if WE_OBEY_THE_WRITTEN_STANDARDS
			if (ms_info.xa_flag) /* necessary for a valid ms_info.addr */
#endif
				vol_desc_start = ms_info.addr.lba;
		}
		else
		{
			udf_debug("CDROMMULTISESSION not supported: rc=%d\n", i);
		}
	}
	else
	{
		udf_debug("Device doesn't know how to ioctl?\n");
	}
	return vol_desc_start;
}

static int
do_scsi(kdev_t dev, struct inode *inode_fake, Uint8 *command, int cmd_len,
	Uint8 *buffer, Uint32 in_len, Uint32 out_len)
{
	extern struct file_operations * get_blkfops(unsigned int);
	Uint32 *ip;

	ip = (Uint32 *)buffer;
	ip[0] = in_len;
	ip[1] = out_len;
	memcpy(buffer + 8, command, cmd_len);
	return get_blkfops(MAJOR(dev))->ioctl(inode_fake,
		NULL, SCSI_IOCTL_SEND_COMMAND, (unsigned long)buffer);
}

static unsigned int
udf_get_last_rti(kdev_t dev, struct inode *inode_fake)
{
	char buffer[128];
	int result = 0;
	int *ip;
	int track_no;
	Uint32 trackstart, tracklength, freeblocks;
	Uint8 cdb[10];
	unsigned long lastsector = 0;

	ip = (int *)(buffer + 8);
	memset(cdb, 0, 12);
	cdb[0] = 0x51;
	cdb[8] = 32;
	result = do_scsi(dev, inode_fake, cdb, 10, buffer, 0, 32);
	if (!result)
	{
		track_no = buffer[14];
		udf_debug("Generic Read Disc Info worked; last track is %d.\n", track_no);
		memset(buffer, 0, 128);
		cdb[0] = 0x52;
		cdb[1] = 1;
		cdb[5] = track_no;
		cdb[8] = 36;
		result = do_scsi(dev, inode_fake, cdb, 10, buffer, 0, 36);
		if (!result)
		{
			if (buffer[14] & 0x40)
			{
				cdb[5] = track_no - 1;
				result = do_scsi(dev, inode_fake, cdb, 10, buffer, 0, 36);
			}
			if (!result)
			{
				trackstart = be32_to_cpu(ip[2]);
				tracklength = be32_to_cpu(ip[6]);
				freeblocks = be32_to_cpu(ip[4]);
				udf_debug("Start %d, length %d, freeblocks %d.\n", trackstart, tracklength, freeblocks);
				if (buffer[14] & 0x10)
				{
					udf_debug("Packet size is %d.\n", be32_to_cpu(ip[5]));
					lastsector = trackstart + tracklength - 1;
				}
				else
				{
					udf_debug("Variable packet written track.\n");
					lastsector = trackstart + tracklength - 1;
					if (freeblocks)
					{
						lastsector = lastsector - freeblocks - 6;
					}
				}
			}
		}
	}
	return lastsector;
}

static int
is_mmc(kdev_t dev, struct inode *inode_fake)
{
    char buffer[142];
    int result = 0;
    Uint8 cdb[6];

	cdb[0] = MODE_SENSE;
	cdb[2] = 0x2A;
	cdb[4] = 128;
	cdb[1] = cdb[3] = cdb[5] = 0;

	result = do_scsi(dev, inode_fake, cdb, 6, buffer, 0, 128);
	return !result;
}

unsigned int

verify_lastblock(kdev_t dev, int lastblock, int *flags)
{
	struct buffer_head *bh;
	tag *tp;
	int blocklist[10];
	int i;

	blocklist[0] = blocklist[1] = lastblock - 2;
	blocklist[2] = blocklist[3] = lastblock;
	blocklist[4] = blocklist[5] = lastblock - 150;
	blocklist[6] = blocklist[7] = lastblock - 152;
	blocklist[8] = blocklist[9] = 32 * ((lastblock + 37) / 39);

	for (i=0; i<10; i+=2)
	{
		bh = bread(dev, blocklist[i], blksize_size[MAJOR(dev)][MINOR(dev)]);
		if (bh)
		{
			tp = (tag *)bh->b_data;
			if (tp->tagIdent == TID_ANCHOR_VOL_DESC_PTR)
			{
				if (tp->tagLocation == blocklist[i])
					break;
				else if (tp->tagLocation == udf_variable_to_fixed(blocklist[i]))
				{
					blocklist[i+1] = udf_variable_to_fixed(blocklist[i+1]);
					*flags |= UDF_FLAG_VARCONV;
					break;
				}
			}
			else if (tp->tagIdent == TID_FILE_ENTRY ||
				tp->tagIdent == TID_EXTENDED_FILE_ENTRY)
			{
				blocklist[i] -= 256;
				i -= 2;
			}
			brelse(bh);
			bh = NULL;
		}
	}

	if (!bh)
		return 0;
	else
	{
		brelse(bh);
		return blocklist[i+1];
	}
}

unsigned int
udf_get_last_block(kdev_t dev, int *flags)
{
	extern int *blksize_size[];
	struct inode inode_fake;
	extern struct file_operations * get_blkfops(unsigned int);
	int ret;
	unsigned long lblock;
	unsigned int hbsize = get_hardblocksize(dev);
	unsigned int mult = 0;
	unsigned int div = 0;
	int accurate = 0;

	if (!hbsize)
		hbsize = 512;

	if (hbsize > blksize_size[MAJOR(dev)][MINOR(dev)])
		mult = hbsize / blksize_size[MAJOR(dev)][MINOR(dev)];
	else if (blksize_size[MAJOR(dev)][MINOR(dev)] > hbsize)
		div = blksize_size[MAJOR(dev)][MINOR(dev)] / hbsize;

	if (get_blkfops(MAJOR(dev))->ioctl!=NULL)
	{
      /* Whoops.  We must save the old FS, since otherwise
       * we would destroy the kernels idea about FS on root
       * mount in read_super... [chexum]
       */
		mm_segment_t old_fs=get_fs();
		inode_fake.i_rdev=dev;
		set_fs(KERNEL_DS);

		lblock = 0;
		ret = get_blkfops(MAJOR(dev))->ioctl(&inode_fake,
				NULL,
				BLKGETSIZE,
				(unsigned long) &lblock);

		if (!ret) /* Hard Disk */
		{
			udf_debug("BLKGETSIZE ret=%d, lblock=%ld\n", ret, lblock);
			if (mult)
				lblock *= mult;
			else if (div)
				lblock /= div;
			lblock --;
			accurate = 1;
		}
		else /* CDROM */
		{
			if (is_mmc(dev, &inode_fake) &&
				(lblock = udf_get_last_rti(dev, &inode_fake)))
			{
				accurate = 1;
			}
			else
			{
				struct cdrom_tocentry toc;

				toc.cdte_format = CDROM_LBA;
				toc.cdte_track = 0xAA;
	
				ret = get_blkfops(MAJOR(dev))->ioctl(&inode_fake,
						NULL,
						CDROMREADTOCENTRY,
						(unsigned long) &toc);
				if (!ret)
				{
					accurate = 0;
					lblock = toc.cdte_addr.lba - 1;
				}
			}
		}
		set_fs(old_fs);
		if (!accurate)
			lblock = verify_lastblock(dev, lblock, flags);
		return lblock;
	}
	else
	{
		udf_debug("Device doesn't know how to ioctl?\n");
	}
	return 0;
}
