#ifndef WORKER_C
#define WORKER_C

#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commondefs.h"
#include "queue.c"

#define MAX_BUFFER_SIZE (Settings.DataBlockSize + 1)

pthread_mutex_t mutex;

void eventNormalize(TAG_NODE_T *Tag, char *Buffer, const uint64_t BlockSize,
                    uint64_t *i) {
  uint64_t I = *i + 1;
  char Delimiter = Buffer[I + Tag->Len];

  uint64_t OldTextStart = I + Tag->Len + 1;
  uint64_t TextStart = I + Tag->Len - 1;

  // Добавляем Двойные кавычки
  Buffer[I + Tag->Len - 2] = D_QUOTES;

  // Заменяем стандартное имя тега ТЖ его коротким алиасом
  memcpy(&Buffer[I], Tag->NameShort, Tag->LenShort);

  if (Delimiter != S_QUOTES && Delimiter != D_QUOTES) {
    // Кавычки не обнаружены; ищем до запятой или CRLF
    Delimiter = COMMA;
    OldTextStart--;
  }

  uint64_t k = TextStart;
  bool isHash = false;

  for (I = OldTextStart; I < BlockSize; I++) {
    if ((Delimiter == D_QUOTES) && (Buffer[I] == D_QUOTES) &&
        (I + 1 < BlockSize) && (Buffer[I + 1] == D_QUOTES)) {
      Buffer[k++] = S_QUOTES;
      Buffer[k++] = S_QUOTES;
      I++;
      continue;
    }

    if ((Delimiter == S_QUOTES) && (Buffer[I] == S_QUOTES) &&
        (I + 1 < BlockSize) && (Buffer[I + 1] == S_QUOTES)) {
      Buffer[k++] = S_QUOTES;
      Buffer[k++] = S_QUOTES;
      I++;
      continue;
    }

    if (Buffer[I] == Delimiter) {
      break;
    }

    if (Buffer[I] == CR && Delimiter == COMMA) {
      break;
    }

    if (Buffer[I] == CR || Buffer[I] == LF || Buffer[I] == TAB) {
      Buffer[k++] = SPACE;
    } else if (Buffer[I] == D_QUOTES) {
      Buffer[k++] = S_QUOTES;
    } else if (Buffer[I] == HASH) {  // временные SQL таблицы
      if (I < BlockSize - 5 && Buffer[I + 1] == 't') {
        isHash = true;
        Buffer[k++] = Buffer[I++];
        Buffer[k++] = Buffer[I++];
        Buffer[k++] = Buffer[I++];
      }
    } else if (isHash && isdigit(Buffer[I])) {
      continue;  // удаляем числа из имени временной SQL таблицы
    } else if (isHash && !isdigit(Buffer[I])) {
      isHash = false;
      Buffer[k++] = Buffer[I];
    } else {
      Buffer[k++] = Buffer[I];
    }
  }

  // Дошли до конца
  Buffer[k++] = D_QUOTES;

  if (Buffer[I] != CR) {
    I++;
  }

  while (k < I) {
    Buffer[k++] = COMMA;
  }

  *i = k - 1;
}

void *runThread(void *ptr) {
  while (true) {
    TASK_NODE_T *Task = NULL;

    pthread_mutex_lock(&mutex);
    Task = popTask();
    if (Task != NULL) {
      TotalBytes += (Task->PosEnd - Task->PosStart + 1);
    }
    pthread_mutex_unlock(&mutex);

    if (Task == NULL) {
      break;  // задачи закончились
    }

    const uint64_t BlockSize = Task->PosEnd - Task->PosStart + 1;

    FILE *fp = fopen(Task->FileName, "rb+");
    if (fp == NULL) {
      fprintf(stderr, "\n*** Ошибка открытия файла на чтение и запись: %s>\n",
              Task->FileName);
      free(Task);
      continue;
    }

    char *Buffer = malloc(BlockSize);
    if (!Buffer) {
      fprintf(stderr, "\n*** Ошибка выделения памяти дл буфера\n");
      fclose(fp);
      free(Task);
      continue;
    }

    fseek(fp, Task->PosStart, SEEK_SET);
    fread(Buffer, 1, BlockSize, fp);

    // Process the buffer
    for (uint64_t i = 0; i < BlockSize; i++) {
      if (Buffer[i] == COMMA) {
        TAG_NODE_T *Tag = TagHead;
        while (Tag != NULL) {
          if (i + 1 + Tag->Len <= BlockSize &&
              memcmp(&Buffer[i + 1], Tag->Name, Tag->Len) == 0) {
            eventNormalize(Tag, Buffer, BlockSize, &i);
            break;
          }
          Tag = Tag->Next;
        }
      }
    }

    // записать обработанный блок
    fseek(fp, Task->PosStart, SEEK_SET);
    fwrite(Buffer, 1, BlockSize, fp);

    fclose(fp);
    free(Buffer);
    free(Task);

    // Индикатор прогресса
    fprintf(stdout, "#");
    fflush(stdout);
  }

  return ptr;
}

void startInThreads(uint8_t Num) {
  pthread_t threads[Num];
  pthread_mutex_init(&mutex, NULL);

  // Создаем потоки
  for (uint8_t i = 0; i < Num; i++) {
    if (pthread_create(&threads[i], NULL, runThread, NULL) != 0) {
      fprintf(stderr, "<Failed to create thread>\n");
      exit(EXIT_FAILURE);
    }
  }

  // Присоединяем результаты потоков
  for (uint8_t i = 0; i < Num; i++) {
    pthread_join(threads[i], NULL);
  }

  pthread_mutex_destroy(&mutex);
}

#endif
