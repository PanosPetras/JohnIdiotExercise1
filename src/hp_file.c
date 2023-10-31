#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

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
  	BF_ErrorCode error = BF_CreateFile(fileName);
	if(error != BF_OK){
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

	strncpy(block0Info.name, fileName, sizeof(block0Info.name));

	int file_desc;
	BF_Block* block0;

	BF_OpenFile(fileName, file_desc);

	error = BF_AllocateBlock(file_desc, block0);
	if(error != BF_OK){
		return -1;
	}
	BF_Block_Init(&block0);

	void* records = BF_Block_GetData(block0);

	memcpy(records, &block0Info, sizeof(HP_info));

	BF_Block_SetDirty(&block0);
	BF_UnpinBlock(block0);

	BF_CloseFile(file_desc);

	return 0;
}

HP_info* HP_OpenFile(
	char *fileName, 
	int *file_desc
){
	HP_info* hpInfo;
	BF_Block* block0;

	BF_OpenFile(fileName, file_desc);

	BF_GetBlock(file_desc, 0, block0);

	hpInfo = BF_Block_GetData(block0);

	return hpInfo;
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

	if(BF_CloseFile(file_desc) != BF_OK){
		return -1;
	} else {
  		return 0;
	}
}

int unpinAllBlocksFromFile(
	int file_desc,
	int numberOfBlocks
){
	BF_ErrorCode error;
	BF_Block* block;

	for(int i = 0; i <= numberOfBlocks; i++){
		error = BF_GetBlock(file_desc, i, block);

		if(error != BF_OK){
			return -1;
		}

		BF_UnpinBlock(block);
	}

	return 0;
}

int HP_InsertEntry(
	int file_desc,
	HP_info* hp_info, 
	Record record
){
	if(hp_info->lastBlockId == 0) { 
		AddRecordToNewBlock(
			file_desc,
			hp_info,
			hp_info->lastBlockId + 1,
			record
		);
	} else {
		BF_Block* lastBlock;
		BF_GetBlock(file_desc, hp_info->lastBlockId, lastBlock);

		void* data = BF_Block_GetData(lastBlock);
		HP_block_info* lastBlockInfo = data + 504;

		int recordsInLastBlock = lastBlockInfo->numberOfRecords;

		if(recordsInLastBlock >= hp_info->recordsPerBlock){
			AddRecordToNewBlock(
				file_desc,
				hp_info,
				hp_info->lastBlockId + 1,
				record
			);
		} else {
			AddRecordToExistingBlock(
				file_desc,
				hp_info,
				lastBlock,
				record
			);
		}
	}

	return -1;
}

int AddRecordToNewBlock(
	int file_desc,
	HP_info* hp_info, 
	int newId,
	Record record
){
	BF_Block* newBlock;
	BF_AllocateBlock(file_desc, newBlock);

	void* data = BF_Block_GetData(newBlock);
	Record* records = data;
	HP_block_info* blockInfo = data + 504;

	HP_block_info newBlockInfo = {
		.blockID = newId, 
		.numberOfRecords = 1
	};

	memcpy(records, &record, sizeof(Record));
	memcpy(blockInfo, &newBlockInfo, sizeof(HP_block_info));

	BF_Block_SetDirty(newBlock);

	hp_info->lastBlockId = newId;
	hp_info->numberOfRecords += 1;
}

void AddRecordToExistingBlock(
	int file_desc,
	HP_info* hp_info, 
	BF_Block* block,
	Record record
){
	void* blockData = BF_Block_GetData(block);
	Record* records = blockData;
	HP_block_info* blockInfo = blockData + 504;

	memcpy(records + (blockInfo->numberOfRecords - 1) * hp_info->recordSize, &record, sizeof(Record));

	BF_Block_SetDirty(block);

	blockInfo->numberOfRecords += 1;
	hp_info->numberOfRecords += 1;
}

int HP_GetAllEntries(
	int file_desc,
	HP_info* hp_info, 
	int value
){    
	return -1;
}
