#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <string.h>
#include <sys/shm.h>
#include <time.h>
 
#define SIZE 10
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
    int print_size;
    int thread_num;
    int printID;    
} print_req;

int shmid;
print_req *shm_buffer;
key_t key = 5112;

int buffer_index;
pthread_mutex_t buffer_mutex;
int job_count;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//SEMAPHORE IMPLEMENTATION

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

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void insertbuffer(print_req *print_req_ptr) {
    if (buffer_index < SIZE) {
        printf("\nPROD %d: -- Insert: %d.%d at index: %d  Size: %d\n", print_req_ptr->thread_num, print_req_ptr->thread_num,print_req_ptr->printID, buffer_index, print_req_ptr->print_size);
        shm_buffer[buffer_index] = *print_req_ptr;
        buffer_index++;
    } else {
        printf("Buffer overflow\n");
    }    
}
 
void dequeuebuffer(int thread_numb) {
    if (job_count < 0) return;
    if (buffer_index > 0) { 
        --buffer_index;
        print_req process_print = shm_buffer[buffer_index];
        printf("\nCONS %d: -- \t\t\tPrint job size %d\n",thread_numb, shm_buffer[buffer_index].print_size);
        sleep(1/10);//(sleep will be proportional to process_print.print_size);        
        printf("CONS %d: -- \t\t\tDequeue: index: %d  ID: %d.%d  Size: %d\n\n", thread_numb, buffer_index, process_print.thread_num, process_print.printID, process_print.print_size);           
    } else {
        printf("Buffer underflow\n");
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
 
void *producer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    int i = 0;
    int rand_loops = rand() % 31;
    int print_num = 0;
    printf("\n**** NEW PROD THREAD %d :: LOOPS %d****\n", thread_numb, rand_loops);
    while (i++ < rand_loops) {
        int rand_size = rand() % 101;
        print_req request = {"10", rand_size, thread_numb, print_num};        
        print_req *ptr = &request;
        print_num++;
        wait_c_sem(&full); //WAIT CALL TO COUNTING_SEM FULL
        pthread_mutex_lock(&buffer_mutex); /* protecting critical section */
        job_count++;
        insertbuffer(ptr);        
        pthread_mutex_unlock(&buffer_mutex);
        post_c_sem(&empty); //POST CALL TO COUNTING_SEM EMPTY 
    }    
    pthread_exit(0);
}
 
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    int i = 0;
    sleep(1); //Prevents consumers exiting before processing any jobs
    while (1) {
        //If there are no more jobs, call sempost one last time to allow any waiting printers to exit. 
        if (job_count < 1) {  
            post_c_sem(&empty);
            break;
        }  
        printf("\nCons %d ENTER LOOPS\n", thread_numb);
        sleep(rand() % 4); //SLEEPS ADDED
        wait_c_sem(&empty); //WAIT CALL TO COUNTING_SEM EMPTY
        pthread_mutex_lock(&buffer_mutex);
        job_count--;       
        printf("\nJob Count = %d\n", job_count);
        dequeuebuffer(thread_numb);
        pthread_mutex_unlock(&buffer_mutex);
        post_c_sem(&full); //POST CALL TO COUNTING_SEM FULL      
    }
    printf("Cons %d EXIT\n", thread_numb);
    pthread_exit(0);
}
 
int main(int argc, int **argv) {

    buffer_index = 0;     

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
 
    init_c_sem(&full, SIZE); //INITIALIZED COUNTING_SEM FULL TO BUFFER SIZE
    init_c_sem(&empty, 0);   //INITIALIZED COUNTING_SEM EMPTY TO 0

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//BUFFER    

    int struct_arr_size = sizeof(struct print_req *) * SZ;

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



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
  
//THREADS

    pthread_mutex_init(&buffer_mutex, NULL);
    
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

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
 
//CLEANUP 

    for (i = 0; i < NUMB_THREADS; i++)
        pthread_join(thread[i], NULL);

    pthread_mutex_destroy(&buffer_mutex);

    destroy_c_sem(&empty); //DESTROY SEMS
    destroy_c_sem(&full);

    if (shmdt(shm_buffer) == -1){
            fprintf(stderr, "Unable to detach\n");
        }
 
    return 0;
}