
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <string.h>
#include <sys/shm.h>
#include <time.h>
#include <signal.h>
#include <wait.h>

#define SIZE 30
#define SHMSZ 1000
#define MAX 100

/*
EVA CURRY
CMSC312 - ASSIGNMENT 3
*/

void create_log(int type, int id, int size);

//DEFINE PRINT REQUEST
typedef struct{
    int print_size;
    int process_num;
    int printID;
} print_req;

//THREAD/PROCESS COMMUNICATION
typedef struct{
    int buffer_index;
    int job_count;
    int total_jobs;
    int prod_count;
    int insert_count;
    int dequeue_count;
    sem_t lock;
} bufferdata;

//COUNTING SEMAPHORE STRUCT
struct counting_sem {                       
    int val;
    sem_t gate;
    sem_t mutex;
    };

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//GLOBAL VARIABLES
int shmid_buff;
int shmid_data;
int shmid_semfull;
int shmid_semempty;
print_req *shm_buffer;
bufferdata *shm_data;
struct counting_sem *shm_semfull; //typedef later
struct counting_sem *shm_semempty;
key_t buff_key = 5112;
key_t full_key = 5113;
key_t empty_key = 5114;
key_t data_key = 5115;
int parent;
int flag;
int num_prod;
int num_cons;
pthread_t *thread_ptr;
char line[MAX];

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//SEMAPHORE IMPLEMENTATION
int min(int a, int b){
    return (a>b)?b:a;
}

//Initialize counting sem fields 
void init_c_sem (struct counting_sem *ptr, int n) {
    ptr->val = n; 
    sem_init(&ptr->gate, 1, min(1, n)); //something about changing second arg
    sem_init(&ptr->mutex, 1, 1);
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

//INSERT FUNCTION
void insertbuffer(print_req *print_req_ptr) {
    if (shm_data->buffer_index < SIZE) {
        printf("\nPROD %d: -- Insert: %d.%d at index: %d  Size: %d bytes\n", print_req_ptr->process_num, print_req_ptr->process_num,print_req_ptr->printID, shm_data->buffer_index, print_req_ptr->print_size);
        create_log(1, print_req_ptr->process_num, print_req_ptr->print_size);
        shm_buffer[shm_data->buffer_index] = *print_req_ptr;
        shm_data->buffer_index++;
        shm_data->insert_count++;
    } else {
        printf("Buffer overflow\n");
    }    
}

//DEQUEUE FUNCTION 
print_req dequeuebuffer(int thread_numb) {
    if (shm_data->buffer_index > 0) {
        --shm_data->buffer_index;
        shm_data->job_count--;
        shm_data->dequeue_count++;
        print_req process_print = shm_buffer[shm_data->buffer_index];sleep(1/10);//(sleep will be proportional to process_print.print_size); 
        printf("\t\tCONS %d: --  Dequeue: index: %d  ID: %d.%d  Size: %d  Jobs Pending = %d\n\n", thread_numb, shm_data->buffer_index, process_print.process_num, process_print.printID, process_print.print_size, shm_data->job_count);
        create_log(2, thread_numb, process_print.print_size);       
        return process_print;
    } else {
        printf("Buffer underflow\n");
    }
}

//CONSUMER FUNCTION 
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    int i = 0;
    while (1) {
    	sleep(1);
        //if (shm_data->job_count > 0){
            print_req print;
            						pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
            wait_c_sem(shm_semempty); //WAIT CALL TO COUNTING_SEM EMPTY
            sem_wait(&shm_data->lock);
            if (shm_data->job_count > 0){           
            	print = dequeuebuffer(thread_numb);
            }
            sem_post(&shm_data->lock);
            post_c_sem(shm_semfull); //POST CALL TO COUNTING_SEM FULL 
            						pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
            sleep((print.print_size/100)); //Simulate print process after removing print data from queue
        //}        
        //If there are no more jobs, call sempost one last time to allow any waiting printers to exit one by one. 
        if (shm_data->job_count < 1) {
            post_c_sem(shm_semempty);
            break;
        }
        
    }
    printf("Cons %d EXIT job count = %d\n", thread_numb, shm_data->job_count);
    pthread_exit(0);
}

