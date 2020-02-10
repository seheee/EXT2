#ifndef _FAT_H_
#define _FAT_H_

#include "common.h"
#include "disk.h"
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#define GNU_SOURCE"

#include <string.h>

#define      EXT2_N_BLOCKS         15
#define		NUMBER_OF_SECTORS		( 4096 + 1 )
#define		NUMBER_OF_GROUPS		2
#define		NUMBER_OF_INODES		200
#define		VOLUME_LABLE			"EXT2 BY NC"

#define MAX_SECTOR_SIZE			1024
#define MAX_BLOCK_SIZE			1024
#define MAX_NAME_LENGTH			256
#define MAX_ENTRY_NAME_LENGTH	11

#define ATTR_READ_ONLY			0x01
#define ATTR_HIDDEN				0x02
#define ATTR_SYSTEM				0x04
#define ATTR_VOLUME_ID			0x08
#define ATTR_DIRECTORY			0x10
#define ATTR_ARCHIVE			0x20
#define ATTR_LONG_NAME			ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID

#define DIR_ENTRY_FREE			0xE5
#define DIR_ENTRY_NO_MORE		0x00
#define DIR_ENTRY_OVERWRITE		1

#define GET_INODE_GROUP(x) ((x) - 1)/( NUMBER_OF_INODES / NUMBER_OF_GROUPS )
#define SQRT(x)  ( (x) * (x)  )
#define TRI_SQRT(x)  ( (x) * (x) * (x) )
#define WHICH_GROUP_BLONG(x) ( ( (x) - 1)  / ( NUMBER_OF_INODES / NUMBER_OF_GROUPS )  )

#define TSQRT(x) ((x)*(x)*(x))
#define GET_INODE_FROM_NODE(x) ((x)->entry.inode)

#ifdef _WIN32
#pragma pack(push,fatstructures)
#endif
#pragma pack(1)

typedef struct
{
	UINT32 max_inode_count; // 파일시스템에서 사용가능한 최대 inode개수
	UINT32 block_count; // 파일시스템 내 전체 블록의 개수
	UINT32 reserved_block_count; // block count의 5%값, 아무대책없이 파일시스템이 꽉차는 경우를 막기위해 존재
	UINT32 free_block_count; // 사용되지 않는 블록의 개수
	UINT32 free_inode_count; // 사용되지 않는 inode 개수

	UINT32 first_data_block; // 첫번째 블록(블록그룹 0이 시작되는 블록)
	UINT32 log_block_size; // 블록의 크기(0은 1KB, 1은 2KB, 2는 4KB)
	UINT32 log_fragmentation_size; // 단편화 발생 시 기록되는 크기(0,1,2로 표현)
	UINT32 block_per_group; // 각 블록 그룹에 속한 블록의 개수
	UINT32 fragmentation_per_group; // 각 블록 그룹에 속한 단편화된 개수
	UINT32 inode_per_group; // 각 블록 그룹에 속한 inode개수
	UINT16 magic_signature; // super block인지를 확인하는 고유값
	UINT16 errors; // 에러처리를 위한 flag
	UINT32 first_non_reserved_inode; // 예약되지 않은 inode의 첫번째 인덱스(10개의 inode가 예약되어있음)
	UINT16 inode_structure_size; // inode구조체의 크기(128byte)

	UINT16 block_group_number; // 현재 super block을 포함하고있는 block group의 번호
	UINT32 first_data_block_each_group; //그룹에 따른 첫번째 데이터 블록 번호
} EXT2_SUPER_BLOCK;

