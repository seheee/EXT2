typedef struct
{
	char* address;
} DISK_MEMORY;

#include "ext2.h"
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
int* get_data_block_at_inode(EXT2_FILESYSTEM *fs, INODE *inode);
int ext2_write(EXT2_NODE* file, unsigned long offset, unsigned long length, const char* buffer)
{  
	BYTE	sector[MAX_SECTOR_SIZE];
	DWORD	currentOffset, currentBlock, blockSeq = 0;
	DWORD	blockNumber, sectorNumber, sectorOffset;
	DWORD	readEnd;
	DWORD	blockSize;
	INODE node;
	int i;
	int *dataBlocks ; 
	get_inode(file->fs, file->entry.inode, &node);    // 해당 아이노드를 구하고
	printf("%d \n",file->entry.inode); 
	if(node.blocks ==0)
	{expand_block(file->fs,file->entry.inode);   // 블록 하나 늘려주고 
	file->fs->sb.free_block_count --;
	file->fs->gd.free_blocks_count --;
	write_super_block(&file->fs->sb, file->fs->disk);
	write_group_descriptor(file->fs->disk, &file->fs->gd, file->entry.inode/file->fs->sb.inode_per_group);
	get_inode(file->fs, file->entry.inode, &node);  
	}  // 해당 아이노드를 구하고 
	currentBlock = node.block[0];     // 현재 블록 초기화 
	readEnd = offset + length;
	currentOffset = offset;
    blockSize = node.blocks * MAX_BLOCK_SIZE ;  // 현재 해당 아이노드가 가지고 있는 블록 크기 초기화 해줌 
	i =0;
	while(offset>blockSize-1)   // 만약 offset이 아이노드가 가지고 있는 블록 크기를 넘어선다면 
	{

         expand_block(file->fs,file->entry.inode);
         blockSize += MAX_BLOCK_SIZE ;
		 ++ blockSeq ;
		 i++ ;
	}
	file->fs->sb.free_block_count -= i;
	file->fs->gd.free_blocks_count -= i;
	write_super_block(&file->fs->sb, file->fs->disk);
	write_group_descriptor(file->fs->disk, &file->fs->gd, file->entry.inode/file->fs->sb.inode_per_group);

	get_inode(file->fs,file->entry.inode,&node); // 다시 초기화 시켜준다 
    dataBlocks = get_data_block_at_inode(file->fs,&node);
	currentBlock= dataBlocks[i];  // 해당 블록 받아온다 
	while (currentOffset < readEnd)       // 계속 쓰는 작업 
	{
		DWORD	copyLength;


		blockNumber = currentOffset / MAX_BLOCK_SIZE;    // 해당 아이노드 블록 넘버 
		if (blockSeq != blockNumber)   // 다음 블록을 쓸경우 
		{
			DWORD nextBlock;
			blockSeq++;
			++i;
			if (i>=node.blocks)
			{   	
				expand_block(file->fs, file->entry.inode);
				file->fs->sb.free_block_count --;
				file->fs->gd.free_blocks_count --;
				write_super_block(&file->fs->sb, file->fs->disk);
				write_group_descriptor(file->fs->disk, &file->fs->gd, file->entry.inode/file->fs->sb.inode_per_group);
				get_inode(file->fs, file->entry.inode, &node);
				dataBlocks =get_data_block_at_inode(file->fs, &node);
				nextBlock =dataBlocks[i];
			}
			else 
			nextBlock = dataBlocks[i];


			currentBlock = nextBlock;
		}
		sectorNumber = (currentOffset / (MAX_SECTOR_SIZE)) % (MAX_BLOCK_SIZE / MAX_SECTOR_SIZE);
		sectorOffset = currentOffset % MAX_SECTOR_SIZE;

		copyLength = MIN(MAX_SECTOR_SIZE - sectorOffset, readEnd - currentOffset);
		if (copyLength != MAX_SECTOR_SIZE)
		{     
			if (data_read(file->fs, file->location.group, currentBlock, sector))
				break;
		}

		memcpy(&sector[sectorOffset],
			buffer,
			copyLength);
		if (data_write(file->fs, file->location.group, currentBlock, sector))
			break;

		buffer += copyLength;
		currentOffset += copyLength;
		get_inode(   file->fs  ,file->entry.inode,&node);
	
	}

	node.size = MAX(currentOffset, node.size);
	set_inode_onto_inode_table(file->fs, file->entry.inode, &node);
  return currentOffset - offset ; // 얼마나 읽었는지 알려준다 
}
int ext2_read(EXT2_NODE * file, unsigned long offset , unsigned long length , const char * buffer)
{   
  int readEnd ;
  int currentOffset ,currentBlock , blockSeq,copyLength ;
  int fileSize ,i=0  ;
  INODE * inodeBuffer ;
  char sector[MAX_SECTOR_SIZE];
  ZeroMemory(sector,sizeof(sector));
  inodeBuffer = (INODE *)sector ;
  int * dataBlocks;
   get_inode(file->fs,file->entry.inode,inodeBuffer);
   if(inodeBuffer->blocks==0)   // 파일에 해당되는 블록이 아예없을경우 
   {
     return EXT2_ERROR ;
   }
  dataBlocks =get_data_block_at_inode(file->fs,inodeBuffer); 
  currentOffset = offset ;
  fileSize =inodeBuffer->blocks;
   readEnd =  MIN(offset +length , fileSize *MAX_BLOCK_SIZE);
  int blockOffset = currentOffset % MAX_BLOCK_SIZE ;   // 해당 블록 내에서 몇번째 섹터인지 
   while((offset/MAX_BLOCK_SIZE) > i )
   {
       dataBlocks[++i];
   }        // 해당 블럭을 찾는 일 
    currentBlock = dataBlocks[i]; // 몇번째 블록 부터 읽을지 정한다 
	ZeroMemory(sector,sizeof(sector));
   while(currentOffset<readEnd)  // 어디까지 읽을지 정하는 것 
   {     
         data_read(file->fs ,file->location.group ,currentBlock,sector);  // 일단 해당 그룹의 블록을 읽어온다 
		 
        copyLength =MIN(MAX_BLOCK_SIZE-blockOffset , readEnd - currentOffset );
		 
      memcpy(buffer,&sector[blockOffset],copyLength);
	 
        buffer += copyLength ; // 읽은만큼 더해준다 
        currentOffset += copyLength ;   
   }
  return currentOffset - offset ; // 얼마나 읽었는지 알려준다 

}

