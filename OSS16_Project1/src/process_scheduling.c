#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dyn_array.h>
#include "../include/processing_scheduling.h"

#define QUANTUM 4 // Used for Robin Round for process as the run time limit

//global lock variable
pthread_mutex_t mutex;

// private function
void virtual_cpu(ProcessControlBlock_t* process_control_block) {
	// decrement the burst time of the pcb
	--process_control_block->remaining_burst_time;
	sleep(1);
}


void destroy_mutex (void) {
	pthread_mutex_destroy(&mutex);	
};	

// init the protected mutex
bool init_lock(void) {
	if (pthread_mutex_init(&mutex,NULL) != 0) {
		return false;
	}
	atexit(destroy_mutex);
	return true;
}


bool first_come_first_serve(dyn_array_t* ready_queue, ScheduleResult_t* result) {
	
}

bool round_robin(dyn_array_t* ready_queue, ScheduleResult_t* result) {
	
}
/*
* MILESTONE 3 CODE
*/
dyn_array_t* load_process_control_blocks (const char* input_file ) {
	
}

void* first_come_first_serve_worker (void* input) {
	
}

void* round_robin_worker (void* input) {
	
}

