#ifndef COMMONDEFS_H
#define COMMONDEFS_H

#define MAX_PATHNAME_LEN 1024

#define KB 1024
#define MB (KB * KB)
#define GB (MB * KB)
#define DEFAULT_DATA_BLOCK_SIZE (50 * MB)
#define _1MB MB

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

const char CR = 0x0d;
const char LF = 0x0a;
const char TAB = 0x09;
const char D_QUOTES = 0x22;
const char S_QUOTES = 0x27;
const char COMMA = 0x2c;
const char SPACE = 0x20;
const char COLON = 0x3a;
const char POINT = 0x2e;
const char HASH = 0x23;

// Application settings
typedef struct {
  uint8_t NumThreads;
  uint64_t DataBlockSize;
  char *SearchPath;
  bool Recursive;
  bool Debug;
} SETTINGS_T;

// Linked list of Tags
typedef struct TAG_NODE {
  char Name[15];
  char NameShort[15];
  uint8_t Len;
  uint8_t LenShort;
  struct TAG_NODE *Next;
} TAG_NODE_T;

// Linked list of FileNames
typedef struct FILE_NODE {
  char FileName[MAX_PATHNAME_LEN];
  uint64_t Size;
  struct FILE_NODE *Next;
} FILE_NODE_T;

// Linked list of Tasks
typedef struct TASK_NODE {
  char FileName[MAX_PATHNAME_LEN];
  uint64_t Size;
  uint64_t PosStart;
  uint64_t PosEnd;
  struct TASK_NODE *Next;
} TASK_NODE_T;

SETTINGS_T Settings;
TASK_NODE_T *TaskHead = NULL;
FILE_NODE_T *FileHead = NULL;
TAG_NODE_T *TagHead = NULL;
uint64_t TotalBytes = 0;

#endif  // COMMONDEFS_H;
