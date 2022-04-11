#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <string.h>
#include <sys/shm.h>
#include <time.h>
 
#define SIZE 3
#define NUMB_THREADS 6
#define PRODUCER_LOOPS 2
#define SHMSZ 1000
#define SZ 30

/*
EVA CURRY
CMSC312 - ASSIGNMENT 3
*/

typedef struct{
    char print[100];
    int num;
} print_req;

int shmid;
print_req *shm_buffer;
key_t key = 5112;

int buffer_index;
pthread_mutex_t buffer_mutex;

/************************
Correct Implementation
************************/

//counting sem structure
struct counting_sem {
int val;
sem_t gate;
sem_t mutex;
};

struct counting_sem empty;
struct counting_sem full;

int min(int a, int b){
    return (a>b)?b:a;
}

//Initialize counting sem fields 
void init_c_sem (struct counting_sem *ptr, int n) {
    ptr->val = n; 
    sem_init(&ptr->gate, 0, min(1, n));
    sem_init(&ptr->mutex, 0, 1);
}

//"Pc" wait function for counting semaphore
void wait_c_sem(struct counting_sem *ptr) {
    sem_wait(&ptr->gate);
    sem_wait(&ptr->mutex);
    ptr->val = ptr->val - 1;
    if (ptr->val > 0) {
        sem_post(&ptr->gate);
        }
    sem_post(&ptr->mutex);
}

//"Vc" post function for counting semaphore
void post_c_sem(struct counting_sem *ptr) {
    sem_wait(&ptr->mutex);
    ptr->val = ptr->val + 1;
    if (ptr->val == 1){  
        sem_post(&ptr->gate); 
        }
    sem_post(&ptr->mutex);
}

//Destroy binary semaphores
void destroy_c_sem(struct counting_sem *ptr) { 
    sem_destroy(&ptr->mutex); 
    sem_destroy(&ptr->gate);
}

/************************/
 
void insertbuffer(print_req *req_ptr, int thread_numb) {
    if (buffer_index < SIZE) {
        shm_buffer[buffer_index++] = *req_ptr;
        printf("Buffer Index = %d\n", buffer_index);
        //PRINT STATEMENT MOVED TO CRITICAL SECTION FOR EASIER ANALYSIS
        printf("Producer %d added %d to buffer\n", thread_numb, req_ptr->num);
    } else {
        printf("Buffer overflow\n");
    }    
}
 
void dequeuebuffer(int thread_numb) {
    if (buffer_index > 0) { 
        --buffer_index;
        printf("Buffer Index = %d\n", buffer_index);
        //PRINT STATEMENT MOVED TO CRITICAL SECTION FOR EASIER ANALYSIS
        printf("Consumer %d dequeue %d from buffer\n", thread_numb, shm_buffer[buffer_index].num);        
    } else {
        printf("Buffer underflow\n");
    }
}
 
void *producer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    int i = 0;

    while (i++ < PRODUCER_LOOPS) {
        int value = rand() % 101;
        print_req request = {"10", value};
        print_req *ptr = &request;
        sleep(rand() % 10); 
            
/************************/
       wait_c_sem(&full); //WAIT CALL TO COUNTING_SEM FULL
/************************/

        pthread_mutex_lock(&buffer_mutex); /* protecting critical section */
        insertbuffer(ptr, thread_numb);
        pthread_mutex_unlock(&buffer_mutex);
           
/************************/
        post_c_sem(&empty); //POST CALL TO COUNTING_SEM EMPTY
/************************/
 
    }
    pthread_exit(0);
}
 
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    int i=0;
    while (i++ < PRODUCER_LOOPS) {
        sleep(rand() % 4); //SLEEPS ADDED
/************************/
        wait_c_sem(&empty); //WAIT CALL TO COUNTING_SEM EMPTY
/************************/

        pthread_mutex_lock(&buffer_mutex);
        dequeuebuffer(thread_numb);
        pthread_mutex_unlock(&buffer_mutex);
                
/************************/
        post_c_sem(&full); //POST CALL TO COUNTING_SEM FULL
/************************/
      
    }
    pthread_exit(0);
}
 
int main(int argc, int **argv) {

    buffer_index = 0; 
    pthread_mutex_init(&buffer_mutex, NULL);
    int struct_arr_size = sizeof(struct print_req *) * SZ;
    
/************************/
    init_c_sem(&full, SIZE); //INITIALIZED COUNTING_SEM FULL TO BUFFER SIZE
    init_c_sem(&empty, 0);   //INITIALIZED COUNTING_SEM EMPTY TO 0
/************************/  

//SETTING UP SHARED MEMORY FOR BUFFER    
    // Create Shared Buffer
    if( (shmid = shmget(key, struct_arr_size, IPC_CREAT | 0600)) < 0 )
    {
        perror("shmget");
        exit(1);
    }

    // Attach to Shared Buffer 
    if( (shm_buffer = (print_req*)shmat(shmid, (void*) 0, 0)) == (void*) -1 )
    {
        perror("shmat");
        exit(1);
    }

    pthread_t thread[NUMB_THREADS];
    int thread_numb[NUMB_THREADS];
    int i;
    for (i = 0; i < NUMB_THREADS; ) {
        thread_numb[i] = i;
        pthread_create(thread + i, // pthread_t *t
                       NULL, // const pthread_attr_t *attr
                       producer, // void *(*start_routine) (void *)
                       thread_numb + i);  // void *arg
        i++;
        thread_numb[i] = i;
        // playing a bit with thread and thread_numb pointers...
        pthread_create(&thread[i], // pthread_t *t
                       NULL, // const pthread_attr_t *attr
                       consumer, // void *(*start_routine) (void *)
                       &thread_numb[i]);  // void *arg
        i++;
    }
 
    for (i = 0; i < NUMB_THREADS; i++)
        pthread_join(thread[i], NULL);

    pthread_mutex_destroy(&buffer_mutex);

/************************/
    destroy_c_sem(&empty); //DESTROY SEMS
    destroy_c_sem(&full);
/************************/   

    if (shmdt(shm_buffer) == -1){
            fprintf(stderr, "Unable to detach\n");
        }
 
    return 0;
}