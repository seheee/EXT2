#ifndef _DISK_H_
#define _DISK_H_

#include "common.h"

typedef struct DISK_OPERATIONS
{
	int		( *read_sector	)( struct DISK_OPERATIONS*, SECTOR, void* );
	int		( *write_sector	)( struct DISK_OPERATIONS*, SECTOR, const void* );
	// 블록의 갯수 
	SECTOR	numberOfSectors;
	// 섹터당 1024바이트다 
	int		bytesPerSector;
	void*	pdata;
} DISK_OPERATIONS;

#endif

