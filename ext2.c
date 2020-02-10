typedef struct
{
	char* address;
} DISK_MEMORY;

#include "ext2.h"
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )

int ext2_write(EXT2_NODE* file, unsigned long offset, unsigned long length, const char* buffer)
{
}

UINT32 get_free_inode_number(EXT2_FILESYSTEM* fs);

// format
/* 	
	super block 초기화
	group descriptor 초기화
	block bitmap, inode bitmap, inode table 초기화
	block group의 개수를 고려한 write 수행
	root directory 생성 
*/
int ext2_format(DISK_OPERATIONS* disk)
{


	EXT2_SUPER_BLOCK sb; // super block
	EXT2_GROUP_DESCRIPTOR gd; // group descriptor 2개
	EXT2_GROUP_DESCRIPTOR  gd_another_group;

	// 그룹 당 섹터개수(boot block때문에 디스크 전체 섹터에서 1빼줌)
	QWORD sector_num_per_group = (disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;
	int i, gi, j;
	const int BOOT_SECTOR_BASE = 1; // boot block이 0, 그다음부터가 block group의 boot block
	char sector[MAX_SECTOR_SIZE]; // 섹터버퍼

	// super block 채우기
	if (fill_super_block(&sb, disk->numberOfSectors, disk->bytesPerSector) != EXT2_SUCCESS)
		return EXT2_ERROR;

	ZeroMemory(sector, sizeof(sector));
	memcpy(sector, &sb, sizeof(sb)); // 섹터버퍼에 superblock내용 가져와서
	disk->write_sector(disk, BOOT_SECTOR_BASE + 0, sector); // 디스크에 write

	// group descriptor 채우기
	if (fill_descriptor_block(&gd, &sb, disk->numberOfSectors, disk->bytesPerSector) != EXT2_SUCCESS)
		return EXT2_ERROR;

	// 다음 group descriptor에 대입
	gd_another_group = gd;
	gd_another_group.free_inodes_count = NUMBER_OF_INODES / NUMBER_OF_GROUPS;
	gd_another_group.free_blocks_count = sb.free_block_count / NUMBER_OF_GROUPS;

	// 섹터버퍼에 group descriptor들 넣기(group descriptor table)
	ZeroMemory(sector, sizeof(sector));
	for (j = 0; j < NUMBER_OF_GROUPS; j++)
	{
		if (j == 0)memcpy(sector + j * sizeof(gd), &gd, sizeof(gd));
		else memcpy(sector + j * sizeof(gd_another_group), &gd_another_group, sizeof(gd_another_group));
	}

	// 디스크에 write
	disk->write_sector(disk, BOOT_SECTOR_BASE + 1, sector);

	// block bitmap
	// 일단 전부 0으로 setting하고 메타데이터에 해당하는 17개 블록 (17bit)를 1로 setting
	ZeroMemory(sector, sizeof(sector));

	sector[0] = 0xff;
	sector[1] = 0xff;
	sector[2] = 0x01;
	disk->write_sector(disk, BOOT_SECTOR_BASE + 2, sector);


	// inode bitmap
	// 일단 전부 0으로 setting하고 예약된 부분(10번까지, 10bit)을 1로 세팅
	ZeroMemory(sector, sizeof(sector));

	sector[0] = 0xff;
	sector[1] = 0x03;
	disk->write_sector(disk, BOOT_SECTOR_BASE + 3, sector);

	// inode table
	ZeroMemory(sector, sizeof(sector));

	for (i = 4; i < sb.first_data_block_each_group; i++)
	disk->write_sector(disk, BOOT_SECTOR_BASE + i, sector);


	// block group 2
	for (gi = 1; gi < NUMBER_OF_GROUPS; gi++)
	{
		sb.block_group_number = gi; // super block의 block group number가 달라짐


		ZeroMemory(sector, sizeof(sector));
		memcpy(sector, &sb, sizeof(sb));


		// 해당 순서의 block group위치에 super block 써줌
		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE, sector);

		// 섹터버퍼에 group descriptor들 넣기(group descriptor table)
		ZeroMemory(sector, sizeof(sector));
		for (j = 0; j < NUMBER_OF_GROUPS; j++)
		{
			memcpy(sector + j * sizeof(gd), &gd, sizeof(gd));
		}

		// 해당 순서의 block group위치 + 1(super block만큼)에 write
		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE + 1, sector);


		// block bitmap
		ZeroMemory(sector, sizeof(sector));
		sector[0] = 0xff;
		sector[1] = 0xff;
		sector[2] = 0x01;
		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE + 2, sector);


		// inode bitmap
		// 0번째 inode bitmap에서는 예약된 inode가 10번까지 있었지만, 이외의 block group에서는 예약된 inode가 없어서 모두 0으로 초기화
		ZeroMemory(sector, sizeof(sector));

		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE + 3, sector);

		// inode table
		ZeroMemory(sector, sizeof(sector));
		for (i = 4; i < sb.first_data_block_each_group; i++)
			disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE + i, sector);
	}

	PRINTF("max inode count : %u\n", sb.max_inode_count);
	PRINTF("total block count : %u\n", sb.block_count);
	PRINTF("byte size of inode structure : %u\n", sb.inode_structure_size);
	PRINTF("block byte size : %u\n", MAX_BLOCK_SIZE);
	PRINTF("total sectors count : %u\n", NUMBER_OF_SECTORS);
	PRINTF("sector byte size : %u\n", MAX_SECTOR_SIZE);
	PRINTF("\n");

	// root directory 생성
	create_root(disk, &sb);

	return EXT2_SUCCESS;
}

