#ifndef STRUCT_H
#define STRUCT_H
#include <sys/types.h>
#include <stdbool.h>
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YLW "\x1b[33m"
#define BLU "\x1b[34m"
#define MAG "\x1b[35m"
#define NRM "\x1b[0m"
static const long long NANO_MODIFIER = 1000000000;

typedef struct sharedStruct {
  long long ossTimer;
  int sigNotReceived;
  pid_t scheduledProcess;
} sharedStruct;

typedef struct msgbuf {
  long mType;
  char mText[80];
} msgbuf;

typedef struct PCB {
  pid_t processID;
  long long totalScheduledTime;
  long long totalTimeRan;
  long long lastBurst;
  long long priority;
} PCB;

#endif
