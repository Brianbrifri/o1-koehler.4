#ifndef MASTER_H
#define MASTER_H
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include "struct.h"


void spawnSlave(void);
bool isTimeToSpawn(void);
void setTimeToSpawn(void);
int incrementTimer(void);
int scheduleProcessTime(void);
pid_t scheduleNextProcess(void);
void interruptHandler(int);
void cleanup(void);
void sendMessage(int, int);
int detachAndRemoveTimer(int, sharedStruct*);
int detachAndRemoveArray(int, PCB*);
void printHelpMessage(void);
void printShortHelpMessage(void);

struct option long_options[] = {
  {"help", no_argument, 0, 'h'},
  {0,     0,            0,  0},
  {}
};

//PCB Array//
struct PCB *pcbArray;

//Begin queue stuff//

struct queue {
  pid_t id;
  struct queue *next;

} *front0, *front1, *front2, *front3,
  *rear0, *rear1, *rear2, *rear3,
  *temp0, *temp1, *temp2, *temp3,
  *frontA0, *frontA1, *frontA2, *frontA3;

int queue0size;
int queue1size;
int queue2size;
int queue3size;

void createQueues(void);
bool isEmpty(int);
void Enqueue(pid_t, int);
pid_t pop(int);
void clearQueues(void);

const int QUEUE0 = 0;
const int QUEUE1 = 1;
const int QUEUE2 = 2;
const int QUEUE3 = 3;

//End Queue Stuff//
//Char arrays for arg passing to children//
char *mArg;
char *nArg;
char *pArg;
char *tArg;

volatile sig_atomic_t cleanupCalled = 0;

pid_t myPid, childPid;
int tValue = 20;
int sValue = 3;
int status;
int shmid;
int pcbShmid;
int slaveQueueId;
int masterQueueId;
int nextProcessToSend = 1;
int processNumberBeingSpawned = -1;
long long timeToSpawn = 0;
int messageReceived = 0;
//long long *ossTimer = 0;

struct sharedStruct *myStruct;

const int TOTAL_SLAVES = 100;
const int MAXSLAVE = 20;
const long long INCREMENTER = 40000;
const int ARRAY_SIZE = 18;
FILE *file;
struct msqid_ds msqid_ds_buf;

key_t timerKey = 148364;
key_t pcbArrayKey = 135155;
key_t masterQueueKey = 128464;
#endif
