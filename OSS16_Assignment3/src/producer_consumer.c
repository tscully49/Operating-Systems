#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/stat.h>

#define DATA_SIZE 256
#define BUFF_SIZE 4096

int main(void) {
    // seed the random number generator
    srand(time(NULL));

    // Parent and Ground Truth Buffers
    char ground_truth[BUFF_SIZE]    = {0};  // used to verify
    char producer_buffer[BUFF_SIZE] = {0};  // used by the parent
    pid_t pid, w;

    // init the ground truth and parent buffer
    for (int i = 0; i < BUFF_SIZE; ++i) {
        producer_buffer[i] = ground_truth[i] = rand() % 256;
    }

    // System V IPC keys for you to use
    /*const key_t s_msq_key = 1337;  // used to create message queue ipc
    const key_t s_shm_key = 1338;  // used to create shared memory ipc
    const key_t s_sem_key = 1339;*/ // used to create semaphore ipc
    
    // POSIX IPC keys for you to use
    const char *const p_msq_key = "OS_MSG";
    /*const char *const p_shm_key = "OS_SHM";
    const char *const p_sem_key = "OS_SEM";*/

    /*
    * MESSAGE QUEUE SECTION
    **/

    unsigned int msgprio = 0;
    time_t currtime;
    //pid_t my_pid = getpid();
    char msgcontent[DATA_SIZE];
    char writecontent[DATA_SIZE];
    char consumer_msg_queue[BUFF_SIZE] = {0};
    int num_read;
    unsigned int sender;

    int msg_queue_id = mq_open(p_msq_key, O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG, NULL);
    if (msg_queue_id == -1) {
        perror("In mq_open()");
        exit(1);
    }

    switch(pid = fork()) {
        case -1:
            return -1;

        case 0: // child
            for (int i=0; i<BUFF_SIZE; i+=DATA_SIZE) {
                currtime = time(NULL);
                num_read = mq_receive(msg_queue_id, msgcontent, DATA_SIZE, &sender);
                if (num_read == -1) {
                    perror("In mq_receive()");
                    exit(1);
                }
                memcpy(&consumer_msg_queue[i], msgcontent, DATA_SIZE);
                //printf("\nReading from process %u (at %s).", pid, ctime(&currtime));
                printf("\nREAD: %s\n", msgcontent);
            }
            _exit(EXIT_SUCCESS);
            
        default: // parent 
            for (int i=0; i<BUFF_SIZE; i+=DATA_SIZE) {
                currtime = time(NULL);
                strncpy(writecontent, &producer_buffer[i], DATA_SIZE);
                if (mq_send(msg_queue_id, writecontent, strlen(writecontent)+1, msgprio) == -1)
                {
                    perror("msgsnd error");
                    fprintf(stderr,"msgsnd failed ");
                    exit(EXIT_FAILURE);
                }
                //printf("\nHello from process %u (at %s).", my_pid, ctime(&currtime));
                printf("\nWRITE: %s\n", writecontent);
            }
            w = waitpid(pid, NULL, 0);
            if (w == -1) { perror("waitpid"); exit(EXIT_FAILURE); }
    }

    /////////////////////////////////////////////////////// CLOSE THE MSG QUEUE

    for (int i=0; i<BUFF_SIZE; i+=DATA_SIZE) {
        if (memcmp(&(ground_truth) + i, &(consumer_msg_queue) + i, DATA_SIZE) != 0) {
            printf("\nBuffers DO NOT match... at index: %d\n", i);
            exit(1);
        }
    }
    printf("\nBuffers DO match!");

    /*
    * PIPE SECTION
    **/

    /*
    * SHARED MEMORY AND SEMAPHORE SECTION
    **/

    return 0;
}
