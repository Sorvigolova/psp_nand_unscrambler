#include <stdio.h>
#include <stdlib.h>
#include "idstorage.h"
#include "sha1.h"
#include "nand.h"
#include "sysreg.h"

unsigned int id_storage_seedx = 0;
unsigned short* leaf_table = 0;
int is_idstorage_initialized = 0;

unsigned int pspIdStorageGetLeafSize()
{
	return 0x200;
}

unsigned int pspIdStorageGetSeedx(unsigned long long fuse_id)
{
	unsigned int scramble;
	unsigned char buf[0x10] = { 0 };
	unsigned char sha1hash[0x14] = { 0 };

	*(unsigned long long*)& buf = fuse_id;
	*(unsigned int*)&buf[8] = (unsigned int)(fuse_id & 0xFFFFFFFF) * 2;
	*(unsigned int*)&buf[0xC] = 0xD41D8CD9;
	sha1(buf, 0x10, sha1hash);
	scramble = (*(int*)&sha1hash ^ *(int*)&sha1hash[0xC]) + *(int*)&sha1hash[8];
	return scramble;
}

int pspIdStorageInit()
{
	int pages_per_block = pspNandGetPagesPerBlock();
	int page_size = pspNandGetPageSize();
	unsigned char* ids_table = (unsigned char*)malloc(page_size * 2);
	unsigned char* page = (unsigned char*)malloc(page_size);
	unsigned char* extra = (unsigned char*)malloc(0x10);

	unsigned int ppn = 0x600;
	unsigned int offset = 0;
	while (1)
	{
		pspNandReadPages(ppn, NULL, extra, 1);
		if (*(unsigned long long*)(extra + 4) != 0xFFFFFFFFFFFFFFFFULL)
			break;

		offset += pages_per_block;
		ppn = offset + 0x600;
		if (offset >= 0x200)
			return -1;
	}

	unsigned int tachyon_version = pspSysregGetTachyonVersion();
	if (tachyon_version >= 0x500000)
		id_storage_seedx = pspIdStorageGetSeedx(pspSysregGetFuseId());

	pspNandSetScramble(id_storage_seedx);
	pspNandReadPages(offset + 0x600, ids_table, NULL, 2);

	leaf_table = (unsigned short*)ids_table;
	is_idstorage_initialized = 1;

	return 0;
}

int _sceIdStorageSearchPage(unsigned int key)
{
	unsigned int i;
	for (i = 0; i < 0x200; i++)
	{
		if (leaf_table[i] == key)
			return i;
	}
	return -1;
}

int pspIdStorageReadLeaf(unsigned int key, unsigned char* buffer)
{
	if (key > 0xFFEF)
		return -1;

	if (buffer == NULL)
		return -1;

	int page = _sceIdStorageSearchPage(key);

	if (page < 0)
		return -1;

	unsigned char* leaf = (unsigned char*)malloc(0x200);

	if (leaf == NULL)
		return -1;

	pspNandSetScramble(id_storage_seedx);

	if (pspNandReadPages(page + 0x600, leaf, NULL, 1))
		return -1;

	memcpy(buffer, leaf, 0x200);
	return 0;
}
