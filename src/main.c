#define _CRT_SECURE_NO_WARNINGS

#include <direct.h>
#include "main.h"
#include "nand.h"
#include "idstorage.h"
#include "sysreg.h"
#include "lflash.h"

unsigned char buf_data[0x200 * 0x20];  //16 KiB

const unsigned long long hex_to_ull(const char* hex)
{
	unsigned long long t = 0, res = 0;
	unsigned int len = (unsigned int)strlen((const char*)hex);
	char c;

	while (len--)
	{
		c = *hex++;
		if (c >= '0' && c <= '9')
			t = c - '0';
		else if (c >= 'a' && c <= 'f')
			t = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			t = c - 'A' + 10;
		else
			t = 0;
		res |= t << (len * 4);
	}

	return res;
}

int psp_regist_sys_info(char* tachyon_ver, char* fuseid)
{
	unsigned int tachyon = (unsigned int)hex_to_ull(tachyon_ver);
	unsigned long long fuseset = hex_to_ull(fuseid);


	if (tachyon && fuseset)
	{
		pspSysregSetFuseId(fuseset);
		pspSysregSetTachyonVersion(tachyon);

		printf("Tachyon: 0x%X\n", pspSysregGetTachyonVersion());
		printf("Fuse Id: 0x%llX\n", pspSysregGetFuseId());
		return 0;
	}

	return -1;
}

int psp_init_nand_drv(char* dname)
{
	FILE* fp_data;

	//open file
	if (dname == NULL)
	{
		printf("Specify the input file.\n");
		return -1;
	}

	if (!(fp_data = fopen(dname, "rb")))
	{
		printf("Can't open %s\n", dname);
		return -1;
	}

	// Get data file size.
	_fseeki64(fp_data, 0, SEEK_END);
	unsigned int nand_size = (unsigned int)_ftelli64(fp_data);
	_fseeki64(fp_data, 0, SEEK_SET);

	unsigned char* nand = (unsigned char*)malloc(nand_size);
	if (nand == NULL)
		return -1;

	memset(nand, 0, nand_size);
	printf("Nand size: 0x%X\n", nand_size);
	fread(nand, nand_size, 1, fp_data);

	pspNandInit(nand, nand_size);

	printf("Total blocks: 0x%X\n", pspNandGetTotalBlocks());

	return 0;
}

int psp_process_ipl()
{
	FILE* fp_out;
	int i;
	char* iplfname = "psp_ipl.bin";

	if (!(fp_out = fopen(iplfname, "wb")))
	{
		printf("Can't Create %s\n", iplfname);
		return -1;
	}

	int pages_per_block = pspNandGetPagesPerBlock();
	int page_size = pspNandGetPageSize();

	unsigned char* ipl_table = (unsigned char*)malloc(page_size);
	unsigned char* page = (unsigned char*)malloc(page_size);
	unsigned char* extra = (unsigned char*)malloc(0x10);
	if ((ipl_table == NULL) || (page == NULL) || (extra == NULL))
		return -1;

	if (pspNandReadPages(4 * pages_per_block, ipl_table, NULL, 1))
		return -1;

	unsigned short* table_entry = (unsigned short* )ipl_table;
	while (*(unsigned short*)(table_entry) != 0)
	{

		for (i = 0; i < pages_per_block; i++)
		{
			if (pspNandReadPages(*(unsigned short*)(table_entry)*pages_per_block + i, page, extra, 1))
				return -1;

			if (*(unsigned int*)(extra + 8) == 0x6DC64A38)
				fwrite(page, page_size, 1, fp_out);
		}
		table_entry ++;
	}

	fclose(fp_out);
	free(ipl_table);
	free(extra);

	return 0;
}

int psp_process_id_storage()
{
	if(pspIdStorageInit())
		return -1;

	unsigned int leaf_size = pspIdStorageGetLeafSize();
	unsigned char* leaf = (unsigned char*)malloc(leaf_size);

	if (leaf == NULL)
		return -1;

	int s, currkey, res;
	FILE* f;
	char filepath[32];

	res = _mkdir ("keys");

	if (res != 0)
	{
		if (errno != EEXIST)
		{
			printf("Can't create folder %s\n", "keys");
			return -1;
		}
	}

	for (currkey = 0; currkey < 0xfff0; currkey++)
	{
		s = pspIdStorageReadLeaf(currkey, leaf);
		if (s != 0) continue;
		sprintf(filepath, "keys/0x%04X.bin", currkey);
		f = fopen(filepath, "wb");
		if (f <= 0) continue;
//		printf(" Saving key %04X to file %s...", currkey, filepath);
		fwrite(leaf, leaf_size, 1, f);
		fclose(f);
//		printf(" done.\n");
	}

	return 0;
}

int write_reserved_blocks(FILE* f, int num)
{
	int i;
	unsigned char* blockbuf = (unsigned char*)malloc(0x200 * 0x20);
	if (blockbuf == NULL)
		return -1;

	memset(blockbuf, 0, 0x4000);

	for (i = 0; i < num; i++)
	{
		fwrite(blockbuf, 0x4000, 1, f);
	}

	free(blockbuf);

	return 0;
}

