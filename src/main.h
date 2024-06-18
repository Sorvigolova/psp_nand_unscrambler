#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "sha1.h"

typedef struct ptab_entry
{
	unsigned char boot_ind;
	unsigned char start_head;
	unsigned char start_sect;
	unsigned char start_cyl;
	unsigned char sys_id;
	unsigned char end_head;
	unsigned char end_sect;
	unsigned char end_cyl;
	unsigned int  relative_sectors;
	unsigned int  total_sectors;
} ptab_entry_t;