UINT32 get_free_inode_number(EXT2_FILESYSTEM* fs, int group);

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
/*int write_super_block(EXT2_SUPER_BLOCK *sb, DISK_OPERATIONS *disk)
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
}*/
int write_super_block(EXT2_SUPER_BLOCK *sb, DISK_OPERATIONS *disk)
{
	QWORD sector_num_per_group = (disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;
	char sector[MAX_SECTOR_SIZE];
	const int BOOT_SECTOR_BASE = 1;
	
	// 디스크에 write
	for (int gi = 0; gi < NUMBER_OF_GROUPS; gi++)
	{
		sb->block_group_number = gi; // super block의 block group number가 달라짐
		//sb->first_data_block_each_group = gi*sector_num_per_group + 17;

		ZeroMemory(sector, sizeof(sector));
		memcpy(sector, sb, sizeof(EXT2_SUPER_BLOCK));

		// 해당 순서의 block group위치에 super block 써줌
		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE, sector);
	}

	return EXT2_SUCCESS;
}

// Group Descriptor
/*int write_group_descriptor(DISK_OPERATIONS* disk, EXT2_GROUP_DESCRIPTOR* gd, int groupNum)
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
}*/
int write_group_descriptor(DISK_OPERATIONS* disk, EXT2_GROUP_DESCRIPTOR* gd, int groupNum)
{
	char sector[MAX_SECTOR_SIZE];
	const int BOOT_SECTOR_BASE = 1;
	QWORD sector_num_per_group = (disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;

	// 기존의 group descriptor table 읽어와서
	disk->read_sector(disk, BOOT_SECTOR_BASE+1, sector);
	EXT2_GROUP_DESCRIPTOR* gd2;
	gd2 = (EXT2_GROUP_DESCRIPTOR*) sector;

	// 전달받은 group descriptor부분만 수정
	memcpy(gd2+groupNum, gd, sizeof(EXT2_GROUP_DESCRIPTOR));

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
   /* EXT2_NODE dotNode, dotdotNode ;
	EXT2_DIR_ENTRY * entry ;
	BYTE sector[MAX_SECTOR_SIZE];
	BYTE sector2[MAX_SECTOR_SIZE];
	INODE *id;
	INODE *id2;
	EXT2_GROUP_DESCRIPTOR * gd;
	SECTOR rootsector =0;
	EXT2_SUPER_BLOCK * sb2;
	ZeroMemory(sector,MAX_SECTOR_SIZE);
	entry= (EXT2_DIR_ENTRY *)sector;
	memcpy(entry->name,VOLUME_LABLE,13);
	entry->name_len=sizeof(VOLUME_LABLE);
	entry->inode=2;
    ZeroMemory(&dotNode, sizeof(EXT2_NODE));
	memset(dotNode.entry.name,0x20,11);
    dotNode.entry.name[0]=".";
    dotNode.entry.inode=2;
	insert_entry(2,dotNode,0x4000);
	 ZeroMemory(&dotNode, sizeof(EXT2_NODE));
	memset(dotNode.entry.name,0x20,11);
    dotdotNode.entry.name[0]=".";
	dotdotNode.entry.name[1]=".";
    dotdotNode.entry.inode=2;
	insert_entry(2,dotdotNode,0x4000);
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
	
	


	return EXT2_SUCCESS ;*/
	BYTE   sector[MAX_SECTOR_SIZE];
	SECTOR   rootSector = 0;
	EXT2_DIR_ENTRY * entry;
	EXT2_GROUP_DESCRIPTOR * gd;
	EXT2_SUPER_BLOCK * sb_read;
	QWORD sector_num_per_group = (disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;
	INODE* ip;
	const int BOOT_SECTOR_BASE = 1;
	int gi;

	ZeroMemory(sector, MAX_SECTOR_SIZE);
	entry = (EXT2_DIR_ENTRY*)sector;

	memcpy(entry->name, VOLUME_LABLE, 11);
	entry->name_len = strlen(VOLUME_LABLE);
	entry->inode = 2;
	entry++;
	entry->name[0] = DIR_ENTRY_NO_MORE;
	rootSector = 1 + sb->first_data_block_each_group;
	disk->write_sector(disk, rootSector, sector);

	sb_read = (EXT2_SUPER_BLOCK *)sector;
	for (gi = 0; gi < NUMBER_OF_GROUPS; gi++)
	{
		disk->read_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE, sector);
		sb_read->free_block_count--;

		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE, sector);
	}
	sb->free_block_count--;

	gd = (EXT2_GROUP_DESCRIPTOR *)sector;
	disk->read_sector(disk, BOOT_SECTOR_BASE + 1, sector);


	gd->free_blocks_count--;
	gd->directories_count = 1;

	for (gi = 0; gi < NUMBER_OF_GROUPS; gi++)
		disk->write_sector(disk, sector_num_per_group * gi + BOOT_SECTOR_BASE + 1, sector);

	disk->read_sector(disk, BOOT_SECTOR_BASE + 2, sector);
	sector[2] |= 0x02;
	disk->write_sector(disk, BOOT_SECTOR_BASE + 2, sector);

	ZeroMemory(sector, MAX_SECTOR_SIZE);
	ip = (INODE *)sector;
	ip++;
	ip->mode = 0x1FF | 0x4000;
	ip->size = 0;
	ip->blocks = 1;
	ip->block[0] = sb->first_data_block_each_group;
	disk->write_sector(disk, BOOT_SECTOR_BASE + 4, sector);
	disk->read_sector(disk, 5, sector);

	return EXT2_SUCCESS;
}

// inode 변경사항이 있을 때 meta data수정
void process_meta_data_for_inode_used(EXT2_NODE * retEntry, UINT32 inode_num, int fileType)
{
}

