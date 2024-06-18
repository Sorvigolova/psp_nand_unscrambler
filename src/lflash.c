#include <stdio.h>
#include <stdlib.h>
#include <direct.h>

#include "lflash.h"
#include "nand.h"
#include "sysreg.h"

const unsigned char zeroes[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
unsigned char pbr_signature[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA };
unsigned int* LPT = 0;
unsigned int total_logical_blocks = 0;
int is_lpt_initialized = 0;
int is_fw_seed_found = 0;
unsigned int fw_seed = 0;
unsigned int seed_alg = 0;
unsigned int lflash_scramble_flag = 0b1101; //bit0 for flash0, bit1 for flash1, etc... 

unsigned int pspLfatfsGetTotalLogicalBlocks()
{
	return total_logical_blocks;
}

int pspLfatfsCreateLPTable()
{
	if (is_lpt_initialized)
		return 0;

	unsigned int phys_block, logic_block;
	unsigned int pages_per_block = pspNandGetPagesPerBlock();
	unsigned int total_blocks = pspNandGetTotalBlocks();

	if (total_blocks == 0x800)
		total_logical_blocks = 0x780;
	else if (total_blocks == 0x1000)
		total_logical_blocks = 0xF00;
	else
	{
		printf("Unknown NAND size!\n");
		return -1;
	}

	unsigned char* extra = (unsigned char*)malloc(0x10);
	unsigned int* _LPT = (unsigned int*)malloc(total_blocks * sizeof(unsigned int));
	if ((_LPT == NULL) || (extra == NULL))
		return -1;

	memset(_LPT, 0xff, total_blocks * sizeof(unsigned int));

	for (phys_block = 0; phys_block < total_blocks; phys_block++)
	{
		if (pspNandIsBadBlock(phys_block * pages_per_block) == 1)
		{
			printf("BLOCK %04X is bad\n", phys_block);
		}
		else
		{
			if (pspNandReadPages(phys_block * pages_per_block, NULL, extra, 1))
				return -1;

			if (*(unsigned int*)&extra[8] == 0x00000000)
			{
				logic_block = extra[6] * 0x100 | extra[7];
//				printf("BLOCK %04X=%04X\n", phys_block, logic_block);

				_LPT[logic_block] = phys_block;
			}
			else
			{
//				printf("BLOCK %04X invalid\n", phys_block);
			}
		}
	}

	LPT = _LPT;
	is_lpt_initialized = 1;

	return 0;
}

int pspLfatfsReadLogicalBlock(unsigned int lbn, unsigned char* user)
{
	if (!is_lpt_initialized)
		return -1;

	unsigned int pages_per_block = pspNandGetPagesPerBlock();
	unsigned char* blockbuf = (unsigned char*)malloc(0x200 * 0x20);

	if (blockbuf == NULL)
		return -1;

	if (LPT[lbn] & 0x8000)
	{
//		printf("block %04X undefined\n", lbn);
		memset(blockbuf, 0xff, 0x4000);
	}
	else
	{
//		printf("block %04X-%04X\n", lbn, LPT[lbn]);
		pspNandReadPages(LPT[lbn] * pages_per_block, blockbuf, NULL, pages_per_block);
	}

	memcpy(user, blockbuf, 0x200 * 0x20);

	return 0;
}

unsigned int pspLfatfsGenSeedx1(unsigned int flashnum)
{
	unsigned int seed = 0;
	unsigned long long fuseid = pspSysregGetFuseId();
	unsigned int fuseid_90 = (unsigned int)((fuseid & 0xFFFFFFFF00000000ULL) / 0x100000000);
	unsigned int fuseid_94 = (unsigned int)(fuseid & 0xFFFFFFFFULL);

	if (lflash_scramble_flag & (1 << flashnum))
	{
		seed = fuseid_94 ^ (_rotr(fuseid_90, 0xFE * flashnum));
		if (!seed)
			seed = _rotr(0xC4536DE6, -(char)flashnum);
	}

	return seed;
}

unsigned int pspLfatfsGenSeedx2(unsigned int flashnum)
{
	unsigned int seed = 0;
	unsigned long long fuseid = pspSysregGetFuseId();
	unsigned int fuseid_90 = (unsigned int)((fuseid & 0xFFFFFFFF00000000ULL) / 0x100000000);
	unsigned int fuseid_94 = (unsigned int)(fuseid & 0xFFFFFFFFULL);
	unsigned int fw_seedx = fw_seed;

	if (fw_seedx == 0)
		return 0;

	if (seed_alg == 1)
	{
		if (lflash_scramble_flag &(1 << flashnum))
		{
			seed = fuseid_94 ^ (_rotr(fuseid_90, 0xFE * flashnum)) ^ fw_seedx;

			if (!seed)
				seed = _rotr(fw_seedx, -(char)flashnum);
		}
	}
	else if (seed_alg == 2)
	{
		if (lflash_scramble_flag & (1 << flashnum))
		{
			
			if (flashnum == 3)
				seed = 0x3C22812A;
			else
			{
				seed = fuseid_94 ^ (_rotr(fuseid_90, 0xFE * flashnum)) ^ fw_seedx;

				if (!seed)
					seed = _rotr(fw_seedx, -(char)flashnum);
			}
		}
	}
	else if (seed_alg == 3)
	{
		if (lflash_scramble_flag & (1 << flashnum))
		{

			if (flashnum == 3)
				seed = 0x3C22812A;
			else
			{
				seed = fuseid_94 ^ (_rotr(fuseid_90, 2 * flashnum)) ^ fw_seedx;

				if (!seed)
					seed = _rotr(fw_seedx, flashnum);
			}
		}
	}
	else if (seed_alg == 4)
	{
		if (lflash_scramble_flag & (1 << flashnum))
		{
			if (flashnum == 3)
				seed = 0x3C22812A;
			else
			{
				seed = fuseid_94 ^ (_rotr(fuseid_90, 3 * flashnum)) ^ fw_seedx;

				if (!seed)
					seed = _rotr(fw_seedx, flashnum);
			}
		}
	}

	return seed;
}

typedef struct
{
	const unsigned int seed;
	const int algorithm;
	const char* name;
} FW_SEED;

static FW_SEED g_FwSeeds[] =
{
	{0x00000000, 0, "not_encr"},
	{0xFBFC21B8, 1, "ver_3000"},
	{0x9B9D2561, 1, "ver_3300"},
	{0xDF6238BA, 2, "ver_3500"},
	{0xF490D272, 2, "ver_3700"},
	{0xD2978A5B, 2, "ver_3800"},
	{0x66C691E4, 3, "ver_4200"},
	{0x9232CA96, 3, "ver_5000"},
	{0xC675E36C, 4, "ver_5700"},
	{0xCCB8E98B, 4, "ver_6000"},
	{0xE3701A7B, 4, "ver_6300"},
	{0x556D81FE, 4, "ver_6600"},
};

unsigned int pspLfatfsFindFwSeedx(unsigned int lbn)
{
	if (is_fw_seed_found)
		return 0;

	printf("Bruteforcing firmware seed...\n");
	FW_SEED *fw_s;
	unsigned char* blockbuf = (unsigned char*)malloc(0x200 * 0x20);
	if (blockbuf == NULL)
		return -1;

	for (unsigned int iSEED = 0; iSEED < sizeof(g_FwSeeds) / sizeof(FW_SEED); iSEED++)
	{
		fw_s = &g_FwSeeds[iSEED];

		fw_seed = fw_s->seed;
		seed_alg = fw_s->algorithm;

		if (seed_alg == 0)
			lflash_scramble_flag = 0;
		else if (seed_alg == 1)
			lflash_scramble_flag = 0b101;
		else
			lflash_scramble_flag = 0b1101;

		pspNandSetScramble(pspLfatfsGenSeedx2(0));
		pspLfatfsReadLogicalBlock(lbn, blockbuf);
		if (!memcmp(zeroes, blockbuf + 0x3FF0, 0x10))
		{
			is_fw_seed_found = 1;
			printf("Seed found: %s\n", fw_s->name);
			return 0;
		}
	}

	printf("Seed not found\n");
	return -1;
}

int pspLfatfsCheckScramble(unsigned int pbr_lbn)
{
	unsigned char* blockbuf = (unsigned char*)malloc(0x200 * 0x20);
	if (blockbuf == NULL)
		return -1;

	pspLfatfsReadLogicalBlock(pbr_lbn, blockbuf);

	char fmt[9] = { 0 };
	memcpy(fmt, blockbuf + 3, 8);

	if (!memcmp(pbr_signature, blockbuf + 0x1F0, 0x10))
	{
		printf("Logic data is not scrambled\n");
		printf("flash format : %s\n", fmt);
		lflash_scramble_flag = 0;
		return 0;
	}
	else
	{
		pspNandSetScramble(pspLfatfsGenSeedx1(0));
		pspLfatfsReadLogicalBlock(pbr_lbn, blockbuf);
		memcpy(fmt, blockbuf + 3, 8);

		if (!memcmp(pbr_signature, blockbuf + 0x1F0, 0x10))
		{
			printf("Logic data is scrambled\n");
			printf("flash format : %s\n", fmt);
			return 0;
		}
	}
	printf("Unknown scrambling, stop.\n");
	return -1;
}
