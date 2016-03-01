#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <dyn_array.h>
#include <errno.h>
#include <unistd.h>
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

bool first_come_first_serve(dyn_array_t* ready_queue, ScheduleResult_t* result) {
	if (!ready_queue || !result) {
		return false; // Checks paramteters to the function
	}

	ProcessControlBlock_t *current = (ProcessControlBlock_t *)malloc(sizeof(ProcessControlBlock_t)); // Malloc memory for a buffer
	if (current == NULL) return false; // Checks return value of the malloc call to make sure it exectuted properly
	
	float size_of_array = dyn_array_size(ready_queue); // Find the size of the dynamic array
	float total_latency = 0, total_clock_time = 0, clocktime = 0; // create two latency variables to calculate different things 

	while(1) {
		pthread_mutex_lock(&mutex); // Lock mutex so thread can grab from PCB array
		if (!dyn_array_empty(ready_queue)) { // check to make sure the dyn array is not empty
			dyn_array_extract_back(ready_queue, current);
			printf("\nGRABBING NEW PROCESS FCFS: %u\n", current->remaining_burst_time);
			pthread_mutex_unlock(&mutex); // Unlock the mutex so another thread can use it 
		} else {
			pthread_mutex_unlock(&mutex); // Unlock the mutex so another thread can use it
			break;
		}
		// Pull the back of the dynamic array and check that it isn't empty(will return false if the array is empty
		total_latency += clocktime; // add the last run's latency time to the total latency
		while(current->remaining_burst_time > 0) { // run the process using virtual_cpu until it is done
			printf("\nCPU FCFS");
			virtual_cpu(current); // decrement the remaining burst time
			++clocktime;
		}
		total_clock_time += clocktime;
	}

	result->average_latency_time = total_latency/size_of_array; // calculate the average total latency
	result->average_wall_clock_time = total_clock_time/size_of_array; // calculate average total wallclock
	result->total_run_time = clocktime;
	free(current); // free memory for the buffer
	return true; // return true on success
}

bool round_robin(dyn_array_t* ready_queue, ScheduleResult_t* result) {
	if (!ready_queue || !result) return false; // Checks paramteters to the function
	
	ProcessControlBlock_t *current = (ProcessControlBlock_t *)malloc(sizeof(ProcessControlBlock_t)); // Malloc memory for a buffer
	if (current == NULL) return false; // Checks return value of the malloc call to make sure it exectuted properly
	
	float size_of_array = dyn_array_size(ready_queue); // Find the size of the dynamic array
	float clocktime = 0, total_latency = 0, total_clock_time = 0;
	while(1) { // Pull the back of the dynamic array and check that it isn't empty(will return false if the array is empty)
		pthread_mutex_lock(&mutex); // Lock the mutex so the thread can grab from the PCB array
		if(!dyn_array_empty(ready_queue)) {
			dyn_array_extract_back(ready_queue, current);
			printf("\nGRABBING NEW PROCESS RR: %u\n", current->remaining_burst_time);
			pthread_mutex_unlock(&mutex); // Unlock mutex so that other threads can use it 
			if(current->started == 0) {
				current->started = 1; // flip the started variable in the PCB
				total_latency += clocktime;
			}
			for (size_t i = 0;i<QUANTUM;++i) {
				printf("\nCPU RR");
				virtual_cpu(current);
				clocktime++;
				if(current->remaining_burst_time == 0) {
					break; // break from the for loop if the process is done executing before reaching the quantum
				} 	
			}
			// ...since the wall clock will be increased for all processes in the queue that are already running each time a different process executes
			if(current->remaining_burst_time == 0) {
				total_clock_time += clocktime;
				continue;
			} else {
				pthread_mutex_lock(&mutex); // Lock the mutex so the thread can write from the PCB array
				dyn_array_push_front(ready_queue, current); // If there is still burst time left in the process, add it to the back (the front) of the queue
				pthread_mutex_unlock(&mutex); // Unlock mutex so that other threads can use it 
			}
		} else {
			pthread_mutex_unlock(&mutex); // Unlock the mutex so another thread can use it
			break;
		}
	}

	result->average_wall_clock_time = total_clock_time/size_of_array; // Set variable to the calculated average wall clock
	result->average_latency_time = total_latency/size_of_array; // Set the result variable to the calculated average latency
	result->total_run_time = clocktime;
	free(current); // free memory for the buffer
	return true;
}