/*int insert_entry(UINT32 inode_num, EXT2_NODE * retEntry)
{
	BYTE entryName[2] = {0, };
    
	EXT2_NODE * entry;
	char sector[MAX_SECTOR_SIZE];
	char sector2[MAX_SECTOR_SIZE];
	EXT2_DIR_ENTRY* ent = (EXT2_DIR_ENTRY*) sector;

	entry = (EXT2_NODE*) sector2;

	printf("retEntry->entry.name : %s\n", retEntry->entry.name);


	entryName[0] = DIR_ENTRY_FREE;
	printf("before entry \n");
	// free 디렉터리 엔트리를 찾은 경우
	if(lookup_entry(retEntry->fs, inode_num, entryName, entry) == EXT2_SUCCESS)
	{
		printf("lookup_entry에서 free디렉터리 엔트리 찾음\n");
		data_read(entry->fs, entry->location.group, entry->location.block, sector );
		ent[entry->location.offset] = retEntry->entry;
		data_write(entry->fs, entry->location.group, entry->location.block, sector);
		retEntry->location = entry->location;
	}
	// free 디렉터리 엔트리를 찾지 못한 경우
	else
	{
		printf("lookup_entry에서 free디렉터리 엔트리 못찾아서 no_more검색\n");
		entryName[0] = DIR_ENTRY_NO_MORE;
	
		if(lookup_entry(retEntry->fs, 2, entryName, entry) == EXT2_SUCCESS)
		{
			printf("success 리턴1\n");
			printf("entry->location.group : %d\n", entry->location.group);
			printf("entry->location.block : %d\n", entry->location.block);
			// DIR_ENTRY_NO_MORE위치에 새로운 entry 추가
			
			data_read(entry->fs, entry->location.group, entry->location.block, sector);
			printf("새로운 엔트리 추가 끝1\n");
			ent[entry->location.offset] = retEntry->entry;
			printf("새로운 엔트리 추가 끝2\n");
			data_write(entry->fs, entry->location.group, entry->location.block, sector);
			printf("새로운 엔트리 추가 끝3\n");
			retEntry->location = entry->location;

			printf("새로운 엔트리 추가 끝4\n");
			entry->location.offset++;

			if(entry->location.offset == (MAX_BLOCK_SIZE/sizeof(EXT2_DIR_ENTRY)))
			{
				// 루트 디렉터리의 경우에는 다음 블록으로 넘어가지 않음
				if(inode_num != 2)
				{
					int num = expand_block(retEntry->fs, inode_num);
					entry->location.block = num;
					entry->location.offset=0;

					data_read(entry->fs, entry->location.group, entry->location.block, sector );
					ent[entry->location.offset].name[0] =DIR_ENTRY_NO_MORE;
					data_write(entry->fs, entry->location.group, entry->location.block, sector);

				}
			}
		}

		

		
	}
        
	return EXT2_SUCCESS;
}*/

int insert_entry(UINT32 inode_num, EXT2_NODE * retEntry)
{
	BYTE entryName[2] = {0, };
    
	EXT2_NODE entry = {0, };
	char sector[MAX_SECTOR_SIZE];
	char sector2[MAX_SECTOR_SIZE];
	EXT2_DIR_ENTRY* ent = (EXT2_DIR_ENTRY*) sector;

	entryName[0] = DIR_ENTRY_FREE;

	// free 디렉터리 엔트리를 찾은 경우
	if(lookup_entry(retEntry->fs, inode_num, entryName, &entry) == EXT2_SUCCESS)
	{
		data_read(entry.fs, entry.location.group, entry.location.block, sector );
		ent[entry.location.offset] = retEntry->entry;
		data_write(entry.fs, entry.location.group, entry.location.block, sector);
		retEntry->location = entry.location;
	}
	// free 디렉터리 엔트리를 찾지 못한 경우
	else
	{
		entryName[0] = DIR_ENTRY_NO_MORE;
	
		if(lookup_entry(retEntry->fs, inode_num, entryName, &entry) == EXT2_SUCCESS)
		{

			// DIR_ENTRY_NO_MORE위치에 새로운 entry 추가
			data_read(entry.fs, entry.location.group, entry.location.block, sector);
			memcpy(&ent[entry.location.offset], &retEntry->entry, sizeof(EXT2_DIR_ENTRY));
			
			data_write(entry.fs, entry.location.group, entry.location.block, sector);
		
			retEntry->location = entry.location;

			entry.location.offset++;
			

			if(entry.location.offset == (MAX_BLOCK_SIZE/sizeof(EXT2_DIR_ENTRY)))
			{
				// 루트 디렉터리의 경우에는 다음 블록으로 넘어가지 않음
				if(inode_num != 2)
				{
					int num = expand_block(retEntry->fs, inode_num);
					retEntry->fs->sb.free_block_count --;
					retEntry->fs->gd.free_blocks_count --;
					write_super_block(&retEntry->fs->sb, retEntry->fs->disk);
					write_group_descriptor(retEntry->fs->disk, &retEntry->fs->gd, inode_num/retEntry->fs->sb.inode_per_group);
					entry.location.block = num;
					entry.location.offset=0;

					data_read(entry.fs, entry.location.group, entry.location.block, sector );
					ent[entry.location.offset].name[0] =DIR_ENTRY_NO_MORE;
					data_write(entry.fs, entry.location.group, entry.location.block, sector);

				}
			}
			else
			{
				data_read(entry.fs, entry.location.group, entry.location.block, sector );
				ent[entry.location.offset].name[0] =DIR_ENTRY_NO_MORE;
				data_write(entry.fs, entry.location.group, entry.location.block, sector);

			}
			
		}

		

		
	}
        
	return EXT2_SUCCESS;
}

UINT32 get_available_data_block(EXT2_FILESYSTEM * fs, UINT32 inode_num)
{       

	BYTE sector[MAX_SECTOR_SIZE];
    UINT32 inode_group = (inode_num-1)/fs->sb.inode_per_group ;

	int used_blocks =0;  

		fs->disk->read_sector(fs->disk, 1+inode_group*fs->sb.block_per_group+2, sector);
	 for(int i = 0; i < MAX_SECTOR_SIZE; i++)
		{
			for(int j = 0; j < 8; j++)
			{
				
				if(!((unsigned char)sector[i] >> j)^0)
				{
					break;
				}
				used_blocks ++;			
			}
		}




       return used_blocks;

}
 
// data block
void process_meta_data_for_block_used(EXT2_FILESYSTEM * fs, UINT32 inode_num)
{
}

