#include <stdio.h>
#include <stdlib.h>

#include "nand.h"

unsigned int page_size = 0x200;
unsigned int pages_per_block = 0x20;
unsigned int extra_size = 0x10;
unsigned int seedx = 0;
unsigned char* nand = 0;
unsigned int total_pages = 0;
unsigned int total_blocks = 0;
int is_nand_initialized = 0;

unsigned int bitrev(unsigned int x)
{
	x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
	x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
	x = ((x >> 4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) << 4);
	x = ((x >> 8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) << 8);
	x = ((x >> 16) & 0xffffu) | ((x & 0xffffu) << 16);
	return x;
}

int pspNandInit(unsigned char* nandbuf, unsigned int size)
{
	nand = nandbuf;
	total_pages = size / (page_size + extra_size);
	total_blocks = total_pages / pages_per_block;
	is_nand_initialized = 1;
	return 0;
}

unsigned int pspNandGetPageSize()
{
	return page_size;
}

unsigned int pspNandGetPagesPerBlock()
{
	return pages_per_block;
}

 unsigned int pspNandGetTotalBlocks()
 {
	 return total_blocks;
 }

void pspNandSetScramble(unsigned int code)
{
	seedx = code;
}

int pspNandUnscramblePage(unsigned int ppn, unsigned char* buf)
{
	if(!seedx)
		return 0;

	unsigned char* page = (unsigned char*)malloc(page_size);
	if (page == NULL)
		return -1;

	memcpy(page, buf, 0x200);

	//setup
	unsigned int e1, e2, e3, e4;
	unsigned char* ptr = buf;
	unsigned int s = _rotr(seedx, 0x15);
	unsigned int k = _rotr(ppn, 0x11) ^ ((s << 3) - s);
	unsigned int sel = (0x10 * ((ppn ^ s) & 0x1F));

	//loop
	do
	{
		e1 = *(unsigned int*)(page + sel);
		e2 = *(unsigned int*)(page + sel + 4);
		e3 = *(unsigned int*)(page + sel + 8);
		e4 = *(unsigned int*)(page + sel + 0xC);
		sel += 0x10;
		e1 -= k;
		k += e1;
		*(unsigned int*)ptr = e1;
		e2 -= k;
		k ^= e2;
		*(unsigned int*)(ptr + 4) = e2;
		e3 -= k;
		k -= e3;
		*(unsigned int*)(ptr + 8) = e3;
		e4 -= k;
		k += e4;
		*(unsigned int*)(ptr + 0xC) = e4;
		ptr += 0x10;
		sel &= 0x5FF;
		k = bitrev(k + s);
	} while (ptr != buf + 0x200);

	free(page);
	return 0;
}

int pspNandReadPages(unsigned int ppn, unsigned char* user, unsigned char* spare, unsigned int len)
{
	if (!is_nand_initialized)
		return -1;

	if (ppn > total_pages)
		return -1;

	unsigned int i;
	unsigned int offset = ppn * (page_size + extra_size);

	for (i = 0; i < len; i++)
	{
		if (user)
		{
			memcpy((user + (i * page_size)), (nand + offset + (i * (page_size + extra_size))), page_size);
			if(pspNandUnscramblePage(ppn + i, user + (i * page_size)))
				return -1;
		}

		if (spare)
			memcpy((spare + (i * extra_size)), (nand + offset + page_size + (i * (page_size + extra_size))), extra_size);
	}
	return 0;
}

int pspNandIsBadBlock(unsigned int ppn)
{
	if (!is_nand_initialized)
		return -1;

	if (ppn % pages_per_block)
		return -1;

	unsigned char* extra = (unsigned char*)malloc(0x10);

	if (extra == NULL)
		return -1;

	int result;
	int attempt = 0;

	while (1)
	{
		attempt++;
		result = pspNandReadPages(ppn, NULL, extra, 1);

		if (result >= 0)
			break;

		if (attempt >= 4)
		{
			free(extra);
			return result;
		}
	}

	result = (extra[5] != 0xFF);
	free(extra);
	return result;
}
