#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dyn_array.h>
#include "../include/processing_scheduling.h"


// private function
void virtual_cpu(ProcessControlBlock_t* process_control_block) {
	// decrement the burst time of the pcb
	--process_control_block->remaining_burst_time;
}

bool first_come_first_serve(dyn_array_t* ready_queue, ScheduleResult_t* result) {
	if (!ready_queue || !result) return false;
	ProcessControlBlock_t *current = (ProcessControlBlock_t *)malloc(sizeof(ProcessControlBlock_t));
	if (current == NULL) return false;
	float size_of_array = dyn_array_size(ready_queue);
	float total_latency = 0, iteration_latency = 0;
	while(dyn_array_extract_back(ready_queue, current) != false) {
		total_latency += iteration_latency;
		result->average_wall_clock_time += iteration_latency;
		while(current->remaining_burst_time > 0) {
			virtual_cpu(current);
			++result->total_run_time;
			++result->average_wall_clock_time;
			++total_latency;
			++iteration_latency;
		}
	}
	total_latency -= iteration_latency;
	result->average_latency_time = total_latency/size_of_array;
	result->average_wall_clock_time = result->average_wall_clock_time/size_of_array;
	free(current);
	return true;
}

// 24,3,3 
// [0] latency = 0, wallclock = 24
// [1] latency = 24, wallclock = 27
// [2] latency = 27, wallclock = 30


// 6,8,7,3 --> 6 + (6+8) + (6+8+7) 
//             6  + 14   +   21
//                20     +   21