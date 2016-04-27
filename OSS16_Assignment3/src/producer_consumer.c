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
    const char *const p_msq_key = "/OS_MSG";
    /*const char *const p_shm_key = "/OS_SHM";
    const char *const p_sem_key = "/OS_SEM";*/

    /*
    * MESSAGE QUEUE SECTION
    **/
    mq_unlink(p_msq_key);

    unsigned int msgprio = 1;
    char consumer_msg_queue[BUFF_SIZE] = {0};
    //char empty_string[DATA_SIZE] = {0};
    int num_read;
    struct mq_attr msgq_attr;

    mqd_t msg_queue_id = mq_open(p_msq_key, O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG, NULL);
    if (msg_queue_id == (mqd_t)-1) {
        perror("In mq_open()");
        exit(1);
    }

    mq_getattr(msg_queue_id, &msgq_attr);
    printf("Queue \"%s\":\n\t- stores at most %ld messages\n\t- large at most %ld bytes each\n\t- currently holds %ld messages\n", p_msq_key, msgq_attr.mq_maxmsg, msgq_attr.mq_msgsize, msgq_attr.mq_curmsgs);

    switch(pid = fork()) {
        case -1:
            return -1;

        case 0: // child
            for (int i=0; i<BUFF_SIZE; i+=DATA_SIZE) {
                char msgcontent[10000] = {0};
                mq_getattr(msg_queue_id, &msgq_attr);
                while (msgq_attr.mq_curmsgs == 0) {
                    sleep(1);
                    mq_getattr(msg_queue_id, &msgq_attr);
                }

                num_read = mq_receive(msg_queue_id, msgcontent, 10000, NULL);
                mq_getattr(msg_queue_id, &msgq_attr);
                if (num_read == -1) {
                    perror("In mq_receive()");
                    exit(1);
                }
                printf("\ni: %d\n", i);

                memcpy(consumer_msg_queue + i, msgcontent, DATA_SIZE);
            }

            printf("\n\nGOT HERE\n\n");
            for (int i=0; i<BUFF_SIZE; ++i) {
                if (memcmp(&producer_buffer[i], &consumer_msg_queue[i], sizeof(char)) != 0) {
                    printf("\nBuffers DO NOT match... at index: %d\n\nInput: %c\nOutput: %c\n", i, producer_buffer[i], consumer_msg_queue[i]);
                    if (producer_buffer[i] == 0) {
                        printf("\nPRODUCER is \\0\n");
                    } else if (consumer_msg_queue[i] == 0) {
                        printf("\nCONSUMER is \\0\n");
                    }
                    mq_unlink(p_msq_key);
                    mq_close(msg_queue_id);
                    exit(1);
                }
            }
            printf("\nBuffers DO match!\n");
            mq_unlink(p_msq_key);
            mq_close(msg_queue_id);

            _exit(EXIT_SUCCESS);
            
        default: // parent 
            for (int i=0; i<BUFF_SIZE; i+=DATA_SIZE) {
                char writecontent[DATA_SIZE] = {0};
                mq_getattr(msg_queue_id, &msgq_attr);
                while (msgq_attr.mq_curmsgs == 10) {
                    sleep(1);
                    mq_getattr(msg_queue_id, &msgq_attr);
                }
                memcpy(writecontent, producer_buffer + i, DATA_SIZE);
                printf("\n\nLEN: %d", sizeof(writecontent));

                if (mq_send(msg_queue_id, writecontent, sizeof(writecontent), msgprio) == -1)
                {
                    perror("msgsnd error");
                    fprintf(stderr,"msgsnd failed ");
                    exit(EXIT_FAILURE);
                }
                mq_getattr(msg_queue_id, &msgq_attr);
                printf("\nADD: %ld\n", msgq_attr.mq_curmsgs);
            }

            w = waitpid(pid, NULL, 0);
            printf("\nDONE\n");
            if (w == -1) { perror("waitpid"); exit(EXIT_FAILURE); }
    }

    mq_unlink(p_msq_key);
    mq_close(msg_queue_id);

    /*
    * PIPE SECTION
    **/
    

    /*
    * SHARED MEMORY AND SEMAPHORE SECTION
    **/

    return 0;
}
