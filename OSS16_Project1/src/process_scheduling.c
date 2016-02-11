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
	return true;
}


