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


void spawnSlaves(int);
void interruptHandler(int);
void cleanup(void);
void sendMessage(int, int);
void processDeath(int, int, FILE*);
int detachAndRemove(int, sharedStruct*);
void printHelpMessage(void);
void printShortHelpMessage(void);

struct option long_options[] = {
  {"help", no_argument, 0, 'h'},
  {0,     0,            0,  0},
  {}
};

#define MAX_QUEUE_SIZE 18
//PCB Array//
struct PCB pcbArray[MAX_QUEUE_SIZE];

//Begin queue stuff//

struct queue {
  pid_t id;
  struct queue *next;
  struct queue *previous;

} queue;

bool isEmpty(struct queue);
void Enqueue(pid_t, struct queue);
pid_t pop(struct queue);

char *mArg;
char *nArg;
char *tArg;

volatile sig_atomic_t cleanupCalled = 0;

pid_t myPid, childPid;
int tValue = 20;
int sValue = 5;
int status;
int shmid;
int slaveQueueId;
int masterQueueId;
int nextProcessToSend = 1;
int processNumberBeingSpawned = 1;
int messageReceived = 0;
//long long *ossTimer = 0;

struct sharedStruct *myStruct;

const int TOTAL_SLAVES = 100;
const int MAXSLAVE = 20;
const long long INCREMENTER = 40000;
FILE *file;
struct msqid_ds msqid_ds_buf;


#endif