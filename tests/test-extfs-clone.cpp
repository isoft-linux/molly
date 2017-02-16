/*
 * Copyright (C) 2017 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#include <errno.h>
#include <features.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <mcheck.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <QDebug>

#ifdef __cplusplus
extern "C" {
#endif
#include "progress.h"
#include "partclone.h"
#include "checksum.h"
#include "fs_common.h"
#ifdef __cplusplus
}
#endif

cmd_opt opt;
fs_cmd_opt fs_opt;

int main(int argc, char *argv[]) 
{
    unsigned long long copied;
    unsigned long long block_id;
    int done;
	char *source;
	char *target;
	int dfr, dfw;
	int r_size, w_size;
	unsigned cs_size = 0;
	int cs_reseed = 1;
	int start, stop;
	unsigned long *bitmap = NULL;
	int debug = 0;
	int tui = 0;
	int pui = 0;

	static const char *const bad_sectors_warning_msg =
		"*************************************************************************\n"
		"* WARNING: The disk has bad sectors. This means physical damage on the  *\n"
		"* disk surface caused by deterioration, manufacturing faults, or        *\n"
		"* another reason. The reliability of the disk may remain stable or      *\n"
		"* degrade quickly. Use the --rescue option to efficiently save as much  *\n"
		"* data as possible!                                                     *\n"
		"*************************************************************************\n";

    const char *argV[] = { "partclone.ext4", 
                           "-L", 
                           "/share/backup/test.log", 
                           "-d", 
                           "-c", 
                           "-s", 
                           argv[1] ? argv[1] : "/dev/sda1", 
                           "-o", 
                           "/share/backup/test.img" };
    int argC = sizeof(argV) / sizeof(char *);
    qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << argC;

	file_system_info fs_info;
	image_options    img_opt;

	init_fs_info(&fs_info);
	init_image_options(&img_opt);

	parse_options(argC, (char **)argV, &opt);

	memset(&fs_opt, 0, sizeof(fs_cmd_opt));
	debug = opt.debug;
	fs_opt.debug = debug;
	fs_opt.ignore_fschk = opt.ignore_fschk;

	open_log(opt.logfile);

    pui = TEXT;
	tui = open_pui(pui, opt.fresh);
	if ((opt.ncurses) && (!tui)) {
		opt.ncurses = 0;
		pui = TEXT;
		log_mesg(1, 0, 0, debug, "Open Ncurses User Interface Error.\n");
	}

	print_partclone_info(opt);

	if (opt.ignore_crc)
		log_mesg(1, 0, 1, debug, "Ignore CRC errors\n");

	source = opt.source;
	target = opt.target;
	log_mesg(1, 0, 0, debug, "source=%s, target=%s \n", source, target);
	dfr = open_source(source, &opt);
	if (dfr == -1) {
		log_mesg(0, 1, 1, debug, "Error exit\n");
        goto cleanup;
	}

	dfw = open_target(target, &opt);
	if (dfw == -1) {
		log_mesg(0, 1, 1, debug, "Error exit\n");
        goto cleanup;
	}

	if (opt.clone) {
		log_mesg(1, 0, 0, debug, "Initiate image options - version %s\n", IMAGE_VERSION_CURRENT);

		img_opt.checksum_mode = opt.checksum_mode;
		img_opt.checksum_size = get_checksum_size(opt.checksum_mode, opt.debug);
		img_opt.blocks_per_checksum = opt.blocks_per_checksum;
		img_opt.reseed_checksum = opt.reseed_checksum;

		cs_size = img_opt.checksum_size;
		cs_reseed = img_opt.reseed_checksum;

		log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
		log_mesg(0, 0, 1, debug, "Reading Super Block\n");

		read_super_blocks(source, &fs_info);
        if (fs_info.block_size == 0 || fs_info.block_size == -1) {
            qWarning() << "ERROR:" << __PRETTY_FUNCTION__ << "Invalid block size";
            goto cleanup;
        }

		if (img_opt.checksum_mode != CSM_NONE && img_opt.blocks_per_checksum == 0) {
			const unsigned int buffer_capacity = opt.buffer_size > fs_info.block_size
				? opt.buffer_size / fs_info.block_size : 1;

			img_opt.blocks_per_checksum = buffer_capacity;
		}
		log_mesg(1, 0, 0, debug, "%u blocks per checksum\n", img_opt.blocks_per_checksum);

		check_mem_size(fs_info, img_opt, opt);

		bitmap = pc_alloc_bitmap(fs_info.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
            goto cleanup;
        }

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... \n");
		read_bitmap(source, fs_info, bitmap, pui);
		update_used_blocks_count(&fs_info, bitmap);

		if (opt.check) {
			unsigned long long needed_space = 0;

			needed_space += sizeof(image_head) + sizeof(file_system_info) + sizeof(image_options);
			needed_space += get_bitmap_size_on_disk(&fs_info, &img_opt, &opt);
			needed_space += cnv_blocks_to_bytes(0, fs_info.usedblocks, fs_info.block_size, &img_opt);

			check_free_space(&dfw, needed_space);
		}

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Writing super block and bitmap...\n");

		write_image_desc(&dfw, fs_info, img_opt, &opt);
		write_image_bitmap(&dfw, fs_info, img_opt, bitmap, &opt);

		log_mesg(0, 0, 1, debug, "done!\n");
	}

	log_mesg(1, 0, 0, debug, "print image information\n");

	if (debug)
		print_opt(opt);

	print_file_system_info(fs_info, opt);

	copied = 0;

	if (opt.clone) {
		const unsigned long long blocks_total = fs_info.totalblock;
		const unsigned int block_size = fs_info.block_size;
		const unsigned int buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1;
		unsigned char checksum[cs_size];
		unsigned int blocks_in_cs, blocks_per_cs, write_size;
		char *read_buffer, *write_buffer;

		blocks_per_cs = img_opt.blocks_per_checksum;

		log_mesg(1, 0, 0, debug, "#\nBuffer capacity = %u, Blocks per cs = %u\n#\n", buffer_capacity, blocks_per_cs);

		write_size = cnv_blocks_to_bytes(0, buffer_capacity, block_size, &img_opt);

		read_buffer = (char*)malloc(buffer_capacity * block_size);
		write_buffer = (char*)malloc(write_size + cs_size);

		if (read_buffer == NULL || write_buffer == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
            goto cleanup;
		}

		if (lseek(dfr, 0, SEEK_SET) == (off_t)-1) {
			log_mesg(0, 1, 1, debug, "source seek ERROR:%s\n", strerror(errno));
            goto cleanup;
        }

		log_mesg(0, 0, 0, debug, "Total block %llu\n", blocks_total);

		log_mesg(1, 0, 0, debug, "start backup data...\n");

		blocks_in_cs = 0;
		init_checksum(img_opt.checksum_mode, checksum, debug);

		block_id = 0;
		do {
			unsigned long long i, blocks_skip, blocks_read;
			unsigned int cs_added = 0, write_offset = 0;
			off_t offset;

			for (blocks_skip = 0;
			     block_id + blocks_skip < blocks_total &&
			     !pc_test_bit(block_id + blocks_skip, bitmap, fs_info.totalblock);
			     blocks_skip++);
			if (block_id + blocks_skip == blocks_total)
				break;

			if (blocks_skip)
				block_id += blocks_skip;

			for (blocks_read = 0;
			     block_id + blocks_read < blocks_total && blocks_read < buffer_capacity &&
			     pc_test_bit(block_id + blocks_read, bitmap, fs_info.totalblock);
			     ++blocks_read);
			if (!blocks_read)
				break;

			offset = (off_t)(block_id * block_size);
			if (lseek(dfr, offset, SEEK_SET) == (off_t)-1) {
				log_mesg(0, 1, 1, debug, "source seek ERROR:%s\n", strerror(errno));
                goto cleanup;
            }

			r_size = read_all(&dfr, read_buffer, blocks_read * block_size, &opt);
			if (r_size != (int)(blocks_read * block_size)) {
				if ((r_size == -1) && (errno == EIO)) {
					if (opt.rescue) {
						memset(read_buffer, 0, blocks_read * block_size);
						for (r_size = 0; r_size < blocks_read * block_size; r_size += PART_SECTOR_SIZE)
							rescue_sector(&dfr, offset + r_size, read_buffer + r_size, &opt);
					} else {
						log_mesg(0, 1, 1, debug, "%s", bad_sectors_warning_msg);
                        goto cleanup;
                    }
				} else {
					log_mesg(0, 1, 1, debug, "read error: %s\n", strerror(errno));
                    goto cleanup;
                }
			}

			log_mesg(2, 0, 0, debug, "blocks_read = %i\n", blocks_read);
			for (i = 0; i < blocks_read; ++i) {
				memcpy(write_buffer + write_offset,
					read_buffer + i * block_size, block_size);

				write_offset += block_size;

				update_checksum(checksum, read_buffer + i * block_size, block_size);

				if (blocks_per_cs > 0 && ++blocks_in_cs == blocks_per_cs) {
				    log_mesg(3, 0, 0, debug, "CRC = %x%x%x%x \n", checksum[0], checksum[1], checksum[2], checksum[3]);

					memcpy(write_buffer + write_offset, checksum, cs_size);

					++cs_added;
					write_offset += cs_size;

					blocks_in_cs = 0;
					if (cs_reseed)
						init_checksum(img_opt.checksum_mode, checksum, debug);
				}
			}

			w_size = write_all(&dfw, write_buffer, write_offset, &opt);
			if (w_size != write_offset) {
				log_mesg(0, 1, 1, debug, "image write ERROR:%s\n", strerror(errno));
                goto cleanup;
            }

			copied += blocks_read;
			log_mesg(2, 0, 0, debug, "copied = %lld\n", copied);

			block_id += blocks_read;

			if (r_size + cs_added * cs_size != w_size) {
				log_mesg(0, 1, 1, debug, "read(%i) and write(%i) different\n", r_size, w_size);
                goto cleanup;
            }

		} while (1);

		if (blocks_in_cs > 0) {
			log_mesg(1, 0, 0, debug, "Write the checksum for the latest blocks. size = %i\n", cs_size);
			log_mesg(3, 0, 0, debug, "CRC = %x%x%x%x \n", checksum[0], checksum[1], checksum[2], checksum[3]);
			w_size = write_all(&dfw, (char*)checksum, cs_size, &opt);
			if (w_size != cs_size) {
				log_mesg(0, 1, 1, debug, "image write ERROR:%s\n", strerror(errno));
                goto cleanup;
            }
		}

		free(write_buffer);
		free(read_buffer);
	} else if (opt.chkimg && img_opt.checksum_mode == CSM_NONE
		&& strcmp(opt.source, "-") != 0) {

		unsigned long long total_offset = (fs_info.usedblocks - 1) * fs_info.block_size;
		char last_block[fs_info.block_size];
		off_t partial_offset = INT32_MAX;

		while (total_offset) {
			if (partial_offset > total_offset)
				partial_offset = total_offset;

			if (lseek(dfr, partial_offset, SEEK_CUR) == (off_t)-1) {
				log_mesg(0, 1, 1, debug, "source seek ERROR: %s\n", strerror(errno));
                goto cleanup;
            }

			total_offset -= partial_offset;
		}

		if (read_all(&dfr, last_block, fs_info.block_size, &opt) != fs_info.block_size) {
			log_mesg(0, 1, 1, debug, "ERROR: source image too short\n");
            goto cleanup;
        }
	}

	sync_data(dfw, &opt);
	print_finish_info(opt);

cleanup:
	if (dfr = -1) {
        close(dfr);
        dfr = -1;
    }

	if (dfw != -1) {
		close(dfw);
        dfw = -1;
    }

	if (bitmap) {
        free(bitmap); 
        bitmap = NULL;
    }

	if (pui) {
        close_pui(pui);
        pui = 0;
    }

	if (opt.debug)
		close_log();

    qDebug() << "DEBUG:" << __PRETTY_FUNCTION__ << "Handle other UI components...";
}