// 파일 크기가 커질때 block 더 할당
UINT32 expand_block(EXT2_FILESYSTEM * fs, UINT32 inode_num)
{  
	INODE inodeBuffer = {0, } ;
	get_inode(fs,inode_num,&inodeBuffer);
	int blocks= inodeBuffer.blocks; 
  	int used_blocks=0;
	int groupNum = (inode_num-1)/fs->sb.inode_per_group;
	char sector[MAX_SECTOR_SIZE];
	char sector2[MAX_SECTOR_SIZE];
	
  	fs->disk->read_sector(fs->disk, 1+groupNum*fs->sb.block_per_group+2, sector);
    if(blocks==12)
	{
      int indexBlock1= get_available_data_block(fs,inode_num);
      write_bitmap(indexBlock1,1,sector); 
	  inodeBuffer.block[12]=indexBlock1;
	  set_inode_onto_inode_table(fs,inode_num,&inodeBuffer);
      fs->disk->write_sector(fs->disk, 1+groupNum*fs->sb.block_per_group+2, sector); // 비트맵 넣어준다  
	}
	else if(blocks==268)
	{
      int indexBlock2= get_available_data_block(fs,inode_num);
      write_bitmap(indexBlock2,1,sector); 
	  inodeBuffer.block[13]=indexBlock2;
	  set_inode_onto_inode_table(fs,inode_num,&inodeBuffer);
      fs->disk->write_sector(fs->disk, 1+groupNum*fs->sb.block_per_group+2, sector); // 비트맵 넣어준다 
	}
	else if((blocks>=268)&& ((blocks-268)%256==0))
	{
      int subBlock = get_available_data_block(fs,inode_num);
	  write_bitmap(subBlock,1,sector);
	  fs->disk->write_sector(fs->disk, 1+groupNum*fs->sb.block_per_group+2, sector); // 비트맵 넣어준다 
      int *b = (int *)sector2;
	  data_read(fs,groupNum,inodeBuffer.block[13]   ,sector2);
	  b[(blocks-268)/256]=subBlock ;
      data_write(fs,groupNum, inodeBuffer.block[13] ,sector2);
      // set_inode_onto_inode_table(fs,inode_num,&inodeBuffer);
	}
	int f=0;
	for(int i = 0; i < MAX_SECTOR_SIZE; i++)
	{
	
		for(int j=7; j>=0; --j)
		{
			printf("%d", (sector[i]>>j)&1);
		}
		for(int j = 0; j < 8; j++)
		{
			
			if(!(((unsigned char)sector[i])&((0x01) << j)))
			{
				f = 1; 
				break;
				
			}
			used_blocks ++;			
			printf("the blocks %d\n",used_blocks);
		}
		if(f==1) break;
		printf("%d\n", i);
	
	}
	
     get_inode(fs,inode_num,&inodeBuffer);
	
    if(blocks<12)
	{
       	inodeBuffer.block[blocks++]= used_blocks ;
		
	}
	else if(blocks < 12+256)
	{
	 
    	int blocknumber = inodeBuffer.block[12];
	  	printf("%d \n",used_blocks);
       data_read(fs,groupNum,blocknumber,sector2);
		int *a = (int *)sector2 ;
		a[blocks-12]=used_blocks ;
		 data_write(fs,groupNum,blocknumber,sector2);   
		blocks ++ ;
	}
	else if(blocks< 12+256+256*256)
	{
		int blocknumber1 = inodeBuffer.block[13];
		data_read(fs,groupNum,blocknumber1,sector2);
		int * a = (int *)sector2 ;
		int blocknumber2 = a[(blocks-268)/256];
		data_read(fs, groupNum, blocknumber2, sector2);
		a[(blocks-268)%256] = used_blocks;
		data_write(fs, groupNum, blocknumber2, sector2);
		blocks++;
		

	}
	
	inodeBuffer.blocks = blocks;
	fs->disk->read_sector(fs->disk, 1+groupNum*fs->sb.block_per_group+2, sector);   // 블록 비트맵 수정 
	write_bitmap( used_blocks,1,sector);
   	fs->disk->write_sector(fs->disk, 1+groupNum*fs->sb.block_per_group+2, sector);   // 블록 비트맵 수정 
    set_inode_onto_inode_table(fs,inode_num,&inodeBuffer);
   return EXT2_SUCCESS ;
        

   // inode쓰기
   // bitmap  



}

// 
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
	{
		return EXT2_ERROR;
	} 
	  
    blockLocation = 1+group* sector_num_per_group  + block;

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
	  
    blockLocation = 1+group* sector_num_per_group + block;
    
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
	INT32 length = strlen(name);
	if(length > 255)
	{
		return EXT2_ERROR;
	}
	for(int i = 0; i < length; i++)
	{
		if(name[i] == '\0' || name[i] == '/')
		{
			return EXT2_ERROR;
		}
	}

	upper_string( name, length );

}

int lookup_entry(EXT2_FILESYSTEM* fs, const int inode, const char* name, EXT2_NODE* retEntry)
{
	INODE* inodeStruct;
	char sector[MAX_SECTOR_SIZE];
	inodeStruct = (INODE*) sector;

	if(get_inode(fs, inode, inodeStruct))
	{
		return EXT2_ERROR;
	}
	
	if(inode == 2)
	{
		return find_entry_on_root(fs, *inodeStruct, name, retEntry);
	}
	else
	{      
		return find_entry_on_data(fs, *inodeStruct, name, retEntry, inode);
	}
	

}

// begin에서 last까지 formattedName을 가진 engry를 sector에서 찾아서 인덱스를 number에 저장
int find_entry_at_sector(const BYTE* sector, const BYTE* formattedName, UINT32 begin, UINT32 last, UINT32* number)
{    
	EXT2_DIR_ENTRY* entry;
	entry = (EXT2_DIR_ENTRY*) sector;
	int i;
  
	for(i = begin; i <= last; i++)
	{
		// formattedName이 null
		if(formattedName == NULL)
		{
			if(entry[i].name[0] != DIR_ENTRY_FREE && entry[i].name[0] != DIR_ENTRY_NO_MORE && entry[i].name[0] != '.')
			{
				*number = i;
				return EXT2_SUCCESS;
			}
			
		}
		// formattedName에 값이 있는경우
		else
		{
			if((formattedName[0] == DIR_ENTRY_NO_MORE || formattedName[0] == DIR_ENTRY_FREE) 
			&& (formattedName[0] == entry[i].name[0]))
			{
				*number = i;
				return EXT2_SUCCESS;
			}
			
			if(memcmp(entry[i].name, formattedName, MAX_ENTRY_NAME_LENGTH) == 0)
			{
				*number = i;
				return EXT2_SUCCESS;
			}
		}

		// 더이상 찾을 엔트리가 없는경우
		if(entry[i].name[0] == DIR_ENTRY_NO_MORE)
		{
			*number = i;
			return -2;
		}
		
	}

	*number = i;
	return -1;
}


