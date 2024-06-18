#ifndef _NAND_H_
#define _NAND_H_

int pspNandInit(unsigned char* nandbuf, unsigned int size);

unsigned int pspNandGetPageSize();

unsigned int pspNandGetPagesPerBlock();

unsigned int pspNandGetTotalBlocks();

void pspNandSetScramble(unsigned int code);

int pspNandReadPages(unsigned int ppn, unsigned char* user, unsigned char* spare, unsigned int len);

int pspNandIsBadBlock(unsigned int ppn);

#endif
