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

int initializeFirstBlockOfFile(
	char *fileName,
	int file_desc,
	HP_info fileInfo
){
	BF_Block* block0;

	//Initialize the first block of the file
	BF_Block_Init(&block0);
	if(
		BF_AllocateBlock(file_desc, block0) != BF_OK
	) return -1;
	
	void* info = BF_Block_GetData(block0);

	memcpy(info, &fileInfo, sizeof(HP_info));

	//Free up the used block
	BF_Block_SetDirty(block0);
	BF_UnpinBlock(block0);
	BF_Block_Destroy(&block0);

	return 0;
}

int HP_CreateFile(
	char *fileName
){
	if(
		BF_CreateFile(fileName) != BF_OK
	) return -1;

	int file_desc;

	BF_OpenFile(fileName, &file_desc);

	//Initialize file info
	HP_info fileInfo = {
		.firstBlock = NULL, 
		.lastBlockId = 0, 
		.name = "", 
		.numberOfRecords = 0,
		.recordSize = 74, 
		.recordsPerBlock = 6
	};
	strncpy(fileInfo.name, fileName, strlen(fileName));

	if(
		initializeFirstBlockOfFile(fileName, file_desc, fileInfo) != 0
	) return -1;
	
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
	) return NULL;

	//Get the first block of the file
	BF_Block_Init(&block0);
	BF_GetBlock(*file_desc, 0, block0);

	//Get the metadata of the file
	void* data = BF_Block_GetData(block0);    
	HP_info* hpInfo = (HP_info *) data;

	BF_UnpinBlock(block0);
	BF_Block_Destroy(&block0);

	return hpInfo;
}

int unpinAllBlocksFromFile(
	int file_desc,
	int numberOfBlocks
){
	BF_Block* block;
	BF_Block_Init(&block);

	for(int i = 0; i <= numberOfBlocks; i++){
		if(
			BF_GetBlock(file_desc, i, block) != BF_OK
		) return -1;

		if(
			BF_UnpinBlock(block) != BF_OK
		) return -1;

		BF_Block_Destroy(&block);
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
	) return -1; 
	else return 0;
}

int AddRecordToNewBlock(
	int file_desc,
	HP_info* hp_info, 
	int newId,
	Record record
){
	//Initialize new block
	BF_Block* newBlock;
	BF_Block_Init(&newBlock);
	if(
		BF_AllocateBlock(file_desc, newBlock) != BF_OK
	) return -1 ;

	//Get data segment of block
	void* data = BF_Block_GetData(newBlock);
	Record* records = data;
	HP_block_info* newBlockInfoAddr = data + 504;

	//Create block metadata
	HP_block_info newBlockInfo = {
		.blockID = newId,
		.numberOfRecords = 1
	};

	//Save block data
	records[0] = record;
	newBlockInfoAddr[0] = newBlockInfo;

	//Free new block
	BF_Block_SetDirty(newBlock);
	BF_UnpinBlock(newBlock);
	BF_Block_Destroy(&newBlock);
	
	//Update file info
	hp_info->numberOfRecords += 1;

	//Update block info
	hp_info->lastBlockId = newId;

	return 0;
}

int AddRecordToLastBlock(
	int file_desc,
	HP_info* hp_info, 
	Record record
){
	//Get the last block
	BF_Block* lastBlock;
	BF_Block_Init(&lastBlock);
	if(
		BF_GetBlock(file_desc, hp_info->lastBlockId, lastBlock)
	) return -1;

	//Get data segment of block
	void* data = BF_Block_GetData(lastBlock);
	Record* records = data;
	HP_block_info* lastBlockInfo = data + 504;

	//Save block data
	records[lastBlockInfo->numberOfRecords] = record;

	//Update block
	lastBlockInfo->numberOfRecords += 1;

	//Free block
	BF_Block_SetDirty(lastBlock);
	BF_UnpinBlock(lastBlock);
	BF_Block_Destroy(&lastBlock);
	
	//Update file info
	hp_info->numberOfRecords += 1;

	return 0;
}

int RecordsInLastBlockOfFile(
	int file_desc,
	HP_info* hp_info
){
	//Get the last block
	BF_Block* lastBlock;
	BF_Block_Init(&lastBlock);
	BF_GetBlock(file_desc, hp_info->lastBlockId, lastBlock);

	//Get data segment of block
	void* data = BF_Block_GetData(lastBlock);
	HP_block_info* lastBlockInfo = data + 504;

	int numberOfRecords = lastBlockInfo->numberOfRecords;

	//Free block
	BF_UnpinBlock(lastBlock);
	BF_Block_Destroy(&lastBlock);

	return numberOfRecords;
}

int HP_InsertEntry(
	int file_desc,
	HP_info* hp_info, 
	Record record
){
	if(hp_info->lastBlockId == 0) { 
		//If file has only one block, the metadata block, create new block
		 return AddRecordToNewBlock(
			file_desc,
			hp_info,
			hp_info->lastBlockId + 1,
			record
		);
	} else {
		int recordsInLastBlock = RecordsInLastBlockOfFile(file_desc, hp_info);

		if(recordsInLastBlock >= hp_info->recordsPerBlock){
			//If the last block is full, create a new one
			return AddRecordToNewBlock(
				file_desc,
				hp_info,
				hp_info->lastBlockId + 1,
				record
			);
		} else {
			//If the last block is not full, add the record to it
			return AddRecordToLastBlock(
				file_desc,
				hp_info,
				record
			);
		}
	}
}

int HP_GetAllEntries(
	int file_desc,
	HP_info* hp_info, 
	int value
){
	//Initialize our variables
	int lastBlockId = 0;
	BF_Block* block;
	void* data;
	Record* records;
	HP_block_info* blockInfo;
	BF_Block_Init(&block);

	//Cycle through all the blocks of the file
	for(int i = 1; i <= hp_info->lastBlockId; i++){
		if(
			BF_GetBlock(file_desc, i, block) != BF_OK
		) return -1;

		//Get the data segments of the block
		data = BF_Block_GetData(block);
		records = data;
		blockInfo = (HP_block_info*)(data + 504);

		//Cycle through all the records of the block
		for (int j = 0; j < blockInfo->numberOfRecords; j++){
			if(
				records[j].id == value
			) {
				lastBlockId = i;
				printRecord(records[j]);
			}
		}

		BF_Block_Destroy(&block);
	}

	return lastBlockId;
}