//TERMINATION FUNCTION
void term() {
    while(wait(NULL) > 0);
    int i = 0;
    for (i = 0; i < num_cons; i++){
        pthread_join(thread_ptr[i],NULL);
    }
    
    if (shm_data->insert_count == shm_data->dequeue_count) printf("\n\nSuccess\n");
    else printf("Error :: Print Jobs Pending");
    printf("Total Jobs \t\t\t= %d\n", shm_data->total_jobs);
    printf("Total jobs added to queue  \t= %d\nTotal jobs removed from queue \t= %d\n\n", shm_data->insert_count, shm_data->dequeue_count);    
    
    
    if (shmdt(shm_buffer) == -1){
            fprintf(stderr, "Unable to detach\n");
        }
    if (shmdt(shm_data) == -1){
            fprintf(stderr, "Unable to detach\n");
        }
    if (shmdt(shm_semfull) == -1){
            fprintf(stderr, "Unable to detach\n");
        }
    if (shmdt(shm_semempty) == -1){
            fprintf(stderr, "Unable to detach\n");
        }
    shmctl(shmid_buff, IPC_RMID, NULL);
    shmctl(shmid_data, IPC_RMID, NULL);
    shmctl(shmid_semfull, IPC_RMID, NULL);
    shmctl(shmid_semempty, IPC_RMID, NULL);
    destroy_c_sem(shm_semempty); //DESTROY SEMS
    destroy_c_sem(shm_semfull);
    sem_destroy(&shm_data->lock);
    printf("***Termination Successful***\nGoodbye\n\n");
    exit(0);
}

//SIGNAL HANDLER
void sig_handler(){
    //flag = 0;
    if(getpid() != parent){
        exit(1);
    } else {
        int i;
	   for (i = 0; i < num_cons; i++){
	        pthread_cancel(thread_ptr[i]);
	   }
	 term();
    }   
}

//////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////// 

//CREATE REPORT
void add_log(int type, char *line){
    FILE *data;
    if(type == 1)data = fopen ("proddata.txt","a");
    else data = fopen ("consdata.txt", "a");
    if (!data) perror("fopen");
    fprintf(data, "%s\n", line);
    fclose(data);
}

void create_log(int type, int id, int size){
    const char *temp;
    temp = "";
    char *print_id;
    char *print_size;
    if (asprintf(&print_id, "%d", id) == -1) {
        perror("asprintf");
    } else if (asprintf(&print_size, "%d", size) == -1) {
        perror("asprintf");
    } else {
       strcat(strcpy(line, temp), print_id);
       strcat(line, ",");
       strcat(line, print_size);
       free(print_id);
       free(print_size);
    }
    add_log(type, line);    
}

void print_report(){
    FILE *prod_data;
    FILE *cons_data;
    prod_data = fopen ("proddata.txt", "r");
    cons_data = fopen ("consdata.txt", "r");
    
}

//////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////// 
 
