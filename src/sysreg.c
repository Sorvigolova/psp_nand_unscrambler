#include <stdio.h>
#include <stdlib.h>

#include "sysreg.h"

unsigned int g_tachyon_version = 0x820000;
//unsigned long long g_fuseid = 0xCA79F317271E;
unsigned long long g_fuseid = 0x353C8C0AAF90;

unsigned long long pspSysregGetFuseId()
{
	return g_fuseid;
}

void pspSysregSetFuseId(unsigned long long fuseid)
{
	g_fuseid = fuseid;
}

unsigned int pspSysregGetTachyonVersion()
{
	return g_tachyon_version;
}

void pspSysregSetTachyonVersion(unsigned int version)
{
	g_tachyon_version = version;
}