int find_entry_on_root(EXT2_FILESYSTEM* fs, INODE inode, char* formattedName, EXT2_NODE* ret)
{   
	char sector[MAX_SECTOR_SIZE];
	EXT2_DIR_ENTRY* entry;
	UINT32 number;

	read_root_sector(fs, sector);
	entry = (EXT2_DIR_ENTRY*) sector;
	
	UINT32 entry_cnt_per_block = MAX_BLOCK_SIZE/sizeof(EXT2_DIR_ENTRY);
	UINT32 last = entry_cnt_per_block - 1;
	 
	UINT32 result = find_entry_at_sector(sector, formattedName, 0, last, &number);
         
	if(result == -1 || result == -2)
	{
		return EXT2_ERROR;
	}
	else
	{   
		memcpy(&ret->entry, &entry[number], sizeof(EXT2_DIR_ENTRY));
     
		ret->fs = fs;
		ret->location.block = 17;
		ret->location.group = 0;
		ret->location.offset = number;

		return EXT2_SUCCESS;
	}


}

int find_entry_on_data(EXT2_FILESYSTEM* fs, INODE first, const BYTE* formattedName, EXT2_NODE* ret, UINT32 inode)
{   
	char sector[MAX_SECTOR_SIZE];
	EXT2_DIR_ENTRY* entry;
	UINT32 number;
	int* blockNumArr;
	int i, group;
	group = inode / fs->sb.inode_per_group;
	UINT32 entry_cnt_per_block = MAX_BLOCK_SIZE/sizeof(EXT2_DIR_ENTRY);
	UINT32 last = entry_cnt_per_block - 1;

	blockNumArr = get_data_block_at_inode(fs, &first);
	
	for(i = 0; i < sizeof(blockNumArr)/sizeof(int); i++)
	{  
		data_read(fs, group, blockNumArr[i], sector);
	   
		entry = (EXT2_DIR_ENTRY*) sector;
       
		UINT32 result = find_entry_at_sector(sector, formattedName, 0, last, &number);
      
		// 못찾은 경우
		if(result == -1)
		{
			continue;
		}
		else
		{
			if(result == -2)
			{
				return EXT2_ERROR;
			}
			else
			{
				memcpy(&ret->entry, &entry[number], sizeof(EXT2_DIR_ENTRY));
				ret->fs = fs;
				ret->location.block = blockNumArr[i];
				ret->location.group = group;
				ret->location.offset = number;

				return EXT2_SUCCESS;
			}
		
		}

		ZeroMemory(sector, sizeof(sector));
	}

}





int get_inode(EXT2_FILESYSTEM * fs, const UINT32 inode, INODE *inodeBuffer)
{   
	EXT2_GROUP_DESCRIPTOR* gd2;
	char sector[MAX_SECTOR_SIZE];
	ZeroMemory(sector,sizeof(sector));

	fs->disk->read_sector(fs->disk, 2, sector) ;  
	
    gd2 = (EXT2_GROUP_DESCRIPTOR *) sector;

	UINT32 inode_group = (inode-1)/fs->sb.inode_per_group; // 몇번째 group인지 

	if(inode_group > 0)
	{
		for  ( int i=0 ; i<inode_group; i++)
	  	{ 
			gd2++ ;
		}  
	}

	memcpy(&fs->gd, gd2, sizeof(EXT2_GROUP_DESCRIPTOR));
   
	UINT32 inode_structure_size = fs->sb.inode_structure_size; // inode 크기 
	UINT32 inode_start_block = fs->gd.start_block_of_inode_table; // inode 테이블의 시작 블록 
	UINT32 inode_offset = (inode-1) % (fs->sb.inode_per_group); // 그룹내의 inode table에서 몇번째인지 
	UINT32 inode_sector = (inode_offset*inode_structure_size)/(MAX_SECTOR_SIZE);
	
	//해당 inode 블럭 
    ZeroMemory(sector,MAX_SECTOR_SIZE);
	INODE* inode_table;
	inode_table = (INODE*)sector;

	fs->disk->read_sector(fs->disk, 1+(inode_group*fs->sb.block_per_group)+inode_start_block+inode_sector, sector);

	for(int i = 0; i < (inode_offset % 8); i++)
		inode_table++;
	
	*inodeBuffer = *inode_table;

	return EXT2_SUCCESS;
}

int read_root_sector(EXT2_FILESYSTEM* fs, BYTE* sector)
{    
	INODE inodeBuffer = {0, };
    int * root ;
	
	get_inode(fs,2,&inodeBuffer);

	root=	get_data_block_at_inode(fs,&inodeBuffer);
	
	data_read(fs, 0, root[0], sector);

	return EXT2_SUCCESS;
}

int ext2_create(EXT2_NODE* parent, char* entryName, EXT2_NODE* retEntry)
{
	//EXT2_DIR_ENTRY_LOCATION parentLocation;
	BYTE name[MAX_NAME_LENGTH] = {0, };
	int result = 0;

	strncpy(name, entryName, MAX_NAME_LENGTH);

	if(format_name(parent->fs, name))
	{
		return EXT2_ERROR;
	}

	ZeroMemory(retEntry, sizeof(EXT2_NODE));
	memcpy(retEntry->entry.name, name, MAX_ENTRY_NAME_LENGTH);

	if(lookup_entry(parent->fs, parent->entry.inode, name, retEntry) == EXT2_SUCCESS)
	{
		return EXT2_ERROR;
	}

	int group = (parent->entry.inode-1)/parent->fs->sb.inode_per_group;
	char sector[MAX_SECTOR_SIZE];

	// fs, inode할당
	retEntry->fs = parent->fs;
	retEntry->entry.inode = get_free_inode_number(parent->fs, group);
	
	// inode bitmap
	parent->fs->disk->read_sector(parent->fs->disk,1+group*parent->fs->sb.block_per_group+3, sector);
	write_bitmap_inode(retEntry->entry.inode-1, 1, sector);
	parent->fs->disk->write_sector(parent->fs->disk,1+group*parent->fs->sb.block_per_group+3, sector);

	// inode
	INODE inode = {0, };
  	inode.mode= 0x8000;
  	if(set_inode_onto_inode_table(parent->fs,retEntry->entry.inode,&inode))
  		return EXT2_ERROR;     
	
	// super block
	parent->fs->sb.free_inode_count--;

	// group descriptor
	parent->fs->gd.free_inodes_count--;

	result = insert_entry(parent->entry.inode, retEntry);

	if(result)
	{
		return EXT2_ERROR;
	}
	
	return EXT2_SUCCESS;
}

