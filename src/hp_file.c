#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define HP_ERROR -1

const int recordsPerBlock = 6;
const int recordSize = 74;

#define CALL_BF(call)       \
{                           \
	BF_ErrorCode code = call; \
	if (code != BF_OK) {         \
		BF_PrintError(code);    \
		return HP_ERROR;        \
	}                         \
}

int HP_CreateFile(
	char *fileName
){
	if(
		BF_CreateFile(fileName) != BF_OK
	){
		return -1;
	}

	HP_info block0Info = {
		.firstBlock = NULL, 
		.lastBlockId = 0, 
		.name = "", 
		.numberOfRecords = 0,
		.recordSize = 74, 
		.recordsPerBlock = 6
	};

	strncpy(block0Info.name, fileName, strlen(fileName));

	int file_desc;
	BF_Block* block0;

	BF_OpenFile(fileName, &file_desc);

	BF_Block_Init(&block0);
	if(
		BF_AllocateBlock(file_desc, block0) != BF_OK
	){
		return -1;
	}
	
	void* records = BF_Block_GetData(block0);

	memcpy(records, &block0Info, sizeof(HP_info));

	HP_info* info = (HP_info*) records;

	

	BF_Block_SetDirty(block0);
	BF_UnpinBlock(block0);
	
	BF_CloseFile(file_desc);

	return 0;
}

HP_info* HP_OpenFile(
	char *fileName, 
	int *file_desc
){
	BF_Block* block0;

	if(
		BF_OpenFile(fileName, file_desc) != BF_OK
	){
		printf("Couldn't open file");
		return NULL;
	}

	BF_Block_Init(&block0);
	BF_GetBlock(*file_desc, 0, block0);

	void* data = BF_Block_GetData(block0);    
	HP_info* hpInfo = (HP_info *) data;

	return hpInfo;
}

int unpinAllBlocksFromFile(
	int file_desc,
	int numberOfBlocks
){
	BF_ErrorCode error;
	BF_Block* block;
	BF_Block_Init(&block);

	for(int i = 0; i <= numberOfBlocks; i++){
		error = BF_GetBlock(file_desc, i, block);

		if(error != BF_OK){
			return -1;
		}

		BF_UnpinBlock(block);
	}

	return 0;
}

int HP_CloseFile(
	int file_desc,
	HP_info* hp_info
){
	if(
		unpinAllBlocksFromFile(
			file_desc,
			hp_info->lastBlockId
		)
	) return -1;

	if(
		BF_CloseFile(file_desc) != BF_OK
	){
		return -1;
	} else {
  		return 0;
	}
}

int AddRecordToNewBlock(
	int file_desc,
	HP_info* hp_info, 
	int newId,
	Record record
){
	BF_Block* newBlock;
	BF_Block_Init(&newBlock);
	BF_AllocateBlock(file_desc, newBlock);

	printf("allocated\n");

	void* data = BF_Block_GetData(newBlock);
	Record* records = data;
	HP_block_info* blockInfo = (HP_block_info*)((Record*)data + 504); //  HP_block_info* blockInfo = (HP_block_info*)((Record*)data + 512 - 8);
	printf("got data segment\n");

	HP_block_info newBlockInfo = {
		.blockID = newId, 
		.numberOfRecords = 1
	};
	printf("created block info\n");

	records[0] = record;
	*blockInfo = newBlockInfo;

	printf("copied mem\n");

	BF_Block_SetDirty(newBlock);
	BF_UnpinBlock(newBlock);

	hp_info->lastBlockId += 1;
	hp_info->numberOfRecords += 1;
	printf("inserted to new\n");
}

void AddRecordToExistingBlock(
	int file_desc,
	HP_info* hp_info, 
	BF_Block* block,
	Record record
){
	void* blockData = BF_Block_GetData(block);
	Record* records = blockData;
	HP_block_info* blockInfo = (HP_block_info*)((Record*)blockData + 504);

	records[blockInfo->numberOfRecords - 1] = record;

	BF_Block_SetDirty(block);

	blockInfo->numberOfRecords += 1;
	hp_info->numberOfRecords += 1;
	printf("inserted to existing\n");
}


int HP_InsertEntry(
	int file_desc,
	HP_info* hp_info, 
	Record record
){
	if(hp_info->lastBlockId == 0) { 
		printf("insert to new, lastid = 0\n");
		AddRecordToNewBlock(
			file_desc,
			hp_info,
			hp_info->lastBlockId + 1,
			record
		);
	} else {
		printf("read block\n");
		BF_Block* lastBlock;
		BF_Block_Init(&lastBlock);
		BF_GetBlock(file_desc, hp_info->lastBlockId, lastBlock);
		printf("readed block\n");

		void* data = BF_Block_GetData(lastBlock);
		HP_block_info* lastBlockInfo = (HP_block_info*)((Record*)data + 504);
		printf("readed block address\n");

		int recordsInLastBlock = lastBlockInfo->numberOfRecords;
		printf("readed block records\n");

		if(recordsInLastBlock >= hp_info->recordsPerBlock){
			printf("insert to new\n");
			AddRecordToNewBlock(
				file_desc,
				hp_info,
				hp_info->lastBlockId + 1,
				record
			);
		} else {
			printf("insert to existing\n");
			AddRecordToExistingBlock(
				file_desc,
				hp_info,
				lastBlock,
				record
			);
		}
		
		BF_UnpinBlock(lastBlock);
	}

	return -1;
}



int HP_GetAllEntries(
	int file_desc,
	HP_info* hp_info, 
	int value
){
	BF_Block* block;
	BF_ErrorCode error;

	for(int i = 1; i <= hp_info->lastBlockId; i++){
		error = BF_GetBlock(file_desc, i, block);

		if(error != BF_OK){
			return -1;
		}

		void* data = BF_Block_GetData(block);
		Record* records = data;
		HP_block_info* blockInfo = (HP_block_info*)((Record*)data + 500);

		for(int j = 0; j < blockInfo->numberOfRecords; j++){
			if(records[j].id == value){
				printf("%d, %s, %s, %s\n", records[j].id, records[j].name, records[j].surname, records[j].city);
			}
		}

		BF_UnpinBlock(block);
	}

	return 0;
}
