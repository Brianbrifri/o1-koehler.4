#include "master.h"

int main (int argc, char **argv)
{
  srand(time(0));
  mArg = malloc(20);
  nArg = malloc(20);
  pArg = malloc(20);
  tArg = malloc(20);
  int hflag = 0;
  int nonOptArgFlag = 0;
  int index;
  char *filename = "test.out";
  char *defaultFileName = "test.out";
  char *programName = argv[0];
  char *option = NULL;
  char *short_options = "hs:l:t:";
  int c;

  
  //process arguments
  opterr = 0;
  while ((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1)
    switch (c) {
      case 'h':
        hflag = 1;
        break;
      case 's':
        sValue = atoi(optarg);
        if(sValue > MAXSLAVE) {
          sValue = 20;
          fprintf(stderr, "No more than 20 slave processes allowed at a time. Reverting to 20.\n");
        }
        break;
      case 'l':
        filename = optarg;
        break;
      case 't':
        tValue = atoi(optarg);
        break;
      case '?':
        if (optopt == 's') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          sValue = 5;
        }
        else if (optopt == 'l') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          filename = defaultFileName;
        }
        else if (optopt == 't') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          tValue = 20;
        }
        else if (isprint (optopt)) {
          fprintf(stderr, "Unknown option -%c. Terminating.\n", optopt);
          return -1;
        }
        else {
          printShortHelpMessage();
          return 0;
        }
      }


  //print out all non-option arguments
  for (index = optind; index < argc; index++) {
    fprintf(stderr, "Non-option argument %s\n", argv[index]);
    nonOptArgFlag = 1;
  }

  //if above printed out, print short help message
  //and return from process
  if(nonOptArgFlag) {
    printShortHelpMessage();
    return 0;
  }

  //if help flag was activated, print help message
  //then return from process
  if(hflag) {
    printHelpMessage();
    return 0;
  }

  //****START PROCESS MANAGEMENT****//

  //Initialize the alarm and CTRL-C handler
  signal(SIGALRM, interruptHandler);
  signal(SIGINT, interruptHandler);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  //set the alarm to tValue seconds
  alarm(tValue);

  int sizeArray = sizeof(*pcbArray) * 18;

  //Try to get the shared mem id from the key with a size of the struct
  //create it with all perms
  if((shmid = shmget(timerKey, sizeof(sharedStruct), IPC_CREAT | 0777)) == -1) {
    perror("Bad shmget allocation shared struct");
    exit(-1);
  }

  //Try to attach the struct pointer to shared memory
  if((myStruct = (struct sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("Master could not attach shared mem");
    exit(-1);
  }
  
  //get shmid for pcbArray of 18 pcbs
  if((pcbShmid = shmget(pcbArrayKey, sizeArray, IPC_CREAT | 0777)) == -1) {
    perror("Bad shmget allocation pcb array");
    exit(-1);
  }

  //try to attach pcb array to shared memory
  if((pcbArray = (struct PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
    perror("Master could not attach to pcb array");
    exit(-1);
  }

  //create message queue for the master process
  if((masterQueueId = msgget(masterQueueKey, IPC_CREAT | 0777)) == -1) {
    perror("Master msgget for master queue");
    exit(-1);
  }


  for (int i = 0; i < ARRAY_SIZE; i++) {
    pcbArray[i].processID = 0;
  }

  //Open file and mark the beginning of the new log
  file = fopen(filename, "a");
  if(!file) {
    perror("Error opening file");
    exit(-1);
  }

  myStruct->ossTimer = 0;
  myStruct->scheduledProcess = -1;
  myStruct->sigNotReceived = 1;

  createQueues();

  fprintf(file,"***** BEGIN LOG *****\n");

  do {

    if(isTimeToSpawn()) {
      spawnSlave();
      setTimeToSpawn();
    }

    myStruct->scheduledProcess = scheduleNextProcess();

    myStruct->ossTimer += incrementTimer();

    updateAfterProcessFinish(waitForTurn());
  
  } while (myStruct->ossTimer < 2000000 && myStruct->sigNotReceived);

  if(!cleanupCalled) {
    cleanupCalled = 1;
    printf("Master cleanup called from main\n");
    cleanup();
  }
  return 0;
}

bool isTimeToSpawn(void) {
  return timeToSpawn >= myStruct->ossTimer ? true : false;
}

void setTimeToSpawn(void) {
  timeToSpawn = myStruct->ossTimer + rand() % 2000000000;
}

void spawnSlave(void) {

    processNumberBeingSpawned = -1;
    
    for(int i = 0; i < ARRAY_SIZE; i++) {
      if(pcbArray[i].processID == 0) {
        processNumberBeingSpawned = i;
        pcbArray[i].processID = 1;
        printf("Got location %d for new process\n", processNumberBeingSpawned);
        break;
      } 
    }

    if(processNumberBeingSpawned != -1) {
      printf("About to spawn process #%d\n", processNumberBeingSpawned);
      //exit on bad fork
      if((childPid = fork()) < 0) {
        perror("Fork Failure");
        //exit(1);
      }

      //If good fork, continue to call exec with all the necessary args
      if(childPid == 0) {
        printf("get my pid: %d\n", getpid());
        pcbArray[processNumberBeingSpawned].processID = getpid();
        pcbArray[processNumberBeingSpawned].priority = queuePriorityNormal_1;
        pcbArray[processNumberBeingSpawned].totalScheduledTime = scheduleProcessTime();
        sprintf(mArg, "%d", shmid);
        sprintf(nArg, "%d", processNumberBeingSpawned);
        sprintf(pArg, "%d", pcbShmid);
        sprintf(tArg, "%d", tValue);
        char *slaveOptions[] = {"./slaverunner", "-m", mArg, "-n", nArg, "-p", pArg, "-t", tArg, (char *)0};
        execv("./slaverunner", slaveOptions);
        fprintf(stderr, "    Should only print this in error\n");
      }
      
    }
    printf("assignedPCB = %d\n", processNumberBeingSpawned);
    if(processNumberBeingSpawned != -1) {
      while(pcbArray[processNumberBeingSpawned].processID <= 1); 
      Enqueue(pcbArray[processNumberBeingSpawned].processID, QUEUE1);
    }

}



int incrementTimer(void) {
  return rand() % 1001;
}

int scheduleProcessTime(void) {
  return rand() % 70000;
}

int waitForTurn(void) {
  struct msgbuf msg;

  if(msgrcv(masterQueueId, (void *) &msg, sizeof(msg.mText), 3, MSG_NOERROR) == -1) {
    if(errno != ENOMSG) {
      perror("Error master receiving message");
      return -1;
    }
    printf("No message for master\n");
    return -1;
  }
  else {
    printf("Message received by master: %s\n", msg.mText);
    int processNum = atoi(msg.mText);
    while(myStruct->scheduledProcess != -1);
    return processNum;
  }
}

void updateAfterProcessFinish(int processLocation) {

  if(processLocation == -1) {
    return;
  }

  pid_t id = pcbArray[processLocation].processID;
  long long lastBurst = pcbArray[processLocation].lastBurst;
  long long priority = pcbArray[processLocation].priority;

  if(id != 0) {
    if(priority == queuePriorityHigh) {
      Enqueue(id, QUEUE0);
    }
    else if(lastBurst < priority) {
      pcbArray[processLocation].priority = queuePriorityNormal_1;
      Enqueue(id, QUEUE1);
    }
    else {
      if(priority == queuePriorityNormal_1) {
        pcbArray[processLocation].priority = queuePriorityNormal_2;
        Enqueue(id, QUEUE2);
      }
      else if(priority == queuePriorityNormal_2) {
        pcbArray[processLocation].priority = queuePriorityNormal_3;
        Enqueue(id, QUEUE3);
      }
      else if(priority == queuePriorityNormal_3) {
        pcbArray[processLocation].priority = queuePriorityNormal_3;
        Enqueue(id, QUEUE3);
      }
      else {
        printf("Unhandled priority exception\n");
      
      }

    }
 
  }
  else {
    printf("Process completed its time\n");
  }

}

pid_t scheduleNextProcess(void) {
  if(!isEmpty(QUEUE0)) {
    return pop(QUEUE0);
  }
  else if(!isEmpty(QUEUE1)) {
    return pop(QUEUE1);
  }
  else if(!isEmpty(QUEUE2)) {
    return pop(QUEUE2);
  }
  else if(!isEmpty(QUEUE3)) {
    return pop(QUEUE3);
  }
  else return -1;
}

void sendMessage(int qid, int msgtype) {
  struct msgbuf msg;

  msg.mType = msgtype;
  sprintf(msg.mText, "Master initiating slave queue\n");

  if(msgsnd(qid, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("Master msgsnd error");
  }

}


//Set queue pointers to null
void createQueues() {
  front0 = front1 = front2 = front3 = NULL;
  rear0 = rear1 = rear2 = rear3 = NULL;
  queue0size = queue1size = queue2size = queue3size = 0;
}

//Function to check if a queue is empty
bool isEmpty(int choice) {
  switch(choice) {
    case 0:
      if((front0 == NULL) && (rear0 == NULL))
        return true;
      break;
    case 1:
      if((front1 == NULL) && (rear1 == NULL))
        return true;
      break;
    case 2:
      if((front2 == NULL) && (rear2 == NULL))
        return true;
      break;
    case 3:
      if((front3 == NULL) && (rear3 == NULL))
        return true;
      break;
    default:
      printf("Not a valid queue choice\n");
  }
  return false;
}

//Function to add a process id to a given queue
void Enqueue(pid_t processId, int choice) {
  printf("Enqueuing pid %d in queue %d\n", processId, choice);
  switch(choice) {
    case 0:
      if(rear0 == NULL) {
        rear0 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear0->next = NULL;
        rear0->id = processId;
        front0 = rear0;
      }
      else {
        temp0 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear0->next = temp0;
        temp0->id = processId;
        temp0->next = NULL;

        rear0 = temp0;
      }
      queue0size++;
      break;
    case 1:
      if(rear1 == NULL) {
        rear1 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear1->next = NULL;
        rear1->id = processId;
        front1 = rear1;
      }
      else {
        temp1 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear1->next = temp1;
        temp1->id = processId;
        temp1->next = NULL;

        rear1 = temp1;
      }
      queue1size++;
      break;
    case 2:
      if(rear2 == NULL) {
        rear2 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear2->next = NULL;
        rear2->id = processId;
        front2 = rear2;
      }
      else {
        temp2 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear2->next = temp2;
        temp2->id = processId;
        temp2->next = NULL;

        rear2 = temp2;
      }
      queue2size++;
      break;
    case 3:
      if(rear3 == NULL) {
        rear3 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear3->next = NULL;
        rear3->id = processId;
        front3 = rear3;
      }
      else {
        temp3 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear3->next = temp3;
        temp3->id = processId;
        temp3->next = NULL;

        rear3 = temp3;
      }
      queue3size++;
      break;
    default:
      printf("Not a valid queue choice\n");
  }
}

//function to pop the process id for a given queue
pid_t pop(int choice) {
  pid_t poppedID;
  switch(choice) {
    case 0:
      frontA0 = front0;
      if(frontA0 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA0->next != NULL) {
          frontA0 = frontA0->next;
          printf("popped %d\n", front0->id);
          poppedID = front0->id;
          free(front0);
          front0 = frontA0;
        }
        else {
          printf("popped %d\n", front0->id);
          poppedID = front0->id;
          free(front0);
          front0 = NULL;
          rear0 = NULL;
        }
        queue0size--;
      }
      break;
    case 1:
      frontA1 = front1;
      if(frontA1 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA1->next != NULL) {
          frontA1 = frontA1->next;
          poppedID = front1->id;
          free(front1);
          front1 = frontA1;
        }
        else {
          poppedID = front1->id;
          free(front1);
          front1 = NULL;
          rear1 = NULL;
        }
        queue1size--;
      }
      break;
    case 2:
      frontA2 = front2;
      if(frontA2 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA2->next != NULL) {
          frontA2 = frontA2->next;
          poppedID = front2->id;
          free(front2);
          front2 = frontA2;
        }
        else {
          poppedID = front2->id;
          free(front2);
          front2 = NULL;
          rear2 = NULL;
        }
        queue2size--;
      }
      break;
    case 3:
      frontA3 = front3;
      if(frontA3 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA3->next != NULL) {
          frontA3 = frontA3->next;
          poppedID = front3->id;
          free(front3);
          front3 = frontA3;
        }
        else {
          poppedID = front3->id;
          free(front3);
          front3 = NULL;
          rear3 = NULL;
        }
        queue3size--;
      }
      break;
    default:
      printf("Not a valid queue choice\n");
  }
  return poppedID;
}