int* get_data_block_at_inode(EXT2_FILESYSTEM *fs, INODE* inode )
{    

	int* blockNumber;
	char sector1[MAX_SECTOR_SIZE];
	char sector2[MAX_SECTOR_SIZE];
	char sector3[MAX_SECTOR_SIZE];

	char sector4[MAX_SECTOR_SIZE];

	int* pointer1;
	int* pointer2;
	int * pointer3;
	int * pointer4;
	pointer1 = (int*)sector1;
	pointer2 = (int*)sector2;
	pointer3 = (int*)sector3;
 	pointer4 = (int*)sector4;
   
    	

	if(inode->blocks==0)
		return EXT2_ERROR;

	
	blockNumber = (int*)malloc(sizeof(int)*inode->blocks);
    
	if(inode->blocks > 11)
	{
		fs->disk->read_sector(fs->disk, inode->block[12], sector1);
	}
	if(inode->blocks > 267)
	{
		fs->disk->read_sector(fs->disk, inode->block[13], sector2);
	}
	if(inode->blocks > 256*256+11)
	{
		fs->disk->read_sector(fs->disk, inode->block[14], sector3);
	}

	
	for(int i = 0; i < inode->blocks; i++)
	{
		// single indirect
		if(i > 11 && i < 268)
		{
			blockNumber[i] = pointer1[i-12];
		}
		// double indirect
		else if(i > 267 && i < (256*256+12))
		{
			if(i % 256 == 12)
			{
				ZeroMemory(sector4, sizeof(sector4));
				fs->disk->read_sector(fs->disk, pointer2[(i-12)/257],sector4);
			}

			blockNumber[i] = pointer4[(i-12)%256];
		}
		// triple indirect
		else if(i > (256*256+11))
		{

		}
		// direct
		else
		{
			blockNumber[i] = inode->block[i];
		}
		
	}
		return blockNumber;


}
// 슈퍼블록하고 그룹디스크립터의 섹터들을 읽어와서 연결된 구조체에 연결한다 

int ext2_read_superblock(EXT2_FILESYSTEM* fs, EXT2_NODE* root)
{  
	
	BYTE sector[MAX_SECTOR_SIZE];
    
	ZeroMemory(sector,sizeof(sector));
	
   	if(fs==NULL || fs->disk==NULL)
   		return EXT2_ERROR;
	   
   	fs->disk->read_sector(fs->disk,1,sector);
	memcpy(&fs->sb, sector, sizeof(EXT2_SUPER_BLOCK));
	
	ZeroMemory(sector,sizeof(sector));
    fs->disk->read_sector(fs->disk,2,sector);
	memcpy(&fs->gd, sector, sizeof(EXT2_GROUP_DESCRIPTOR));
	
	ZeroMemory(sector,sizeof(sector));
	if (fs->sb.magic_signature != 0xEF53)
		return EXT2_ERROR;
	
	if(read_root_sector(fs,sector))
		return EXT2_ERROR;
	ZeroMemory(root,sizeof(EXT2_NODE));
	memcpy(&root->entry,sector,sizeof(EXT2_DIR_ENTRY));
	root->fs= fs;
	root->location.block=17;
	root->location.group=0;
	root->location.offset=0;

	return EXT2_SUCCESS ;

	/*INT result;
	BYTE sector[MAX_SECTOR_SIZE];

	if (fs == NULL || fs->disk == NULL)
	{
		WARNING("DISK OPERATIONS : %p\nEXT2_FILESYSTEM : %p\n", fs, fs->disk);
		return EXT2_ERROR;
	}

	meta_read(fs, 0, SUPER_BLOCK, sector);
	memcpy(&fs->sb, sector, sizeof(EXT2_SUPER_BLOCK));
	meta_read(fs, 0, GROUP_DES, sector);
	memcpy(&fs->gd, sector, sizeof(EXT2_GROUP_DESCRIPTOR));

	if (fs->sb.magic_signature != 0xEF53)
		return EXT2_ERROR;

	ZeroMemory(sector, sizeof(MAX_SECTOR_SIZE));
	if (read_root_sector(fs, sector))
		return EXT2_ERROR;

	ZeroMemory(root, sizeof(EXT2_NODE));
	memcpy(&root->entry, sector, sizeof(EXT2_DIR_ENTRY));
	root->fs = fs;

	return EXT2_SUCCESS;*/
	


   


      

}

UINT32 get_free_inode_number(EXT2_FILESYSTEM* fs, int group)
{
	int cnt = group*fs->sb.inode_per_group;

	char sector[MAX_SECTOR_SIZE];
	

	fs->disk->read_sector(fs->disk, 1 + fs->gd.start_block_of_inode_bitmap + (group*fs->sb.block_per_group), sector);
	for(int i = 0; i < MAX_SECTOR_SIZE; i++)
	{
		
		for(int j = 0; j < 8; j++)
		{
			cnt++;
			if(!(((unsigned char)sector[i])&((0x01) << j)))
			{
				return cnt;
			}	
				
		}
	}
	
	
}

int set_inode_onto_inode_table(EXT2_FILESYSTEM *fs, const UINT32 which_inode_num_to_write, INODE * inode_to_write)
{   
	// inode 버퍼를 가지고 와서 inode번호를 쓴다 
    if(which_inode_num_to_write<0|| which_inode_num_to_write>NUMBER_OF_INODES)
	 	return EXT2_ERROR;
	
	EXT2_GROUP_DESCRIPTOR *gd2;
	BYTE sector[MAX_SECTOR_SIZE];
	ZeroMemory(sector,MAX_SECTOR_SIZE);
	
	fs->disk->read_sector(fs->disk, 2, sector) ;  

    UINT32 inode_group = (which_inode_num_to_write-1)/fs->sb.inode_per_group ;  

	UINT32 inode_structure_size = fs->sb.inode_structure_size; // inode 크기 
	UINT32 inode_start_block = fs->gd.start_block_of_inode_table; // inode 테이블의 시작 블록 
	UINT32 inode_offset = (which_inode_num_to_write-1) % fs->sb.inode_per_group; // 그룹내의 inode table에서 몇번째인지 
	UINT32 inode_sector = (inode_offset*inode_structure_size)/(MAX_SECTOR_SIZE);
	
	//해당 inode 블럭 
    ZeroMemory(sector,MAX_SECTOR_SIZE);
	INODE* inode_table;
	inode_table = (INODE*)sector;
    
	fs->disk->read_sector(fs->disk, 1+4+(inode_group*  fs->sb.block_per_group) + inode_sector, sector);
    inode_table[inode_offset%8] = *inode_to_write ;             
    fs->disk->write_sector(fs->disk, 1+4+(inode_group* fs->sb.block_per_group)+inode_sector,sector);    

	return EXT2_SUCCESS;
}

