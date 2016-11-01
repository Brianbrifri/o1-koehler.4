
#include "slave.h"
#include "struct.h"

int main (int argc, char **argv) {
  int timeoutValue = 30;
  long long startTime;
  long long currentTime;
  key_t masterKey = 128464;
  key_t slaveKey = 120314;

  int shmid = 0;
  int pcbShmid = 0;

  char *fileName;
  char *defaultFileName = "test.out";
  char *option = NULL;
  char *short_options = "l:m:n:p:t:";
  FILE *file;
  int c;
  myPid = getpid();

  //get options from parent process
  opterr = 0;
  while((c = getopt(argc, argv, short_options)) != -1)
    switch (c) {
      case 'l':
        fileName = optarg;
        break;
      case 'm':
        shmid = atoi(optarg);
        break;
      case 'n':
        processNumber = atoi(optarg);
        break;
      case 'p':
        pcbShmid = atoi(optarg);
        break;
      case 't':
        timeoutValue = atoi(optarg) + 2;
        break;
      case '?':
        fprintf(stderr, "    Arguments were not passed correctly to slave %d. Terminating.\n", myPid);
        exit(-1);
    }

  srand(time(NULL) + processNumber);

  //Try to attach to shared memory
  if((myStruct = (sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("    Slave could not attach shared mem");
    exit(1);
  }

  if((pcbArray = (PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
    perror("    Slave could not attach to shared memory array");
    exit(1);
  }

  //Ignore SIGINT so that it can be handled below
  signal(SIGINT, sigquitHandler);

  //Set the sigquitHandler for the SIGQUIT signal
  signal(SIGQUIT, sigquitHandler);

  //Set the alarm handler
  signal(SIGALRM, zombieKiller);

  //Set the default alarm time
  alarm(QUIT_TIMEOUT);

  if((masterQueueId = msgget(masterKey, IPC_CREAT | 0777)) == -1) {
    perror("    Slave msgget for master queue");
    exit(-1);
  }

  //Set an alarm to 10 more seconds than the parent process
  //so that the child will be killed if parents gets killed
  //and child becomes slave of init
  alarm(timeoutValue);

  int i = 0;
  int j;

  long long duration = 1 + rand() % 100000;

  pcbArray[processNumber].processID = getpid();

  while(myStruct->scheduledProcess != getpid());

  printf("    Slave %d got duration %llu\n", processNumber, duration);
  startTime = myStruct->ossTimer;
  currentTime = myStruct->ossTimer - startTime;

  pcbArray[processNumber].lastBurst = duration;

  myStruct->scheduledProcess = -1;

  sendMessage(masterQueueId, 3, myStruct->ossTimer);

  if(shmdt(myStruct) == -1) {
    perror("    Slave could not detach shared memory struct");
  }

  if(shmdt(pcbArray) == -1) {
    perror("    Slave could not detach from shared memory array");
  }

  printf("    Slave %d exiting\n", processNumber);
  //exit(1);
  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
  printf("    Slave error\n");

}


void sendMessage(int qid, int msgtype, long long finishTime) {
  struct msgbuf msg;

  msg.mType = msgtype;
  sprintf(msg.mText, "%llu.%09llu\n", finishTime / NANO_MODIFIER, finishTime % NANO_MODIFIER);

  if(msgsnd(qid, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("    Slave msgsnd error");
  }

}




void getMessage(int qid, int msgtype) {
  struct msgbuf msg;

  if(msgrcv(qid, (void *) &msg, sizeof(msg.mText), msgtype, MSG_NOERROR) == -1) {
    if(errno != ENOMSG) {
      perror("    Slave msgrcv");
    }
    printf("    Slave: No message available for msgrcv()\n");
  }
  else {
    printf("    Message received by slave #%d: %s", processNumber, msg.mText);
  }
}


//This handles SIGQUIT being sent from parent process
//It sets the volatile int to 0 so that it will not enter in the CS.
void sigquitHandler(int sig) {
  printf("    Slave %d has received signal %s (%d)\n", processNumber, strsignal(sig), sig);
  sigNotReceived = 0;

  if(shmdt(myStruct) == -1) {
    perror("    Slave could not detach shared memory");
  }

  kill(myPid, SIGKILL);

  //The slaves have at most 5 more seconds to exit gracefully or they will be SIGTERM'd
  alarm(5);
}

//function to kill itself if the alarm goes off,
//signaling that the parent could not kill it off
void zombieKiller(int sig) {
  printf("    %sSlave %d is killing itself due to slave timeout override%s\n", MAG, processNumber, NRM);
  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
}