typedef struct
{
	UINT16  mode;         /* File mode */
	UINT16  uid;          /* Low 16 bits of Owner Uid */
	UINT32  size;         /* Size in bytes */
	UINT32  atime;        /* Access time */
	UINT32  ctime;        /* Creation time */
	UINT32  mtime;        /* Modification time */
	UINT32  dtime;        /* Deletion Time */
	UINT16  gid;          /* Low 16 bits of Group Id */
	UINT16  links_count;  /* Links count */
	UINT32  blocks;       /* Blocks count */
	UINT32  flags;        /* File flags */
	UINT32  i_reserved1;   // OS dependent 1
	UINT32  block[EXT2_N_BLOCKS];/* Pointers to blocks */
	UINT32  generation;   /* File version (for NFS) */
	UINT32  file_acl;     /* File ACL */
	UINT32  dir_acl;      /* Directory ACL */
	UINT32  faddr;        /* Fragment address */
	UINT32  i_reserved2[3];   // OS dependent 2
} INODE;

typedef struct
{
	UINT32 start_block_of_block_bitmap; // block bitmap의 블록 번호
	UINT32 start_block_of_inode_bitmap; // inode bitmap의 블록 번호
	UINT32 start_block_of_inode_table; // inode table의 블록 번호
	UINT32 free_blocks_count; // 해당 블록 그룹 내의 비어있는 블록 수
	UINT32 free_inodes_count; // 해당 블록 그룹 내의 비어있는 inode 수
	UINT16 directories_count; // 해당 블록 그룹 내의 생성된 디렉터리 수
	BYTE padding[2];
	BYTE reserved[12];
} EXT2_GROUP_DESCRIPTOR;

typedef struct
{
	UINT32 inode;
	BYTE name[11];
	UINT32 name_len;
	BYTE pad[13];
} EXT2_DIR_ENTRY;

#ifdef _WIN32
#pragma pack(pop, fatstructures)
#else
#pragma pack()
#endif

typedef struct
{
	EXT2_SUPER_BLOCK		sb;
	EXT2_GROUP_DESCRIPTOR	gd;
	DISK_OPERATIONS*		disk;

} EXT2_FILESYSTEM;

typedef struct
{
	UINT32 group;
	UINT32 block;
	UINT32 offset;
} EXT2_DIR_ENTRY_LOCATION;

typedef struct
{
	EXT2_FILESYSTEM * fs;
	EXT2_DIR_ENTRY entry;
	EXT2_DIR_ENTRY_LOCATION location;

} EXT2_NODE;

#define SUPER_BLOCK 0
#define GROUP_DES  1
#define BLOCK_BITMAP 2
#define INODE_BITMAP 3
#define INODE_TABLE(x) (4 + x)

#define FILE_TYPE_FIFO               0x1000
#define FILE_TYPE_CHARACTERDEVICE    0x2000
#define FILE_TYPE_DIR				 0x4000
#define FILE_TYPE_BLOCKDEVICE        0x6000
#define FILE_TYPE_FILE				 0x8000

int meta_read(EXT2_FILESYSTEM *, SECTOR group,SECTOR block, BYTE* sector);
int meta_write(EXT2_FILESYSTEM * fs, SECTOR group, SECTOR block, BYTE* sector);
int data_read(EXT2_FILESYSTEM *, SECTOR group, SECTOR block, BYTE* sector);
int data_write(EXT2_FILESYSTEM * fs, SECTOR group, SECTOR block, BYTE* sector);

int ext2_format(DISK_OPERATIONS* disk);
int ext2_create(EXT2_NODE* parent, char* entryName, EXT2_NODE* retEntry);
int ext2_lookup(EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry);

UINT32 expand_block(EXT2_FILESYSTEM * , UINT32 );
int fill_super_block(EXT2_SUPER_BLOCK * sb, SECTOR numberOfSectors, UINT32 bytesPerSector);
int fill_descriptor_block(EXT2_GROUP_DESCRIPTOR * gd, EXT2_SUPER_BLOCK * sb, SECTOR numberOfSectors, UINT32 bytesPerSector);
int create_root(DISK_OPERATIONS* disk, EXT2_SUPER_BLOCK * sb);
typedef int(*EXT2_NODE_ADD)(EXT2_FILESYSTEM*,void*, EXT2_NODE*);
void process_meta_data_for_block_used(EXT2_FILESYSTEM * fs, UINT32 inode_num);
#endif