//Interrupt handler function that calls the process destroyer
//Ignore SIGQUIT and SIGINT signal, not SIGALRM, so that
//I can handle those two how I want
void interruptHandler(int SIG){
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  if(SIG == SIGINT) {
    fprintf(stderr, "\n%sCTRL-C received. Calling shutdown functions.%s\n", RED, NRM);
  }

  if(SIG == SIGALRM) {
    fprintf(stderr, "%sMaster has timed out. Initiating shutdown sequence.%s\n", RED, NRM);
  }

  if(!cleanupCalled) {
    cleanupCalled = 1;
    printf("Master cleanup called from interrupt\n");
    cleanup();
  }
}

//Cleanup memory and processes.
//kill calls SIGQUIT on the groupid to kill the children but
//not the parent
void cleanup() {
  signal(SIGQUIT, SIG_IGN);
  myStruct->sigNotReceived = 0;

  printf("Master sending SIGQUIT\n");
  kill(-getpgrp(), SIGQUIT);

  //free up the malloc'd memory for the arguments
  free(mArg);
  free(nArg);
  free(pArg);
  free(tArg);
  printf("Master waiting on all processes do die\n");
  childPid = wait(&status);

  printf("Master about to detach from shared memory\n");
  //Detach and remove the shared memory after all child process have died
  if(detachAndRemoveTimer(shmid, myStruct) == -1) {
    perror("Failed to destroy shared memory segment");
  }

  if(detachAndRemoveArray(pcbShmid, pcbArray) == -1) {
    perror("Failed to destroy shared memory segment");
  }

  clearQueues();

  printf("Master about to delete message queues\n");
  //Delete the message queues
  msgctl(slaveQueueId, IPC_RMID, NULL);
  msgctl(masterQueueId, IPC_RMID, NULL);

  if(fclose(file)) {
    perror("    Error closing file");
  }


  printf("Master about to kill itself\n");
  //Kill this master process
  kill(getpid(), SIGKILL);
}


