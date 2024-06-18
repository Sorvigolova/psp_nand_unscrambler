#ifndef _LFLASH_H_
#define _LFLASH_H_

unsigned int pspLfatfsGetTotalLogicalBlocks();

int pspLfatfsCreateLPTable();

int pspLfatfsReadLogicalBlock(unsigned int lbn, unsigned char* user);

unsigned int pspLfatfsGenSeedx1(unsigned int flashnum);

unsigned int pspLfatfsGenSeedx2(unsigned int flashnum);

unsigned int pspLfatfsFindFwSeedx(unsigned int lbn);

int pspLfatfsCheckScramble(unsigned int pbr_lbn);

#endif
