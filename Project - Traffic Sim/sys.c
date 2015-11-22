//sys.c

DEFINE_SPINLOCK(sem_lock);
 
	asmlinkage long sys_cs1550_down(struct cs1550_sem *sem){
		spin_lock(&sem_lock);	//stops the semaphore from being interrupted
		sem->value--;			
		 if(sem->value<0){		//if the value is less than 0 sleep a process
			//add process P to the queue of waiting processes
			struct task_struct *cur = current;	//gets the current process
			sem->processes[sem->front] = cur;	//add it to queue
			sem->front = (sem->front + 1) % 100;
			set_current_state(TASK_INTERRUPTIBLE);	//sleep
			schedule();								//call scheduler
		}
		spin_unlock(&sem_lock);	
	}

	asmlinkage long sys_cs1550_up(struct cs1550_sem *sem){
		spin_lock(&sem_lock);	//stops the semaphore from being interrupted
		sem->value++;			
		if(sem->value<=0){		//if the value is greater than 0 we need to wake up a process from the queue
			wake_up_process(sem->processes[sem->back]);//wake up	
			sem->back = (sem->back + 1) % 100;
		}
		spin_unlock(&sem_lock);	
	}

//unistd.h
	#define __NR_cs1550_up          325
	#define __NR_cs1550_down        326
	//Up and Down system calls added to unisted.h which links them to their index i$

//syscall_table.S

	.long sys_cs1550_up             /* 325 */
	.long sys_cs1550_down

