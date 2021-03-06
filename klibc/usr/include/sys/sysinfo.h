/*
 * sys/sysinfo.h
 */

#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H

#include <sys/types.h>
#include <linux/kernel.h>

extern int sysinfo(struct sysinfo *info);

#endif				/* _SYS_SYSINFO_H */