// Super Block 
int write_super_block(EXT2_SUPER_BLOCK *sb, DISK_OPERATIONS *disk)
{
	QWORD sector_num_per_group = (disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;
	char sector[MAX_SECTOR_SIZE];
	const int BOOT_SECTOR_BASE = 1;
	
	// 디스크에 write
	for (int gi = 0; gi < NUMBER_OF_GROUPS; gi++)
	{
		sb->block_group_number = gi; // super block의 block group number가 달라짐
		sb->first_data_block_each_group = gi*sector_num_per_group + 17;

		ZeroMemory(sector, sizeof(sector));
		memcpy(sector, &sb, sizeof(sb));

		// 해당 순서의 block group위치에 super block 써줌
		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE, sector);
	}

	return EXT2_SUCCESS;
}

// Group Descriptor
int write_group_descriptor(DISK_OPERATIONS* disk, EXT2_GROUP_DESCRIPTOR* gd, int groupNum)
{
	char sector[MAX_SECTOR_SIZE];
	const int BOOT_SECTOR_BASE = 1;
	QWORD sector_num_per_group = (disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;

	// 기존의 group descriptor table 읽어와서
	disk->read_sector(disk, BOOT_SECTOR_BASE+1, sector);

	// 전달받은 group descriptor부분만 수정
	memcpy(sector + groupNum * sizeof(gd), &gd, sizeof(gd));

	// disk write
	for (int gi = 0; gi < NUMBER_OF_GROUPS; gi++)
	{
		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE + 1, sector);
	}

	return EXT2_SUCCESS;
}

