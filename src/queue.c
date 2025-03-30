#ifndef QUEUE_C
#define QUEUE_C

#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "commondefs.h"

ino_t get_inode(const char *path);

// Размер буфера чтения
#define READ_BUFFER_SIZE 100000

// Добавляет запись в список FileHead
void pushFile(const char *fullPath, uint64_t size) {
  // Берем только файлы с расширением .log
  const char *ext = strrchr(fullPath, '.');
  if (!ext || strcmp(ext, ".log") != 0) {
    return;
  }

  FILE_NODE_T *newNode = malloc(sizeof(FILE_NODE_T));
  if (!newNode) {
    printf("\n*** Ошибка выделения памяти\n");
    exit(EXIT_FAILURE);
  }

  snprintf(newNode->FileName, MAX_PATHNAME_LEN, "%s", fullPath);
  newNode->Size = size;
  newNode->Next = NULL;

  if (!FileHead) {
    FileHead = newNode;
  } else {
    FILE_NODE_T *current = FileHead;
    while (current->Next) {
      current = current->Next;
    }
    current->Next = newNode;
  }
}

// Достать запись из списка FileHead по FIFO
FILE_NODE_T *popFile() {
  if (!FileHead) {
    return NULL;
  }

  FILE_NODE_T *retVal = FileHead;
  FileHead = FileHead->Next;
  retVal->Next = NULL;
  return retVal;
}

// Составляет список файлов в папке, рекурсия
void listDirectory(const char *path) {
  DIR *dir = opendir(path);
  if (!dir) {
    printf("Ошибка чтения каталога '%s'\n", path);
    exit(EXIT_FAILURE);
  }

  struct dirent *entry;
  struct stat st;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;  // пропуск "." и ".."
    }

    char full_path[MAX_PATHNAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

    if (stat(full_path, &st) == -1) {
      printf("\n*** Ошибка получения свойств файла: %s\n", full_path);
      exit(EXIT_FAILURE);
    }

    if (S_ISREG(st.st_mode)) {
      // Это файл, добавляем:
      pushFile(full_path, st.st_size);
    } else if (S_ISDIR(st.st_mode) && Settings.Recursive) {
      // Это папка, пошли в рекурсию:
      if (st.st_ino == get_inode(path)) {
        // избегаем зацикливание на симлинках
        continue;
      }
      listDirectory(full_path);
    }
  }

  closedir(dir);
}

ino_t get_inode(const char *path) {
  struct stat st;
  if (stat(path, &st) == -1) {
    printf("\n*** Ошибка получения свойств файла: %s\n", path);
    exit(EXIT_FAILURE);
  }
  return st.st_ino;
}

// Добавляет запись в список TaskHead
void pushTask(const char *fileName, uint64_t size, uint64_t posStart,
              uint64_t posEnd) {
  TASK_NODE_T *newNode = malloc(sizeof(TASK_NODE_T));
  if (!newNode) {
    printf("\n*** Ошибка выделения памяти\n");
    exit(EXIT_FAILURE);
  }

  snprintf(newNode->FileName, MAX_PATHNAME_LEN, "%s", fileName);
  newNode->Size = size;
  newNode->PosStart = posStart;
  newNode->PosEnd = posEnd;
  newNode->Next = NULL;

  if (!TaskHead) {
    TaskHead = newNode;
  } else {
    TASK_NODE_T *current = TaskHead;
    while (current->Next) {
      current = current->Next;
    }
    current->Next = newNode;
  }
}

TASK_NODE_T *popTask() {
  if (!TaskHead) {
    return NULL;
  }

  TASK_NODE_T *retVal = TaskHead;
  TaskHead = TaskHead->Next;
  retVal->Next = NULL;
  return retVal;
}

// Создает список блоков данных для обработки
// Список файлов берет из FileHead
void createTasks() {
  while (true) {
    FILE_NODE_T *fileNode = popFile();
    if (!fileNode) {
      break;
    }

    FILE *fp = fopen(fileNode->FileName, "r");
    if (!fp) {
      printf("\n*** Ошибка открытия файла: %s\n", fileNode->FileName);
      free(fileNode);
      exit(EXIT_FAILURE);
    }

    uint64_t size = fileNode->Size;
    uint64_t restToDo = size;
    uint64_t pos = 0;

    char readBuffer[READ_BUFFER_SIZE];
    while (restToDo > 0) {
      uint64_t blockEnd = pos + Settings.DataBlockSize;
      if (blockEnd > size) {
        blockEnd = size;
      }

      if (restToDo > Settings.DataBlockSize) {
        fseek(fp, blockEnd, SEEK_SET);
        size_t bytesRead = fread(readBuffer, 1, READ_BUFFER_SIZE, fp);

        // Поиск последовательности CR LF
        for (size_t i = 0; i < bytesRead - 8; ++i) {
          if (readBuffer[i] == CR && readBuffer[i + 1] == LF &&
              readBuffer[i + 4] == COLON && readBuffer[i + 7] == POINT) {
            blockEnd += i + 2;
            break;
          }
        }
        if (blockEnd > size) {
          blockEnd = size;
        }
      } else {
        blockEnd = size;
      }

      pushTask(fileNode->FileName, size, pos, blockEnd - 1);

      pos = blockEnd;
      restToDo -= (blockEnd - pos);
      if (restToDo <= 0 || pos >= size) {
        break;
      }
    }

    fclose(fp);
    free(fileNode);
  }
}

// Добавляет запись в список TaskHead
void pushTag(const char *tagName, const char *tagNameShort) {
  TAG_NODE_T *newNode = malloc(sizeof(TAG_NODE_T));
  if (!newNode) {
    printf("\n*** Ошибка выделения памяти\n");
    exit(EXIT_FAILURE);
  }

  snprintf(newNode->Name, sizeof(newNode->Name), "%s", tagName);
  snprintf(newNode->NameShort, sizeof(newNode->NameShort), "%s", tagNameShort);
  newNode->Len = strlen(tagName);
  newNode->LenShort = strlen(tagNameShort);
  newNode->Next = NULL;

  if (!TagHead) {
    TagHead = newNode;
  } else {
    TAG_NODE_T *current = TagHead;
    while (current->Next) {
      current = current->Next;
    }
    current->Next = newNode;
  }
}

void prepareTags() {
  pushTag("Sql=", "S=");
  pushTag("Sdbl=", "Sd=");
  pushTag("Prm=", "P=");
  pushTag("Context=", "Cntxt=");
  pushTag("Txt=", "T=");
  pushTag("txt=", "T=");
  pushTag("Descr=", "Dsc=");
  pushTag("ServerList=", "ServList=");
  pushTag("ManagerList=", "ManagList=");
  pushTag("Headers=", "Heads=");
  pushTag("RetExcp=", "RExcp=");
}

#endif