int psp_process_logic_data()
{
	int res;
	unsigned int lbn = 0;
	unsigned int flash_num = 0;
	unsigned int file_pos_blocks = 0;
	unsigned int EBR_offset, EBR0_offset, reserved_blocks, i;

	if (pspLfatfsCreateLPTable())
		return -1;

	FILE* fp_data;
	if (!(fp_data = fopen("psp_hdd.bin", "wb")))
	{
		printf("Can't create %s\n", "psp_hdd.bin");
		return -1;
	}

	FILE* lflash;
	char filepath[32];

	res = _mkdir("lflash");
	if(res != 0)
	{
		if (errno != EEXIST) 
		{
			printf("Can't create folder %s\n", "lflash");
			fclose(fp_data);
			return -1;
		}
	}

	//read MBR
	pspNandSetScramble(0);
	if (pspLfatfsReadLogicalBlock(lbn, buf_data))
	{
		printf("pspLfatfsReadLogicalBlock() failed\n");
		fclose(fp_data);
		return -1;
	}

	ptab_entry_t etx_part0 = *(ptab_entry_t*)&buf_data[0x1BE];
	EBR0_offset = etx_part0.relative_sectors / 0x20;
	EBR_offset = etx_part0.relative_sectors / 0x20;
	fwrite(buf_data, 0x4000, 1, fp_data);
	file_pos_blocks++;

	//write reserved blocks
	reserved_blocks = (etx_part0.relative_sectors / 0x20) - file_pos_blocks;
	if(write_reserved_blocks(fp_data, reserved_blocks))
	{
		printf("write_reserved_blocks() failed\n");
		fclose(fp_data);
		return -1;
	}

	file_pos_blocks += reserved_blocks;

	//read all flashes in loop
	while (TRUE)
	{
		sprintf(filepath, "lflash/flash%d.fat", flash_num);
		if (!(lflash = fopen(filepath, "wb")))
		{
			printf("Can't create %s\n", filepath);
			fclose(fp_data);
			return -1;
		}

		//read EBR
		lbn = EBR_offset;
		if (pspLfatfsReadLogicalBlock(lbn, buf_data))
		{
			printf("pspLfatfsReadLogicalBlock() failed\n");
			fclose(fp_data);
			fclose(lflash);
			return -1;
		}

		ptab_entry_t fat_part = *(ptab_entry_t*)&buf_data[0x1BE];
		ptab_entry_t next_etx_part = *(ptab_entry_t*)&buf_data[0x1CE];

		printf("Flash%d partition offset = 0x%X  size = 0x%X\n", flash_num, (fat_part.relative_sectors * 0x200 + EBR_offset * 0x4000), fat_part.total_sectors * 0x200);
		fwrite(buf_data, 0x4000, 1, fp_data);
		file_pos_blocks++;

		if (flash_num == 0)
			if (pspLfatfsCheckScramble(lbn + (fat_part.relative_sectors / 0x20)))
			{
				printf("pspLfatfsCheckScramble() failed\n");
				fclose(fp_data);
				fclose(lflash);
				return -1;
			}

		//read Flash
		for (i = 0; i < (fat_part.total_sectors / 0x20); i++)
		{
			lbn = EBR_offset + (fat_part.relative_sectors / 0x20) + i;
			if (lbn < (EBR_offset + 2))
			{
				pspNandSetScramble(pspLfatfsGenSeedx1(flash_num));
				goto label_READ;
			}

			if (lbn < (EBR_offset + (fat_part.total_sectors / 0x20)))
			{
				if (pspLfatfsFindFwSeedx(lbn))
				{
					printf("Lflash seedx 2 not found, exitting\n");
					fclose(fp_data);
					fclose(lflash);
					return -1;
				}

				pspNandSetScramble(pspLfatfsGenSeedx2(flash_num));
			}

		label_READ:
			if(pspLfatfsReadLogicalBlock(lbn, buf_data))
			{
				printf("pspLfatfsReadLogicalBlock() failed\n");
				fclose(fp_data);
				fclose(lflash);
				return -1;
			}

			fwrite(buf_data, 0x4000, 1, fp_data);
			fwrite(buf_data, 0x4000, 1, lflash);
			file_pos_blocks++;
		}

		pspNandSetScramble(0);

		if (next_etx_part.relative_sectors == 0)
			break;

		//write reserved blocks
		reserved_blocks = EBR0_offset + (next_etx_part.relative_sectors / 0x20) - file_pos_blocks;
		if(write_reserved_blocks(fp_data, reserved_blocks))
		{
			printf("write_reserved_blocks() failed\n");
			fclose(fp_data);
			fclose(lflash);
			return -1;
		}

		file_pos_blocks += reserved_blocks;

		EBR_offset = EBR0_offset + (next_etx_part.relative_sectors / 0x20);
		fclose(lflash);
		flash_num++;
	}

	reserved_blocks = pspLfatfsGetTotalLogicalBlocks() - file_pos_blocks;
	if(write_reserved_blocks(fp_data, reserved_blocks))
	{
		printf("write_reserved_blocks() failed\n");
		fclose(fp_data);
		fclose(lflash);
		return -1;
	}

	fclose(fp_data);

	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		printf("----- Psp Nand Dump Unscrambler ver 0.1 ------\n");
		printf("Usage: %s <dump_file> <Tachyon> <FuseId> \n", argv[0]);
		return -1;
	}

	if (psp_regist_sys_info(argv[2], argv[3]))
	{
		printf("psp_regist_sys_info() failed\n");
		return -1;
	}

	if (psp_init_nand_drv(argv[1]))
	{
		printf("psp_init_nand_drv() failed\n");
		return -1;
	}

	if (psp_process_ipl())
	{
		printf("psp_process_ipl() failed\n");
		return -1;
	}

	if (psp_process_id_storage())
	{
		printf("psp_process_id_storage() failed\n");
		return -1;
	}

	if (psp_process_logic_data())
	{
		printf("psp_process_logic_data() failed\n");
		return -1;
	}

	return 0;
}