// fill_super_block(&sb, disk->numberOfSectors, disk->bytesPerSector
int fill_super_block(EXT2_SUPER_BLOCK * sb, SECTOR numberOfSectors, UINT32 bytesPerSector)
{

	ZeroMemory(sb, sizeof(EXT2_SUPER_BLOCK));

	sb->max_inode_count = NUMBER_OF_INODES; // 아이노드의 갯수 (200)
	sb->block_count = numberOfSectors; // 4097개
	sb->reserved_block_count = 0; // 예약된 블록의 갯수
	sb->free_block_count = numberOfSectors - (17 * NUMBER_OF_GROUPS) - 1; //
	sb->free_inode_count = NUMBER_OF_INODES - 10;// 아이노드중에서 10개는 이미 사용중
	sb->first_data_block = 1; //
	sb->log_block_size = 0;
	sb->log_fragmentation_size = 0;
	sb->block_per_group = (numberOfSectors - 1) / NUMBER_OF_GROUPS; // 2048개
	sb->fragmentation_per_group = 0;
	sb->inode_per_group = NUMBER_OF_INODES / NUMBER_OF_GROUPS; // 그룹당 아이노드의 갯수 100개
	sb->magic_signature = 0xEF53; // 슈퍼블록인것을 알기 위한 단서
	sb->errors = 0; // 에러의 갯수
	sb->first_non_reserved_inode = 11; // 아이노드중에서 10개는 사용중이라서 11번째 부터 사용가능하다
	sb->inode_structure_size = 128; // 아이노드의 크기는 128바이트이다
	sb->block_group_number = 0;
	sb->first_data_block_each_group = 1 + 1 + 1 + 1 + 13; // 첫번째 데이터 블록
	return EXT2_SUCCESS;
	
	
}
                             
int fill_descriptor_block(EXT2_GROUP_DESCRIPTOR * gd, EXT2_SUPER_BLOCK * sb, SECTOR numberOfSectors, UINT32 bytesPerSector)
{
	ZeroMemory(gd, sizeof(EXT2_GROUP_DESCRIPTOR));


	gd->start_block_of_block_bitmap = 2;
	gd->start_block_of_inode_bitmap = 3;
	gd->start_block_of_inode_table = 4;
	// group개수로 나누고, 나머지까지 더해줌
	gd->free_blocks_count = (UINT16)(sb->free_block_count / NUMBER_OF_GROUPS + sb->free_block_count % NUMBER_OF_GROUPS);
	gd->free_inodes_count = (UINT16)(((sb->free_inode_count) + 10) / NUMBER_OF_GROUPS - 10); //??
	gd->directories_count = 0;


	return EXT2_SUCCESS;
}

// root directory 생성 
int create_root(DISK_OPERATIONS* disk, EXT2_SUPER_BLOCK * sb)
{

	EXT2_DIR_ENTRY * entry ;
	BYTE sector[MAX_SECTOR_SIZE];
	INODE *id;
	EXT2_GROUP_DESCRIPTOR * gd;
	SECTOR rootsector =0;
	EXT2_SUPER_BLOCK * sb2;
	ZeroMemory(sector,MAX_SECTOR_SIZE);
	entry= (EXT2_DIR_ENTRY *)sector;
	memcpy(entry->name,VOLUME_LABLE,13);
	entry->name_len=VOLUME_LABLE;
	entry->inode=2;
	entry++;
	entry->name[0]= DIR_ENTRY_NO_MORE;
	rootsector= 1+sb->first_data_block_each_group; // 부트 코드 땜에 1 더해줌
	disk->write_sector(disk,rootsector,sector);
	// 지금까지 해당 엔트리를 만들어서 넣었고 이제 inode 테이블하고 superblock하고 groupdescriptor 내용 바꾸는 일 해야함
	ZeroMemory(sector,MAX_SECTOR_SIZE);
	sb2= (EXT2_SUPER_BLOCK *)sector ;
	disk->read_sector(disk,1,sector);
	sb2->free_block_count --;
	sb2->free_inode_count --;
	write_super_block(sb,disk);
	gd= (EXT2_GROUP_DESCRIPTOR *)sector ;
	ZeroMemory(sector,MAX_SECTOR_SIZE);
	disk->read_sector(disk,2,gd);
	gd-> free_blocks_count --;
	gd-> free_inodes_count --;
	gd-> directories_count ++;
	for(int i=0; i<NUMBER_OF_GROUPS; i++)
	write_group_descriptor(disk,gd,i);
	ZeroMemory(sector, sizeof(sector));
	disk->read_sector(disk, 3,sector);
	sector[2] |= 0x02;
	disk->write_sector(disk,3,sector); // 블록 비트맵 할당
	ZeroMemory(sector, MAX_SECTOR_SIZE);
	id= (INODE *)sector;
	disk->read_sector(disk,5,sector); // 아이노드 테이블
	id ++;
	id->mode = 0x4000 ;
	id->size =0 ;
	id->uid = 0;
	id->block[0]= 1+sb->first_data_block_each_group ;
	disk->write_sector(disk,5,sector);
	return EXT2_SUCCESS ;
}

