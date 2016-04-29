#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <semaphore.h>

#define DATA_SIZE 256
#define BUFF_SIZE 4096
#define TEN_MILLION 10000000L

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
    const char *const p_shm_key = "/OS_SHM";
    const char *const p_sem_full_key = "/OS_SEM_FULL";
    const char *const p_sem_empty_key = "/OS_SEM_EMPTY"

    /*
    * MESSAGE QUEUE SECTION
    **/

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

                memcpy(consumer_msg_queue + i, msgcontent, DATA_SIZE);
            }

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

                if (mq_send(msg_queue_id, writecontent, sizeof(writecontent), msgprio) == -1)
                {
                    perror("msgsnd error");
                    fprintf(stderr,"msgsnd failed ");
                    exit(EXIT_FAILURE);
                }
                mq_getattr(msg_queue_id, &msgq_attr);
            }

            w = waitpid(pid, NULL, 0);
            printf("\nDONE MQUEUE\n");
            if (w == -1) { perror("waitpid"); exit(EXIT_FAILURE); }
    }

    mq_unlink(p_msq_key);
    mq_close(msg_queue_id);

    /*
    * PIPE SECTION
    **/
    // int pfd[2];                             /* Pipe file descriptors */
    // char buf[DATA_SIZE];
    // ssize_t numRead;
    // char consumer_pipe[BUFF_SIZE] = {0};


    // if (pipe(pfd) == -1) return -1; //create the pipe

    // switch (pid = fork()) {
    //     case -1:
    //         return -1;

    //     case 0:             /* Child  - reads from pipe */
    //         if (close(pfd[1]) == -1)            /* Write end is unused */
    //             return -1;

    //         for (int i=0;i<BUFF_SIZE;i+=DATA_SIZE) {              /* Read data from pipe, echo on stdout */
    //             //printf("\nREAD: %d", i);
    //             numRead = read(pfd[0], buf, DATA_SIZE);
    //             if (numRead == -1)
    //                 return -1;
    //             if (numRead == 0)
    //                 break;                      /* End-of-file */
    //             memcpy(consumer_pipe + i, buf, DATA_SIZE);
    //         }

    //         if (close(pfd[0]) == -1)
    //             return -1;

    //         for (int i=0; i<BUFF_SIZE; ++i) {
    //             if (memcmp(&ground_truth[i], &consumer_pipe[i], sizeof(char)) != 0) {
    //                 printf("\nBuffers DO NOT match... at index: %d\n\nInput: %c\nOutput: %c\n", i, ground_truth[i], consumer_pipe[i]);
    //                 exit(1);
    //             }
    //         }
    //         printf("\nBuffers DO match!\n");

    //         _exit(EXIT_SUCCESS);

    //     default:            /* Parent - writes to pipe */
    //         if (close(pfd[0]) == -1)            /* Read end is unused */
    //             return -1;

    //         for (int i=0; i<BUFF_SIZE; i+=DATA_SIZE) {
    //             //printf("\nWRITE: %d", i);
    //             if (write(pfd[1], producer_buffer + i, DATA_SIZE) != DATA_SIZE)
    //                 return -1;
    //             sleep(1);
    //         }

    //         if (close(pfd[1]) == -1)            /* Child will see EOF */
    //             return -1;

    //         w = waitpid(pid, NULL, 0);
    //         printf("\nDONE PIPE\n");
    //         if (w == -1) { perror("waitpid"); exit(EXIT_FAILURE); } /* Wait for child to finish */
    // }

    /*
    * SHARED MEMORY AND SEMAPHORE SECTION
    **/
    char consumer_shm[BUFF_SIZE] = {0};
    int shm_fd;
    void *ptr;
    sem_t *semlock;
    //int num;

    struct timespec sleeptime;
 
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = TEN_MILLION;

    /* create the shared memory segment */
    shm_fd = shm_open(p_shm_key, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0)
    {
        perror("In shm_open()");
        exit(1);
    }

    /* configure the size of the shared memory segment */
    ftruncate(shm_fd,DATA_SIZE);

    /* now map the shared memory segment in the address space of the process */
    ptr = mmap(0,DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        printf("Map failed\n");
        return -1;
    }

    semlock = sem_open(p_sem_key, O_CREAT, S_IRUSR | S_IWUSR, 1);

    switch(pid = fork()) {
        case -1:
            return -1;

        case 0:
            // loop through and read all shm 
            for (int i=0;i<BUFF_SIZE; i+=DATA_SIZE) {
                //printf("\nSEM: %d", sem_getvalue(semlock, &num));
                sem_wait(semlock);
                memcpy(consumer_shm + i, ptr, DATA_SIZE);
                printf("\nREAD: %d",i);
                sem_post(semlock);
                //nanosleep(&sleeptime, NULL);
                sleep(1);
            }

            for (int i=0; i<BUFF_SIZE; ++i) {
                if (memcmp(&ground_truth[i], &consumer_shm[i], sizeof(char)) != 0) {
                    printf("\nBuffers DO NOT match... at index: %d\n\nInput: %c\nOutput: %c\n", i, ground_truth[i], consumer_shm[i]);
                    exit(1);
                }
            }
            printf("\nBuffers DO match!\n");

            if (shm_unlink(p_shm_key) != 0) {
                perror("In shm_unlink()");
                exit(1);
            }
            /**
             * Semaphore Close: Close a named semaphore
             */
            if (sem_close(semlock) < 0 ) {
                perror("sem_close");
            }

            /**
             * Semaphore unlink: Remove a named semaphore  from the system.
             */
            if (sem_unlink(p_sem_key) < 0 ) {
                perror("sem_unlink");
            }

            _exit(EXIT_SUCCESS);

        default:
            // loop through and write all shm
            for (int i=0; i<BUFF_SIZE; i+=DATA_SIZE) {
                sem_wait(semlock);
                memcpy(ptr, producer_buffer + i, DATA_SIZE);
                printf("\nWRITE: %d", i);
                sem_post(semlock);
                //nanosleep(&sleeptime, NULL);
                sleep(1);
            }

            w = waitpid(pid, NULL, 0);
            printf("\nDONE SHM\n");
            if (w == -1) { perror("waitpid"); exit(EXIT_FAILURE); }/* Wait for child to finish */
    }


    return 0;
}