/*
* MILESTONE 3 GIVEN CODE
*/
void destroy_mutex(void) {
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

/*
* MILESTONE 3 CODE
*/
dyn_array_t* load_process_control_blocks (const char* input_file ) {
	// error check parameters and size of dyn array, size of dyn array should equal number of burst times in file
	if(!input_file || strcmp(input_file, "") == 0 || strcmp(input_file, "\n") == 0) return NULL;
	
	dyn_array_t *PCB_array = NULL; // create an empty dynamic array that will hold PCB's, initialize start to 0
	ProcessControlBlock_t pcb_buffer; // buffer for PCB array indicies

	int fd = open(input_file, O_RDONLY); // Open binary file with binary access
	if (fd == -1) { // if error opening the file
		return NULL;
	}

	uint32_t buffer = 0; // buffer to hold the burst time until it is added to the pcb
	uint32_t total_pcbs = 0; // first line of file, which holds the number of PCBs in the file

	ssize_t num_read = read(fd, &total_pcbs, sizeof(uint32_t));
	if (num_read == 0) { // error check to make sure that there is actual data in the file
		return NULL;
	}

	PCB_array = dyn_array_create(0,sizeof(ProcessControlBlock_t),NULL);
	if (PCB_array == NULL) {
		return NULL;
	}
	
	for (uint32_t i = 0; i<total_pcbs; ++i) { // read through the file, and input the burst time into new PCB's
		if((num_read = read(fd, &buffer, sizeof(uint32_t))) == 0) {
			dyn_array_destroy(PCB_array); // if error, delete dyn array 
			return NULL;
		}
		// fill the pcb buffer and copy it to a new dyn_array index
		pcb_buffer.remaining_burst_time = buffer; // put data in the PCB 
		pcb_buffer.started = 0; // Initialize started to 0 
		dyn_array_push_back(PCB_array, (void *)&pcb_buffer); // push to PCB array
	}

	close(fd); // Close file
	return PCB_array; // return a pointer to the dyn array if no errors occur
}

void* first_come_first_serve_worker (void* input) {
	if (!input) { // error check parameters
		printf("\nNo parameters received in thread.\n");
		return NULL;
	}

	// assign *input to a variable of type WorkerInput_t
	WorkerInput_t *the_input = (WorkerInput_t *)input; // Cast void pointer to its correct data type
	ScheduleResult_t *results = the_input->results; // Cast the attributes from the worker struct to variables
	dyn_array_t *ready_queue = the_input->ready_queue_array; // The ready q that all threads share

	if (first_come_first_serve(ready_queue, results) == false) { // check for any errors in the FCFS algorithm
		printf("\nPROBLEMS IN FCFS ALGORITHM\n"); 
	}
	
	return NULL;
}

void* round_robin_worker (void* input) {
	if (!input) { // error check parameters
		printf("\nNo parameters received in thread.\n");
		return NULL;
	}

	// assign *input to a variable of type WorkerInput_t
	WorkerInput_t *the_input = (WorkerInput_t *)input; // Cast void pointer to its correct data type
	ScheduleResult_t *results = the_input->results; // Cast the attributes from the worker struct to variables
	dyn_array_t *ready_queue = the_input->ready_queue_array; // The ready q that all threads share

	if (round_robin(ready_queue, results) == false) { // check for any errors in the RR algorithm 
		printf("\nPROBLEMS IN RR ALGORITHM\n");
	}

	return NULL;
}
