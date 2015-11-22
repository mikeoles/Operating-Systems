#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>


#define QSIZE 50//size of the buffer (max number of cars on each side)
	
//Semaphore created for trafficsim	
struct cs1550_sem
{
	int value;	
	int front;	//front of the process queue
	int back;	//back of the process queue
	struct task_struct *processes[100];//Some process queue of your devising
};

	//Create a mutex for queue1 and queue2 with a semaphore
	struct cs1550_sem mutexN;
	struct cs1550_sem mutexS;	

	//Keeps track of the count of both mutexes
	struct cs1550_sem countTotal;

	//Holds the cars from one direction of traffic
	int qN[QSIZE];
	struct cs1550_sem queueNempty;;
	struct cs1550_sem queueNfull;


	//Holds the cars from the opposite direction of traffic
	int qS[QSIZE];
	struct cs1550_sem queueSempty;;
	struct cs1550_sem queueSfull;
	
	//Initialize Variables
	int *inN;		//Keeps track of where to add and remove cars from the queues
	int *inS;
	int *outN;		//Keeps track of where to add and remove cars from the queues
	int *outS;
	int *carNum;	//used to print to screen to keep track of cars
	int *queueN;
	int *queueS;
	
//function pointers	
void* producerN();
void* producerS();
void* consumer();

main(int argc, char** argv){

//set the values for each semaphore (front and back are used for the process queue start at 0 always)
	mutexN.value = 1;
	mutexN.front = 0;	
	mutexN.back = 0;	

	mutexS.value = 1;
	mutexS.front = 0;	
	mutexS.back = 0;	

	countTotal.value = 0;	
	countTotal.front = 0;	
	countTotal.back = 0;

	queueNempty.value = QSIZE;
	queueNempty.front = 0;	
	queueNempty.back = 0;		

	queueNfull.value = 0;
	queueNfull.front = 0;	
	queueNfull.back = 0;	

	queueSempty.value = QSIZE;
	queueSempty.front = 0;	
	queueSempty.back = 0;

	queueSfull.value = 0;
	queueSfull.front = 0;	
	queueSfull.back = 0;	
	
	//allows queues to be mmapped 
	queueN = qN;
	queueS = qS;
	
	int N = 5 + QSIZE + QSIZE;//size of region to MMAP
	void *ptr = mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);//ptr is start of mmapped region	
	
	//mmaped variables
	inN = ptr;
	inS = inN + 1;
	outN = inS + 1;
	outS = outN + 1;
	carNum = outS + 1;
	queueN = carNum + 1;
	queueS = queueN + QSIZE;
	
	*inN = 0; 
	*outN = 0;
	*inS = 0; 
	*outS = 0;
	*carNum = 1;

	pid_t  pid;

    pid = fork();	//Create 3 seperate processes, one for each producer and one for the consumer
    if (pid == 0){ 
         producerN();
	}
    else{
	    pid = fork();
		if (pid == 0){ 
			producerS();
		}else{ 
			consumer();
		}	
	}
	return 0;
	
}

//Converts up() and down() functions into system calls
void up(struct cs1550_sem *sem) {
	syscall(__NR_cs1550_up, sem);
}
	
void down(struct cs1550_sem *sem) {
	syscall(__NR_cs1550_down, sem);
}	
	
//creates new cars coming from the North
void* producerN(){
	while(1){
		down(&queueNempty);				//decrease num of empty locations
		down(&mutexN);					//start critical region
		queueN[*inN] = *carNum;			//adds a car to queue
		*inN = (*inN + 1) % QSIZE;		
		printf("Car %d heading North /n",*carNum);
		if(countTotal.value == 0 ){		//only does this if the value of countTotal signals the flagman was sleeping
			printf("Car %d honks its horn to wake up the flag man",*carNum);
		}			
		*carNum++;						
		up(&mutexN);					//end critical region
		up(&queueNfull);				//increase number of full locations
		up(&countTotal);
		int chance = rand() % 10;
		if(chance < 2){		//There is an 20% chance that no car will follow
			sleep(20);			//No car followed so there will be no more cars for 20 seconds from this side
		}
	}
}

//creates new cars coming from the South
void* producerS(){
	while(1){
		down(&queueSempty);				//decrease num of empty locations
		down(&mutexS);					//start critical region
		queueS[*inS] = *carNum;			//adds a car to queue
		*inS = (*inS + 1) % QSIZE;
		printf("Car %d heading South /n",*carNum);
		if(countTotal.value == 0 ){		//only does this if the value of countTotal signals the flagman was sleeping
			printf("Car %d honks its horn to wake up the flag man",*carNum);
		}		
		*carNum++;
		up(&mutexS);					//end critical region
		up(&queueSfull);				//increase number of full locations
		up(&countTotal);
		int chance = rand() % 10;
		if(chance < 8){		//There is an 20% chance that no car will follow
			sleep(20);			//No car followed so there will be no more cars for 20 seconds from this side
		}
	}		
}

//Flagger - allows cars to pass
void* consumer(){
	if(countTotal.value == 0){
		printf("There are no more cars, the flagperson will go to sleep");
	}
	down(&countTotal);
	if(queueSfull.value <10 && queueNfull.value > 0){//Pass cars from North Side
		down(&queueNfull);			//decrease number of full locations
		down(&mutexN);				//start critical region
		int num = queueN[*outN];	//get first item in queue
		printf("Car %d passes through construction area/n",num);
		up(&mutexN);				//end critical region
		up(&queueNempty);
	}else if(queueNfull.value <10 && queueSfull.value > 0){//Pass cars from South Side
		down(&queueSfull);			//decrease number of full locations
		down(&mutexS);				//start critical region
		int num = queueS[*outS];	//get first item in queue
		printf("Car %d passes through construction area/n",num);
		up(&mutexS);				//end critical region
		up(&queueSempty);		
	}
		
}