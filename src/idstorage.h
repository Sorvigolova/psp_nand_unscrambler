#ifndef _IDSTORAGE_H_
#define _IDSTORAGE_H_

unsigned int pspIdStorageGetLeafSize();

unsigned int pspIdStorageGetSeedx(unsigned long long fuse_id);

int pspIdStorageInit();

int pspIdStorageReadLeaf(unsigned int key, unsigned char* buffer);

#endif