int ext2_lookup(EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry)
{
	BYTE formattedName[MAX_NAME_LENGTH] = {0, };

	strncpy(formattedName, entryName, MAX_NAME_LENGTH);

	if(format_name(parent->fs, formattedName))
	{
		return EXT2_ERROR;
	}

	return lookup_entry(parent->fs, parent->entry.inode, formattedName, retEntry);
}

int ext2_read_dir(EXT2_NODE* dir, EXT2_NODE_ADD adder, void* list)
{    
	// 디렉토리를 읽는 함수인데 루트 디렉토리의 엔트리들을 읽는 것과 다른 디렉토리를 읽는 것과 차이가 있다
    BYTE sector[MAX_SECTOR_SIZE];
	EXT2_DIR_ENTRY_LOCATION location ;
	int entriesPerSector = (MAX_SECTOR_SIZE/sizeof(EXT2_DIR_ENTRY));
	INODE* inodeBuffer; 
	inodeBuffer = (INODE *)sector ;

	get_inode(dir->fs,dir->entry.inode,inodeBuffer);

	int  inodeGroup  = dir->entry.inode/ (dir->fs->sb.inode_per_group+1); // 몇번째 아이노드 그룹인지 

   	if(dir->entry.inode==2)// 루트 디렉토리일경우 
   	{
    	read_root_sector(dir->fs,sector);
	
		read_dir_from_sector(dir->fs,sector,adder,list);
	
   	}
   	else if (inodeBuffer->mode!=0x4000) // 디렉토리가 아닐경우 
   	{
    	return EXT2_ERROR;
   	}
   	else // 디렉토리일경우 
   	{
    	if(inodeBuffer->block[0]==0)  // 아무 것도 없을 경우 
     		return EXT2_ERROR ;
    	
		int * dataBlocks = get_data_block_at_inode(dir->fs,inodeBuffer);

    	for(int j=0; j<inodeBuffer->blocks ; j++)
		{
			data_read(dir->fs,inodeGroup,dataBlocks[j],sector);
      		read_dir_from_sector( dir->fs,sector,adder,list);
	  	}
	}
   
   return EXT2_SUCCESS ;
}

int read_dir_from_sector(EXT2_FILESYSTEM* fs, BYTE* sector, EXT2_NODE_ADD adder, void* list)
{     
	int  entriesPerSector;
    EXT2_DIR_ENTRY * dir;
	EXT2_NODE  node={0, } ;
	dir= (EXT2_DIR_ENTRY *)sector ;
	int i;
	int inodeOffset ,inodeGroup ;
	   
	entriesPerSector = ( MAX_SECTOR_SIZE/sizeof(EXT2_DIR_ENTRY));
	
    
	for( i=0; i<entriesPerSector ; i++)
	{
		if(dir->name[0]== DIR_ENTRY_FREE)
		{
			dir ++;
			continue ;
		}

		else if(dir->name[0]==DIR_ENTRY_NO_MORE)
		{
			break;
		}
		else if(!(memcmp(dir->name, "EXT2 BY NC", 10)))
		{
		}
		else 
		{
			node.fs = fs;
			node.entry=*dir;
			node.location.offset =i; 
			adder(fs,list,&node);	
		}
		dir ++;
	}
	
	return (i == entriesPerSector ? 0:1);  
}

char* my_strncpy(char* dest, const char* src, int length)
{
	while (*src && *src != 0x20 && length-- > 0)
	*dest++ = *src++;

	return dest;
}
int write_bitmap(int number, int set, BYTE * sector)// 아이노드 비트맵을 쓸때는 number에다가 1빼주자 
{

    int offset = number / 8 ;
	int offset2 = number %8 ;

 	if(set==0)
  	{  
	   sector[offset] &= (~(0x01<<(offset2)));
	   return EXT2_SUCCESS;
   	}
   	else if(set==1)
	{      
		sector[offset] |= (0x01<<(offset2));
	 	return EXT2_SUCCESS;
	}
  	else
  	{
		return EXT2_ERROR ;
  	}

}
int write_bitmap_inode(int number, int set, BYTE * sector)// 아이노드 비트맵을 쓸때는 number에다가 1빼주자 
{
	if(number >= 100)
	{
		number -= 100;
	}
    int offset = number / 8 ;
	int offset2 = number %8 ;

 	if(set==0)
  	{  
	   sector[offset] &= (~(0x01<<(offset2)));
	   return EXT2_SUCCESS;
   	}
   	else if(set==1)
	{      
		sector[offset] |= (0x01<<(offset2));
	 	return EXT2_SUCCESS;
	}
  	else
  	{
		return EXT2_ERROR ;
  	}

}


