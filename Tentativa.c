#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_QUEUE 3
#define CLOCK_SIZE 3

typedef struct Clock {
    int p[CLOCK_SIZE];
} Clock;

typedef struct Message {
    int pid;
    Clock clock;
    int source;
} Message;

typedef struct Task{
    int pid;
    Clock received;
}

typedef struct Queue{
    Task t[MAX_QUEUE];
    int front, rear, size;
    pthread_mutex_t lock;
    pthread_cond_t notFull, notEmpty;
}

Clock process_clock;
Queue input_queue;
Queue output_queue;

void Event(int pid, Clock *clock){
    clock->p[pid]++;
}

void create_task(Clock *clock, int pid, Task *newTask){
    newTask.pid = pid;
    for(int i = 0; i < CLOCK_SIZE; i++){
        newTask.received.p[i] = clock->p[i];
    }
}

void initQueue(Queue *q){
    q-> front = q->rear = q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->notFull, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
}

void enqueue(Queue *q, Task item){
    pthread_mutex_lock(&q->lock);
    while(q->size == MAX_QUEUE){
        pthread_cond_wait(&q->notFull, &q->lock);
    }
    q->t[q->rear] = item;
    q->rear = (q->rear + 1) % MAX_QUEUE;
    q->size++;
    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->lock);
}

void create_and_enqueue_task(int pid) {
    Task newTask;
    create_task(&process_clock, pid, &newTask);
    enqueue(&output_queue, newTask);
}

void dequeue(Queue *q, Task *item){
    pthread_mutex_lock(&q->lock);
    while(q->size == 0){
        pthread_cond_wait(&q->notEmpty, &q->lock);
    }
    *item = q->t[q->front];
    q->front = (q->front + 1) % MAX_QUEUE;
    q->size--;
    pthread_cond_signal(&q->notFull);
    pthread_mutex_unlock(&q->lock);
}

void process_task(Task *task){
    printf("Task recebida: %d %d %d\n",  task->received.p[0], task->received.p[1], task->received.p[2]);
    printf("Relogio do processo: %d %d %d\n", process_clock.p[0], process_clock.p[1], process_clock.p[2]);
    printf("Processando task...\n");
    sleep(2);
    for(int i = 0; i < CLOCK_SIZE; i++){
        if(task->received.p[i] > process_clock.p[i]){
            process_clock.p[i] = task->received.p[i];
        }
    }
    process_clock.p[task->pid]++;
    printf("Relogio do processo atualizado: %d %d %d\n", process_clock.p[0], process_clock.p[1], process_clock.p[2]);
}

void send_message(int pid, int dest, Clock *clock){
    MPI_Send(&clock, sizeof(Clock), MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    printf("Processo %d enviou para o processo %d: Clock(%d, %d, %d)\n", pid, dest, clock->p[0], clock->p[1], clock->p[2]));
}

void receive_message(int pid, Clock *clock, int source){
    Clock received;
    MPI_Recv(&received, sizeof(Clock), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    for(int i = 0; i < CLOCK_SIZE; i++){
        if(received.p[i] > clock->p[i]){
            clock->p[i] = received.p[i];
        }
    }
    printf("Processo %d recebeu do processo %d: Clock(%d, %d, %d)\n", pid, source, clock->p[0], clock->p[1], clock->p[2]);
}

void* input_thread(void *arg) {
    int pid = *((int*)arg);
    int source = *((int*)arg + 1);
    while (1) {
        Clock received_clock;
        MPI_Recv(&received_clock, sizeof(Clock), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        Task newTask;
        create_task(&received_clock, pid, &newTask);
        enqueue(&input_queue, newTask);
    }
    return NULL;
}

void* process_thread(void *arg) {
    int pid = *((int*)arg);
    int *thread_args = (int*)arg;
    while (1) {
        Task task;
        dequeue(&input_queue, &task);
        printf("Processo %d processou uma task.\n", pid);
        process_task(&task);
        create_and_enqueue_task(pid);
    }
    return NULL;
}

void* output_thread(void *arg) {
    int *thread_args = (int*)arg;
    int pid = thread_args[0];
    int dest = thread_args[1];
    while (1) {
        Task task;
        dequeue(&output_queue, &task);
        printf("Processo %d enviou mensagem MPI com base em uma task.\n", pid);
        send_message(pid, dest, &task.received);
    }
    return NULL;
}

void process0(){
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    pthread_t input_thread, process_thread, output_thread;
    Clock clock = {{0, 0, 0}};

    int thread_args[] = {my_rank, 1};  // Adjust destination accordingly
    pthread_create(&input_thread, NULL, input_thread, thread_args);

    int process_args[] = {my_rank};
    pthread_create(&process_thread, NULL, process_thread, process_args);

    int output_args[] = {my_rank, 1};  // Adjust destination accordingly
    pthread_create(&output_thread, NULL, output_thread, output_args);

    // Operações do processo 0
    send_message(0, &clock, 1);
    receive_message(0, &clock, 1);

    send_message(0, &clock, 2);
    receive_message(0, &clock, 2);

    send_message(0, &clock, 1);
    receive_message(0, &clock, 1);

    printf("Processo %d, Clock troca com o processo 1: (%d, %d, %d)\n", my_rank, clock.p[0], clock.p[1], clock.p[2]);

    // Aguardar o término das threads (o exemplo usa um loop infinito para simulação)
    pthread_join(input_thread, NULL);
    pthread_join(process_thread, NULL);
    pthread_join(output_thread, NULL);
    
}

void process1(){
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    pthread_t input_thread, process_thread, output_thread;
    Clock clock = {{0, 0, 0}};

    int thread_args[] = {my_rank, 0};  // Adjust destination accordingly
    pthread_create(&input_thread, NULL, input_thread, thread_args);

    int process_args[] = {my_rank};
    pthread_create(&process_thread, NULL, process_thread, process_args);

    int output_args[] = {my_rank, 0};  // Adjust destination accordingly
    pthread_create(&output_thread, NULL, output_thread, output_args);

    // Operações do processo 1
    send_message(1, &clock, 0);
    receive_message(1, &clock, 0);

    receive_message(1, &clock, 0);

    // Aguardar o término das threads (o exemplo usa um loop infinito para simulação)
    pthread_join(input_thread, NULL);
    pthread_join(process_thread, NULL);
    pthread_join(output_thread, NULL);
}

void process2(){
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    pthread_t input_thread, process_thread, output_thread;
    Clock clock = {{0, 0, 0}};

    int thread_args[] = {my_rank, 0};  // Adjust destination accordingly
    pthread_create(&input_thread, NULL, input_thread, thread_args);

    int process_args[] = {my_rank};
    pthread_create(&process_thread, NULL, process_thread, process_args);

    int output_args[] = {my_rank, 0};  // Adjust destination accordingly
    pthread_create(&output_thread, NULL, output_thread, output_args);

    // Operações do processo 2
    send_message(2, &clock, 0);
    receive_message(2, &clock, 0);

    // Aguardar o término das threads (o exemplo usa um loop infinito para simulação)
    pthread_join(input_thread, NULL);
    pthread_join(process_thread, NULL);
    pthread_join(output_thread, NULL);
}

int main(void) {
    int my_rank;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if (my_rank == 0) {
        process0();
    } else if (my_rank == 1) {
        process1();
    } else if (my_rank == 2) {
        process2();
    }
    MPI_Finalize();
    return 0;
}