//make sure all the nodes in the queues are freed
void clearQueues(void) {
  while(!isEmpty(QUEUE0)) {
    pop(QUEUE0);
  }
  while(!isEmpty(QUEUE1)) {
    pop(QUEUE1);
  }
  while(!isEmpty(QUEUE2)) {
    pop(QUEUE2);
  }
  while(!isEmpty(QUEUE3)) {
    pop(QUEUE3);
  }
}

//Detach and remove function
int detachAndRemoveTimer(int shmid, sharedStruct *shmaddr) {
  printf("Master: Detach and Remove Shared Memory\n");
  int error = 0;
  if(shmdt(shmaddr) == -1) {
    error = errno;
  }
  if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
    error = errno;
  }
  if(!error) {
    return 0;
  }

  return -1;
}

//Detach and remove function
int detachAndRemoveArray(int shmid, PCB *shmaddr) {
  printf("Master: Detach and Remove Shared Memory\n");
  int error = 0;
  if(shmdt(shmaddr) == -1) {
    error = errno;
  }
  if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
    error = errno;
  }
  if(!error) {
    return 0;
  }

  return -1;
}

//Long help message
void printHelpMessage(void) {
    printf("\nThank you for using the help menu!\n");
    printf("The following is a helpful guide to enable you to use this\n");
    printf("slavedriver program to the best of your ability!\n\n");
    printf("-h, --help: Prints this help message.\n");
    printf("-s: Allows you to set the number of slave process waiting to run.\n");
    printf("\tThe default value is 5. The max is 20.\n");
    printf("-l: Allows you to set the filename for the logger so the aliens can see how bad you mess up.\n");
    printf("\tThe default value is test.out.\n");
    printf("-t: Allows you set the wait time for the master process until it kills the slaves.\n");
    printf("\tThe default value is 20.\n");
}

//short help message
void printShortHelpMessage(void) {
  printf("\nAcceptable options are:\n");
  printf("[-h], [--help], [-l][required_arg], [-s][required_arg], [-t][required_arg]\n\n");
}
