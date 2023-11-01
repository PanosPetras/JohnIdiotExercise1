#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hp_file.h"

#define RECORDS_NUM 1000 // you can change it if you want
#define FILE_NAME "data.db"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

int main() {
  printf("%s",FILE_NAME);
  BF_Init(LRU);
  HP_CreateFile(FILE_NAME);
  int file_desc;

  HP_info* hp_info = HP_OpenFile(FILE_NAME, &file_desc);

  printf("\nfilename: %s\nnumber of records: %d\nrecords per block: %d\n\n", hp_info->name, hp_info->numberOfRecords, hp_info->recordsPerBlock);
  
  Record record;
  srand(165854134);
  int r;
  printf("Insert Entries\n");
  for (int id = 0; id < RECORDS_NUM; ++id) {
    record = randomRecord();
    HP_InsertEntry(file_desc, hp_info, record);
  }

  printf(
    "\nfilename: %s\nnumber of records: %d\nrecords per block: %d\nnumber of blocks: %d\n\n", 
    hp_info->name, 
    hp_info->numberOfRecords, 
    hp_info->recordsPerBlock,
    hp_info->lastBlockId
  );

  printf("RUN PrintAllEntries\n");
  int id = 210;
  printf("\nSearching for: %d\n",id);
  HP_GetAllEntries(file_desc,hp_info, id);

  HP_CloseFile(file_desc,hp_info);
  BF_Close();
}
