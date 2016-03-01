// Put the code for your analysis program here!
#include "../include/processing_scheduling.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	if(argc < 3) { // Error check parameters
		printf("\nPlease use format './analysis (filename) FCFS RR etc...'\n");
		return -1;
	}

	int i, rc; // initiate counter and rc 
	int num_cores = argc-2; // number of cores 
	void *status; // used for joining threads together
	for (i=2; i<argc; ++i) {
		if(strcmp(argv[i], "RR") != 0 && strcmp(argv[i], "FCFS") != 0) { // Error checking that each core is FCFS or RR, nothing else
			printf("\nA core is labeled incorrectly (must be FCFS or RR)...\n");
			return -1;
		}
		printf("CORE STYLE: %s\n", argv[i]); // Print out each core for checking 
	}

	if (init_lock() != true) { // Check that the global mutex is created successfully
		printf("\nCould not create the global mutex...\n");
		// Free threads 
		pthread_exit(NULL);
		return -1;
	}

	dyn_array_t *PCB_array = load_process_control_blocks(argv[1]); // create an empty dynamic array that will hold PCB's, initialize start to 0
	if (PCB_array == NULL || dyn_array_empty(PCB_array)) { // Error check PCB Array malloc and that the array is populated
		printf("\nProblem mallocing memory for the PCB array or PCB_array is empty...\n");
		// Free memory so far 
		pthread_exit(NULL);
		destroy_mutex();
		return -1;
	}

	WorkerInput_t *the_input = (WorkerInput_t *)malloc(sizeof(WorkerInput_t)*num_cores); // Create worker structs to hold PCB array and result for each core
	if (the_input == NULL) { // Error check mallocing memory for WorkerInput array
		printf("\nProblem mallocing memory for the WorkerInput Array...\n");
		// Free memory so far
		dyn_array_destroy(PCB_array);
		pthread_exit(NULL);
		destroy_mutex();
		return -1;
	}

	// Malloc memory for all of the results structs in each WorkerInput struct
	for (i=0;i<num_cores;++i) {
		(the_input+i)->results = (ScheduleResult_t *)malloc(sizeof(ScheduleResult_t)); // Malloc memory for results struct
		if ((the_input+i)->results == NULL) { // Check that each Schedule Result struct is malloced properly
			printf("\nProblem mallocing memory for the results struct...\n");
			// Free all memory malloced so far
			for (int j=0;j<i;++j) { // Free all results structs malloced so far in the case of an error
				free((the_input+j)->results);
			}
			free(the_input);
			dyn_array_destroy(PCB_array);
			pthread_exit(NULL);
			destroy_mutex();
			return -1;
		}
		(the_input+i)->ready_queue_array = PCB_array; // Pass PCB array to the ready q attribute of each Worker Input object 
	}

	// Create each tread and run until completion, check for errors with the core names incase something slipped through
	pthread_t threads[num_cores]; // Create array for schedule results
	for (i=2; i<argc; ++i) {
		//create thread and call worker functions respectively
		if (strcmp(argv[i], "FCFS") == 0) {
			//create thread and call first_come_first_serve_worker and pass &the_input
			pthread_create(&threads[i-2], NULL, first_come_first_serve_worker, (void *)(the_input+(i-2))); // Pass the & reference to the_input to the threads so they can get to the memory 
		} else if (strcmp(argv[i], "RR") == 0) {
			//create thread and call round_robin_worker and pass &the_input
			pthread_create(&threads[i-2], NULL, round_robin_worker, (void *)(the_input+(i-2))); // Pass the & reference to the_input to the threads so they can get to the memory 
		} else { // In the event that an invalid core slipped through the first check 
			printf("\nCannot create thread, an input is labeled incorrectly...(This error should never happen)...\n");
			// Free all memory made so far 
			for (int i=0;i<num_cores;++i) {
				free((the_input+i)->results);
			}
			free(the_input);
			dyn_array_destroy(PCB_array);
			pthread_exit(NULL);
			destroy_mutex();
			return -1;
		}
	}

	// Join all threads back together
	for (i=0;i<num_cores;++i) {
		rc = pthread_join(threads[i], &status); // can add &status as a second parameter
		if (rc) { // Checks that the thread was joined properly
			printf("\nERROR: Return code from pthread_join is: %d...\n", rc);
			// Free all memory made so far 
			for (int i=0;i<num_cores;++i) {
				free((the_input+i)->results);
			}
			free(the_input);
			dyn_array_destroy(PCB_array);
			pthread_exit(NULL);
			destroy_mutex();
			exit(-1);
		}
	}

	// Free all memory and return 0
	for (i=0;i<num_cores;++i) {
		free((the_input+i)->results);
	}
	free(the_input);
	dyn_array_destroy(PCB_array);
	pthread_exit(NULL);
	destroy_mutex();
	return 0;
}