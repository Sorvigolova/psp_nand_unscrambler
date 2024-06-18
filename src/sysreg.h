#ifndef _SYSREG_H_
#define _SYSREG_H_

unsigned long long pspSysregGetFuseId();

void pspSysregSetFuseId(unsigned long long fuseid);

unsigned int pspSysregGetTachyonVersion();

void pspSysregSetTachyonVersion(unsigned int version);

#endif
