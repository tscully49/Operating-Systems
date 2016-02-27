// Put the code for your analysis program here!
#include "../include/processing_scheduling.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	if(argc < 3) {
		printf("\nPlease use format './analysis (filename) FCFS/RR'\n");
		return 0;
	}

	int i, rc;
	int num_cores = argc-2;
	void *status;
	for (i=2; i<argc; ++i) {
		if(strcmp(argv[i], "RR") != 0 && strcmp(argv[i], "FCFS") != 0) {
			//printf("\nONE CORE IS LABELED INCORRECTLY (FCFS or RR)\n");
			return 0;
		}
		printf("CORE: %s\n", argv[i]);
	}

	dyn_array_t *PCB_array = load_process_control_blocks(argv[1]); // create an empty dynamic array that will hold PCB's, initialize start to 0

	if (init_lock() != true) {
		printf("\nSomething went wrong with creating the mutex!\n");
		return 0;
	}
	WorkerInput_t *the_input = (WorkerInput_t *)malloc(sizeof(WorkerInput_t)*num_cores);
	for (i=0;i<num_cores;++i) {
		(the_input+i)->results = (ScheduleResult_t *)malloc(sizeof(ScheduleResult_t));
		(the_input+i)->ready_queue_array = PCB_array;
	}
	//printf("\nP: %p, PCB: %p", (the_input+0)->ready_queue_array, PCB_array);
	//printf("\n%p, %p\n", (void *)(the_input+0), (void *)(the_input+1));

	pthread_t threads[num_cores];
	// Create array for schedule results

	// Pass the & reference to the_input to the threads so they can get to the memory 
	for (i=2; i<argc; ++i) {
		//create thread and call worker functions respectively
		if (strcmp(argv[i], "FCFS") == 0) {
			pthread_create(&threads[i-2], NULL, first_come_first_serve_worker, (void *)(the_input+(i-2)));
			//create thread and call first_come_first_serve_worker and pass &the_input
		} else if (strcmp(argv[i], "RR") == 0) {
			//create thread and call round_robin_worker and pass &the_input
		} else {
			printf("ERROR");
			// Free memory!!!!!!!!!!!!!!!!!!!!!!!!
			return 0;
		}
	}

	// For loop to join all threads together
	for (i=0;i<num_cores;++i) {
		rc = pthread_join(threads[i], &status); // can add &status as a second parameter
		if (rc) {
			printf("ERROR: Return code from pthread_join is: %d\n", rc);
			// Free memory!!!!!!!!!!!!!!!!!!!!!!!!
			exit(-1);
		}
	}

	printf("SIZE OF ARRAY: %zu\n", dyn_array_size(PCB_array));
	printf("NUM CORES: %d\n", num_cores);
	for (int i=0;i<num_cores;++i) {
		free((the_input+i)->results);
	}
	free(the_input);
	dyn_array_destroy(PCB_array);
	pthread_exit(NULL);
	destroy_mutex();
	return 0;
}

/*void free_memory(void) {
	free(the_input->results);
	free(the_input);
	dyn_array_destroy(PCB_array);
	destroy_mutex();
	pthread_exit(NULL);
}*/