int ext2_mkdir(const EXT2_NODE* parent, const char* entryName, EXT2_NODE* retEntry)
{ 
	// parent node 를 가지고 
	EXT2_NODE  *dotNode;
	EXT2_NODE * dotdotNode ;
	int result ;
	INODE * inode;
	int inodenumber;
	EXT2_GROUP_DESCRIPTOR * gd_buffer;
	int gd_group =0;
	EXT2_GROUP_DESCRIPTOR  gd_temp ; // 몇번째 그룹디스크립터에 넣을건지 보관할곳 
	BYTE sector[MAX_SECTOR_SIZE];
	BYTE sector2[MAX_SECTOR_SIZE];
	BYTE sector3[MAX_SECTOR_SIZE]; 
	QWORD sector_num_per_group = (parent->fs->disk->numberOfSectors - 1) / NUMBER_OF_GROUPS;   // 그룹당 블록의 갯수 
 	BYTE name[MAX_ENTRY_NAME_LENGTH];
 	strncpy(name,entryName,MAX_NAME_LENGTH);
	int group ;

 	if(format_name(parent->fs,name))
 		return EXT2_ERROR;

 	ZeroMemory(retEntry,sizeof(EXT2_NODE));   // EXT2 노드 쓸준비 
 	memcpy(retEntry->entry.name,name,MAX_ENTRY_NAME_LENGTH); // 새로운 엔트리에 이름 넣어드림 

 	parent->fs->disk->read_sector(parent->fs->disk,2,sector); // 그룹 디스크립터 읽는다 
 	gd_buffer = (EXT2_GROUP_DESCRIPTOR *)sector ;
 	gd_temp = *gd_buffer ;

	for(int i=0 ; i<NUMBER_OF_GROUPS; i++)   // 빈 아이노드 찾는 과정 
	{
		if(gd_temp.free_inodes_count<gd_buffer->free_inodes_count)
		{  
			gd_group=i ;
			gd_temp= *gd_buffer; 
		}
		gd_buffer ++;
	}

	retEntry->fs=parent->fs;
	retEntry->fs->gd = gd_temp ;  // 그룹 디스크립터 채워주기 
	
	
	//아이노드 에 넣는다 
	
	inode =(INODE *)sector3;
	ZeroMemory(inode,sizeof(INODE));

	inode->mode= 0x4000;
	inode->blocks=1;

	inodenumber = get_free_inode_number(parent->fs,gd_group);  // 해당 아이노드 넘버 찾고 
	retEntry->entry.inode = inodenumber;
    insert_entry(parent->entry.inode,retEntry);  // 새로운 ext2노드 넣는거 
	inode->block[0]= get_available_data_block(parent->fs,inodenumber);

	if(set_inode_onto_inode_table(parent->fs,inodenumber,inode))
		return EXT2_ERROR;      // 아이노드 채워줌 
	
	parent->fs->sb.free_block_count --;
	parent->fs->sb.free_inode_count --; // 슈퍼블록 내용 수정 
	write_super_block(&parent->fs->sb, parent->fs->disk);


	group = inodenumber/(parent->fs->sb.inode_per_group);

	retEntry->fs->gd.free_blocks_count --;
	retEntry->fs->gd.free_inodes_count --;
	retEntry ->fs->gd.directories_count ++; // 디렉토리 늘었으니까 그룹 디스크립터 내용 수정 
	write_group_descriptor(parent->fs->disk, &parent->fs->gd, group);


	// 블록 비트맵과 아이노드 비트맵 수정 
	ZeroMemory(sector,sizeof(sector));
	parent->fs->disk->read_sector(parent->fs->disk,1+sector_num_per_group*group+2,sector);
	write_bitmap(inode->block[0],1,sector);
	parent->fs->disk->write_sector(parent->fs->disk,1+sector_num_per_group*group+2,sector);
	ZeroMemory(sector,sizeof(sector));
	parent->fs->disk->read_sector(parent->fs->disk,1+group*parent->fs->sb.block_per_group+3, sector);
	write_bitmap_inode(retEntry->entry.inode-1, 1, sector);
	parent->fs->disk->write_sector(parent->fs->disk,1+group*parent->fs->sb.block_per_group+3, sector);


	dotNode =(EXT2_NODE *)sector3;
	ZeroMemory(dotNode,sizeof(EXT2_NODE));
	dotNode->entry=retEntry->entry;
	memset(dotNode->entry.name,0x20,MAX_ENTRY_NAME_LENGTH);
	dotNode->entry.name[0]='.'; // 현재 디렉토리 
	dotNode->fs = retEntry->fs ;
	insert_entry(inodenumber,dotNode);

	dotdotNode =(EXT2_NODE *)sector3;
	ZeroMemory(dotdotNode,sizeof(EXT2_NODE));
	dotdotNode->entry=parent->entry;
	memset(dotdotNode->entry.name,0x20,MAX_ENTRY_NAME_LENGTH);
	dotdotNode->entry.name[0]='.';  // 상위 디렉토리 
	dotdotNode->entry.name[1]='.';
	dotdotNode->fs = parent->fs ;
	insert_entry(inodenumber,dotdotNode);


	printf("inodenumber : %d\n", inodenumber);

	return EXT2_SUCCESS ;

}

int ext2_rmdir(EXT2_NODE* dir)
{
	// 디렉터리인지 검사
	INODE inodeBuffer;
	get_inode(dir->fs, dir->entry.inode, &inodeBuffer);
	if(!(inodeBuffer.mode & FILE_TYPE_DIR))
	{
		return EXT2_ERROR;
	}

	/*if(has_sub_entries(dir->fs, &dir->entry))
	{
		return EXT2_ERROR;
	}*/

	//memset(dir->entry.name, 0x20, MAX_NAME_LENGTH);
	dir->entry.name[0] = DIR_ENTRY_FREE;

	char sector[MAX_SECTOR_SIZE];
	EXT2_DIR_ENTRY * ent;
	ent = (EXT2_DIR_ENTRY*) sector;

	data_read(dir->fs, dir->location.group, dir->location.block, sector );
	ent[dir->location.offset] = dir->entry;
	data_write(dir->fs, dir->location.group, dir->location.block, sector);
	
	// super block
	dir->fs->sb.free_inode_count++;
	dir->fs->sb.free_block_count++;
	write_super_block(&dir->fs->sb, dir->fs->disk);

	int group = (dir->entry.inode-1)/dir->fs->sb.inode_per_group;

	// group descriptor
	dir->fs->gd.free_inodes_count++;
	dir->fs->gd.free_blocks_count++;
	dir->fs->gd.directories_count--;
	write_group_descriptor(dir->fs->disk, &dir->fs->gd, group);

	// inode bitmap
	ZeroMemory(sector,sizeof(sector));
	dir->fs->disk->read_sector(dir->fs->disk, 1 + (dir->fs->sb.block_per_group*group) + 3, sector);
	write_bitmap_inode(dir->entry.inode-1, 0, sector);
	dir->fs->disk->write_sector(dir->fs->disk,1 + (group*dir->fs->sb.block_per_group) + 3, sector);
	printf("inode bitmap inode : %d\n", dir->entry.inode);

	// block bitmap
	ZeroMemory(sector,sizeof(sector));
	dir->fs->disk->read_sector(dir->fs->disk, 1 + (dir->fs->sb.block_per_group*group) + 2, sector);
	write_bitmap(inodeBuffer.block[0], 0, sector);
	dir->fs->disk->write_sector(dir->fs->disk,1 + (group*dir->fs->sb.block_per_group) + 2, sector);
	
}

int has_sub_entries(EXT2_FILESYSTEM*fs, const EXT2_DIR_ENTRY* entry)
{
	EXT2_NODE subEntry;


	if(!lookup_entry(fs, entry->inode, NULL, &subEntry ))
	{
		return EXT2_ERROR; // 찾음
	}



	return EXT2_SUCCESS;

}