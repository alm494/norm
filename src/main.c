#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>

#include "commondefs.h"
#include "queue.c"
#include "worker.c"

// error handling function
void error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

// forward declarations
void showHelp();
void parseArgs(int argc, char **argv);
void formatHumanReadable(double value, char *buffer, size_t bufferSize);

int main(int argc, char **argv) {
  // замер времени выполнения, старт
  struct timespec begin, end;
  timespec_get(&begin, TIME_UTC);

  prepareTags();
  parseArgs(argc, argv);
  printf("\nnorm utility (c) 2021 Алмаз Шарипов\n\n");
  printf("Число потоков:         %d\n", Settings.NumThreads);
  printf("Путь поиска:           %s\n", Settings.SearchPath);
  printf("Рекурсивный поиск:     %s\n", Settings.Recursive ? "Да" : "Нет");

  listDirectory(Settings.SearchPath);
  createTasks();
  startInThreads(Settings.NumThreads);

  // замер времени выполнения, стоп
  timespec_get(&end, TIME_UTC);

  double totalTime =
      (end.tv_sec - begin.tv_sec) + (end.tv_nsec - begin.tv_nsec) / 1e9;

  char bytesBuffer[50];
  char speedBuffer[50];

  formatHumanReadable(TotalBytes, bytesBuffer, sizeof(bytesBuffer));
  formatHumanReadable(TotalBytes / totalTime, speedBuffer, sizeof(speedBuffer));

  printf("\n");
  printf("Время выполнения:      %0.3f сек\n", totalTime);
  printf("Обработано данных:     %s\n", bytesBuffer);
  printf("Скорость обработки:    %s/сек\n\n", speedBuffer);

  return 0;
}

void parseArgs(int argc, char **argv) {
  if (argc < 2) {
    showHelp();
    exit(EXIT_SUCCESS);
  }

  Settings = (SETTINGS_T){.NumThreads = get_nprocs(),
                          .Recursive = false,
                          .Debug = false,
                          .DataBlockSize = DEFAULT_DATA_BLOCK_SIZE,
                          .SearchPath = ""};

  for (int i = 1; i < argc;) {
    char *arg = argv[i];

    if (strcmp(arg, "-i") == 0) {
      if (++i >= argc) error("Отсутствует параметр после -i");
      Settings.SearchPath = argv[i++];
    } else if (strcmp(arg, "-r") == 0) {
      Settings.Recursive = true;
      i++;
    } else if (strcmp(arg, "-j") == 0) {
      if (++i >= argc) error("Отсутствует параметр после -j");
      char *endptr;
      Settings.NumThreads = strtol(argv[i], &endptr, 10);
      if (*endptr != '\0')
        error("Ошибка в указании количества потоков: %s", argv[i]);
      i++;
    } else if (strcmp(arg, "-b") == 0) {
      if (++i >= argc) error("Отсутствует параметр после -b");
      char *endptr;
      Settings.DataBlockSize = strtol(argv[i], &endptr, 10) * MB;
      if (*endptr != '\0')
        error("Ошибка в указании размера блока: %s", argv[i]);
      i++;
    } else if (strcmp(arg, "-d") == 0) {
      Settings.Debug = true;
      i++;
    } else {
      error("Неизвестная опция: %s", arg);
    }
  }

  if (Settings.SearchPath[0] == '\0')
    error("Обязательный параметр -i не указан");
}

void showHelp() {
  printf("\nnorm utility (c) 2021 Алмаз Шарипов\n\n");
  printf(
      "Использование: norm -i <строка> [-r] [-j <число>] [-b <число>]\n\n");
  printf("Параметры:\n");
  printf(
      "   -i 'rphost_8508'    Искать файлы ТЖ в указанной папке. Обязательный "
      "параметр.\n");
  printf("   -r                  Искать файлы ТЖ рекурсивно.\n");
  printf(
      "   -j 4                Обработка в 4 потока, по умолчанию используются "
      "все\n");
  printf("                       доступные ядра.\n");
  printf(
      "   -b 200              Размер порции данных для обработки в Mб, по "
      "умолчанию 50\n\n");
  printf("Описание:\n");
  printf(
      "   Утилита предназначена для перезаписи файлов Технологического Журнала "
      "1С в\n");
  printf(
      "   однострочный формат. Для этого удаляются все лишние возвраты "
      "каретки, перевода\n");
  printf("   строк, символы табуляции.\n");
  printf(
      "   Кроме того, некоторые поля журнала приводятся в унифицированный "
      "формат,\n");
  printf(
      "   с обязательным использованием двойных кавычек, для облегчения "
      "дальнейшего\n");
  printf(
      "   анализа. Т.к. утилита перезаписывает существующие файлы в "
      "многопоточном режиме,\n");
  printf(
      "   то для целостности обрабатываемого блока данных пришлось сокращать "
      "наименования\n");
  printf(
      "   полей на 2 символа в целях гарантирования места для вставки 2-х "
      "кавычек.\n\n");
  printf("Список обрабатываемых полей до и после обработки:\n");

  TAG_NODE_T *tag = TagHead;
  while (tag) {
    printf("   %-19s %s\n", tag->Name, tag->NameShort);
    tag = tag->Next;
  }
  printf("\n");
}

void formatHumanReadable(double value, char *buffer, size_t bufferSize) {
  if (value >= GB) {
    snprintf(buffer, bufferSize, "%.2f GB", value / GB);
  } else if (value >= MB) {
    snprintf(buffer, bufferSize, "%.2f MB", value / MB);
  } else if (value >= KB) {
    snprintf(buffer, bufferSize, "%.2f KB", value / KB);
  } else {
    snprintf(buffer, bufferSize, "%.0f bytes", (double)value);
  }
}