int main(int argc, char **argv) {
	
    FILE *prod_data;
    FILE *cons_data;
    prod_data = fopen ("proddata.txt","w");
    if (!prod_data) perror("fopen");
    cons_data = fopen ("consdata.txt","w");
    if (!cons_data) perror("fopen");
    fclose(prod_data);
    fclose(cons_data);


    parent = getpid();
    flag = 1;
    signal(SIGINT, sig_handler);
    if (argc == 3){
        num_prod = atoi(argv[1]);
        num_cons = atoi(argv[2]);
    }
    else printf("invalid args\n");
    
//SHARED MEMORY VARS
    int buffer_max = sizeof(struct print_req *) * SIZE;
    int sem_size = sizeof(struct counting_sem *);
    int bufferdata_size = sizeof(struct bufferdata*);

//BUFFER    
    // Create Shared Buffer
    if( (shmid_buff = shmget(buff_key, buffer_max, IPC_CREAT | 0600)) < 0 ){
        perror("shmget");
        exit(1);
    }
    // Attach to Shared Buffer 
    if( (shm_buffer = (print_req*)shmat(shmid_buff, (void*) 0, 0)) == (void*) -1 ){
        perror("shmat");
        exit(1);
    }
//BOOK KEEPING
    if( (shmid_data = shmget(data_key, bufferdata_size, IPC_CREAT | 0600)) < 0 ){
        perror("shmget");
        exit(1);
    }
    // Attach to Shared Buffer 
    if( (shm_data = (bufferdata*)shmat(shmid_data, (void*) 0, 0)) == (void*) -1 ){
        perror("shmat");
        exit(1);
    }
//SEM FULL
    // Create Shared Buffer
    if( (shmid_semfull = shmget(full_key, sem_size, IPC_CREAT | 0600)) < 0 ){
        perror("shmget");
        exit(1);
    }
    // Attach to Shared Buffer 
    if( (shm_semfull = (struct counting_sem*)shmat(shmid_semfull, (void*) 0, 0)) == (void*) -1 ){
        perror("shmat");
        exit(1);
    }
//SEM EMPTY
    // Create Shared Buffer
    if( (shmid_semempty = shmget(empty_key, sem_size, IPC_CREAT | 0600)) < 0 ){
        perror("shmget");
        exit(1);
    }
    // Attach to Shared Buffer 
    if( (shm_semempty = (struct counting_sem*)shmat(shmid_semempty, (void*) 0, 0)) == (void*) -1 ){
        perror("shmat");
        exit(1);
    }

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//INITIALIZATION
    init_c_sem(shm_semfull, SIZE); //INITIALIZED COUNTING_SEM FULL TO BUFFER SIZE
    init_c_sem(shm_semempty, 0);   //INITIALIZED COUNTING_SEM EMPTY TO 0
    shm_data->buffer_index = 0;
    shm_data->insert_count = 0;
    shm_data->dequeue_count = 0;
    shm_data->prod_count = num_prod;
    sem_init(&shm_data->lock, 1, 1);

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
 
//THREADS
    
    pthread_t thread[num_cons];
    int thread_numb[num_cons];
    thread_ptr = thread;
    int i;
        for (i = 0; i < num_cons; i++) {
        thread_numb[i] = i;
        pthread_create(thread + i, // pthread_t *t
                      NULL, // const pthread_attr_t *attr
                      consumer, // void *(*start_routine) (void *)
                      thread_numb + i);  // void *arg
    }
    
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
 
//PROCESSES
    
    for (i = 0; i < num_prod; i++){
        if (fork() == 0){
            srand(time(0) + getpid());
            int procID = getpid();
            int rand_loops = rand() % 31 + 1;
            int print_num = 0;
            shm_data->job_count = shm_data->job_count + rand_loops;
            shm_data->total_jobs = shm_data->job_count;
            printf("\n**** NEW PROD THREAD %d :: LOOPS %d****\n", procID, rand_loops);
            int j = 0;
            while (j < rand_loops && flag == 1) {            	 
                int rand_size = rand() % (1000+1-100) + 100;
                print_req request = {rand_size, procID, print_num};        
                print_req *ptr = &request;
                print_num++;
                wait_c_sem(shm_semfull);
                sem_wait(&shm_data->lock);
                insertbuffer(ptr);
                sem_post(&shm_data->lock);
                post_c_sem(shm_semempty);
                sleep(rand() % 4);
                j++;
            }
            --shm_data->prod_count;
            exit(0);
        }
    }

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//FINISH
    
     
    term();
    return 0;
}
