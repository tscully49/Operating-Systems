#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dyn_array.h>
#include "../include/processing_scheduling.h"

// The time limit per process using the CPU
// used for the round robin process scheduling algorithm
#define QUANTUM 4 

// private function
void virtual_cpu(ProcessControlBlock_t* process_control_block) {
	// decrement the burst time of the pcb
	--process_control_block->remaining_burst_time;
}

bool first_come_first_serve(dyn_array_t* ready_queue, ScheduleResult_t* result) {
	if (!ready_queue || !result) return false; // Checks paramteters to the function
	ProcessControlBlock_t *current = (ProcessControlBlock_t *)malloc(sizeof(ProcessControlBlock_t)); // Malloc memory for a buffer
	if (current == NULL) return false; // Checks return value of the malloc call to make sure it exectuted properly
	float size_of_array = dyn_array_size(ready_queue); // Find the size of the dynamic array
	float total_latency = 0, iteration_latency = 0; // create two latency variables to calculate different things 
	while(dyn_array_extract_back(ready_queue, current) != false) { // Pull the back of the dynamic array and check that it isn't empty(will return false if the array is empty)
		total_latency += iteration_latency; // add the last run's latency time to the total latency
		result->average_wall_clock_time += iteration_latency; // add the last run's latency to the total wall clock
		while(current->remaining_burst_time > 0) { // run the process using virtual_cpu until it is done
			virtual_cpu(current); // decrement the remaining burst time
			++result->total_run_time; // increment the total run time counter
			++result->average_wall_clock_time; // incremement the total wall clock counter
			++total_latency; // increment the total latency counter
			++iteration_latency; // increment the latency for each process since the beginning
		}
	}
	total_latency -= iteration_latency; // remove the "last" last run's latency from the total
	result->average_latency_time = total_latency/size_of_array; // calculate the average total latency
	result->average_wall_clock_time = result->average_wall_clock_time/size_of_array; // calculate average total wallclock
	free(current); // free memory for the buffer
	return true; // return true on success
}

// 24,3,3 
// [0] latency = 0, wallclock = 24
// [1] latency = 24, wallclock = 27
// [2] latency = 27, wallclock = 30

bool round_robin(dyn_array_t* ready_queue, ScheduleResult_t* result) {
	return true;
}