void process_meta_data_for_inode_used(EXT2_NODE * retEntry, UINT32 inode_num, int fileType)
{
}

int insert_entry(UINT32 inode_num, EXT2_NODE * retEntry, int fileType)
{
}

UINT32 get_available_data_block(EXT2_FILESYSTEM * fs, UINT32 inode_num)
{
}

void process_meta_data_for_block_used(EXT2_FILESYSTEM * fs, UINT32 inode_num)
{
}

UINT32 expand_block(EXT2_FILESYSTEM * fs, UINT32 inode_num)
{    




}

int meta_read(EXT2_FILESYSTEM * fs, SECTOR group, SECTOR block, BYTE* sector)
{
}
int meta_write(EXT2_FILESYSTEM * fs, SECTOR group, SECTOR block, BYTE* sector)
{
}

int data_read(EXT2_FILESYSTEM * fs, SECTOR group, SECTOR block, BYTE* sector)
{  
   int blockLocation ;
   QWORD sector_num_per_group = (fs->disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;
  // 몇번째 그룹의 몇번째 block을 읽는 함수 , 섹터 버퍼에다가 그내용을 저장한다     
      if(group<0 || group>NUMBER_OF_GROUPS)
      return EXT2_ERROR; 
	  
    blockLocation = group* sector_num_per_group +17 + block;
    

   fs->disk->read_sector(fs->disk,blockLocation   ,sector);

   return EXT2_SUCCESS ;
}
int data_write(EXT2_FILESYSTEM * fs, SECTOR group, SECTOR block, BYTE* sector)
{
	  int blockLocation ;
   QWORD sector_num_per_group = (fs->disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;
  // 몇번째 그룹의 몇번째 block을 읽는 함수 , 섹터 버퍼에다가 그내용을 저장한다     
      if(group<0 || group>NUMBER_OF_GROUPS)
      return EXT2_ERROR; 
	  
    blockLocation = group* sector_num_per_group +17 + block;
    

   fs->disk->write_sector(fs->disk,blockLocation   ,sector);

   return EXT2_SUCCESS ;
}

unsigned char toupper(unsigned char ch);
int isalpha(unsigned char ch);
int isdigit(unsigned char ch);

void upper_string(char* str, int length)
{
while (*str && length-- > 0)
{
*str = toupper(*str);
str++;
}
}

int format_name(EXT2_FILESYSTEM* fs, char* name)
{    
    




}

int lookup_entry(EXT2_FILESYSTEM* fs, const int inode, const char* name, EXT2_NODE* retEntry)
{
}

int find_entry_at_sector(const BYTE* sector, const BYTE* formattedName, UINT32 begin, UINT32 last, UINT32* number)
{    
}

int find_entry_on_root(EXT2_FILESYSTEM* fs, INODE inode, char* formattedName, EXT2_NODE* ret)
{   
    



}

int find_entry_on_data(EXT2_FILESYSTEM* fs, INODE first, const BYTE* formattedName, EXT2_NODE* ret)
{   
}




int get_inode(EXT2_FILESYSTEM * fs, const UINT32 inode, INODE *inodeBuffer)
{    EXT2_GROUP_DESCRIPTOR *gd2;
	BYTE sector[MAX_SECTOR_SIZE];
	ZeroMemory(sector,MAX_SECTOR_SIZE);
	fs->disk->read_sector(fs->disk, 2, sector) ;  
    gd2 = (EXT2_GROUP_DESCRIPTOR *) sector;
	UINT32 inode_group = (inode-1)/fs->sb.inode_per_group; // 몇번째 inode group인지 
	for  ( int i=0 ; i<inode_group; i++)
	  { gd2++ ;}  
    fs->gd= *gd2;
      
	UINT32 inode_structure_size = fs->sb.inode_structure_size; // inode 크기 
	UINT32 inode_start_block = fs->gd.start_block_of_inode_table; // inode 테이블의 시작 블록 

	UINT32 inode_offset = inode % (fs->sb.inode_per_group+1); // 그룹내의 inode table에서 몇번째인지 
	UINT32 inode_sector = (inode_offset*inode_structure_size)/(MAX_SECTOR_SIZE);
	//해당 inode 블럭 
    ZeroMemory(sector,MAX_SECTOR_SIZE);
	INODE* inode_table;
	inode_table = (INODE*)sector;

	fs->disk->read_sector(fs->disk, inode_start_block + inode_sector, sector);

	inodeBuffer = &inode_table[inode_offset%8];
		
	return EXT2_SUCCESS;
}

int read_root_sector(EXT2_FILESYSTEM* fs, BYTE* sector)
{  
}

int ext2_create(EXT2_NODE* parent, char* entryName, EXT2_NODE* retEntry)
{
}


int get_data_block_at_inode(EXT2_FILESYSTEM *fs, INODE inode, UINT32 number)
{    int blockNumber;
	if(number <0 || number>NUMBER_OF_INODES)
      return EXT2_ERROR;	 
    get_inode(fs,number,&inode);
	if(inode.block[0]>0)
	{
          blockNumber= inode.block[0];
		  return blockNumber;
	} 
	return EXT2_ERROR;

}

int ext2_read_superblock(EXT2_FILESYSTEM* fs, EXT2_NODE* root)
{
}

UINT32 get_free_inode_number(EXT2_FILESYSTEM* fs)
{
}

int set_inode_onto_inode_table(EXT2_FILESYSTEM *fs, const UINT32 which_inode_num_to_write, INODE * inode_to_write)
{   // inode 버퍼를 가지고 와서 inode번호를 쓴다 
     if(which_inode_num_to_write<0|| which_inode_num_to_write>NUMBER_OF_INODES)
	 return EXT2_ERROR;
	 EXT2_GROUP_DESCRIPTOR *gd2;
	BYTE sector[MAX_SECTOR_SIZE];
	ZeroMemory(sector,MAX_SECTOR_SIZE);
	fs->disk->read_sector(fs->disk, 2, sector) ;  
    gd2 = (EXT2_GROUP_DESCRIPTOR *) sector;
	UINT32 inode_group = (which_inode_num_to_write-1)/fs->sb.inode_per_group; // 몇번째 inode group인지 
	for  ( int i=0 ; i<inode_group; i++)
	  { gd2++ ;}  
    fs->gd= *gd2;
      
	UINT32 inode_structure_size = fs->sb.inode_structure_size; // inode 크기 
	UINT32 inode_start_block = fs->gd.start_block_of_inode_table; // inode 테이블의 시작 블록 

	UINT32 inode_offset = which_inode_num_to_write % (fs->sb.inode_per_group+1); // 그룹내의 inode table에서 몇번째인지 
	UINT32 inode_sector = (inode_offset*inode_structure_size)/(MAX_SECTOR_SIZE);
	//해당 inode 블럭 
    ZeroMemory(sector,MAX_SECTOR_SIZE);
	INODE* inode_table;
	inode_table = (INODE*)sector;

	fs->disk->read_sector(fs->disk, inode_start_block + inode_sector, sector);
      inode_table[inode_offset%8] = *inode_to_write ;             
     fs->disk->write_sector(fs->disk, inode_start_block+inode_sector,sector);
	return EXT2_SUCCESS;






}

int ext2_lookup(EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry)
{
}

int ext2_read_dir(EXT2_NODE* dir, EXT2_NODE_ADD adder, void* list)
{
}

int read_dir_from_sector(EXT2_FILESYSTEM* fs, BYTE* sector, EXT2_NODE_ADD adder, void* list)
{
}

char* my_strncpy(char* dest, const char* src, int length)
{
while (*src && *src != 0x20 && length-- > 0)
*dest++ = *src++;

return dest;
}

int ext2_mkdir(const EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry)
{
}