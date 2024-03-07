#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <pthread.h>
#include <unistd.h>
#include <omp.h>

#define MAX_QUEUE 3
#define CLOCK_SIZE 3

typedef struct Clock {
    int p[CLOCK_SIZE];
} Clock;

typedef struct Task {
    int pid;
    Clock clock;
    int source;
} Task;

typedef struct Queue {
    Task t[MAX_QUEUE];
    int front, rear, size;
    pthread_mutex_t lock;
    pthread_cond_t notFull, notEmpty;
} Queue;

Queue input_queue;
Queue output_queue;

void initQueue(Queue *q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->notFull, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
}

void Event(int pid, Clock *clock) {
    clock->p[pid]++;
}

void enqueue(Queue *q, Task t) {
    pthread_mutex_lock(&q->lock);
    while (q->size == MAX_QUEUE) {
        pthread_cond_wait(&q->notFull, &q->lock);
    }
    q->t[q->rear] = t;
    q->rear = (q->rear + 1) % MAX_QUEUE;
    q->size++;
    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->lock);
}

void dequeue(Queue *q, Task *t) {
    pthread_mutex_lock(&q->lock);
    while (q->size == 0) {
        pthread_cond_wait(&q->notEmpty, &q->lock);
    }
    *t = q->t[q->front];
    q->front = (q->front + 1) % MAX_QUEUE;
    q->size--;
    pthread_cond_signal(&q->notFull);
    pthread_mutex_unlock(&q->lock);
}

void ReceiveAndEnqueue(int pid, Clock *clock, int source) {
    Clock received;
    MPI_Recv(&received, sizeof(Clock), MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("Processo %d recebeu do processo %d: Clock(%d, %d, %d)\n", pid, source, received.p[0], received.p[1], received.p[2]);
    Task newTask = {pid, received, source};
    enqueue(&input_queue, newTask);
}

void ProcessAndEnqueue(int pid, Clock *clock) {
    Task currentTask;
    dequeue(&input_queue, &currentTask);
    for (int i = 0; i < CLOCK_SIZE; i++) {
        if (currentTask.clock.p[i] > clock->p[i]) {
            clock->p[i] = currentTask.clock.p[i];
        }
    }
    clock->p[pid]++;
    printf("Processo %d recebeu do processo %d: Clock(%d, %d, %d)\n", pid, currentTask.source, clock->p[0], clock->p[1], clock->p[2]);
    Task nextTask = {pid, *clock, currentTask.source};
    enqueue(&output_queue, nextTask);
}

void ProcessEvent(int pid, Clock *clock) {
    Event(pid, clock);
    printf("Processo %d: Clock(%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

void createAndEnqueueTask(int pid, Clock *clock, int dest) {
    Task newTask = {pid, *clock, dest};
    enqueue(&output_queue, newTask);
}

void SendFromOutputQueue(int pid, Clock *clock, int dest) {
    Task currentTask;
    dequeue(&output_queue, &currentTask);
    MPI_Send(&currentTask.clock, sizeof(Clock), MPI_BYTE, dest, 0 , MPI_COMM_WORLD);
    printf("Processo %d enviou para o processo %d: Clock(%d, %d, %d)\n", pid, dest, currentTask.clock.p[0], currentTask.clock.p[1], currentTask.clock.p[2]);
}

void* input_thread(void* arg) {
    int pid = *((int*)arg);
    Clock clock = {0, 0, 0};
    ReceiveAndEnqueue(pid, &clock, pid);
    return NULL;
}

void* process_thread(void* arg) {
    int pid = *((int*)arg);
    Clock clock = {0, 0, 0};
    ProcessAndEnqueue(pid, &clock);
    return NULL;
}

void* output_thread(void* arg) {
    int pid = *((int*)arg);
    Clock clock = {0, 0, 0};
    SendFromOutputQueue(pid, &clock, (pid + 1) % 3);
    return NULL;
}


void process0(){
    Clock clock = {{0, 0, 0}};
    ProcessEvent(0, &clock);
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
    createAndEnqueueTask(0, &clock, 1);
    SendFromOutputQueue(0, &clock, 1);
    ReceiveAndEnqueue(0, &clock, 1);
    ProcessAndEnqueue(0, &clock);
    SendFromOutputQueue(0, &clock, 2);
    ReceiveAndEnqueue(0, &clock, 2);
    ProcessAndEnqueue(0, &clock);
    SendFromOutputQueue(0, &clock, 1);
    ProcessEvent(0, &clock);
    printf("Processo %d, Clock troca com o processo 1: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
}

void process1(){
    Clock clock = {{0, 0, 0}};
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 1, clock.p[0], clock.p[1], clock.p[2]);
    createAndEnqueueTask(1, &clock, 0);
    SendFromOutputQueue(1, &clock, 0);
    ReceiveAndEnqueue(1, &clock, 0);
    ProcessAndEnqueue(1, &clock);
    ReceiveAndEnqueue(1, &clock, 0);
    ProcessAndEnqueue(1, &clock);
}

void process2(){
    Clock clock = {{0, 0, 0}};
    ProcessEvent(2, &clock);
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 2, clock.p[0], clock.p[1], clock.p[2]);
    createAndEnqueueTask(2, &clock, 0);
    SendFromOutputQueue(2, &clock, 0);
    ReceiveAndEnqueue(2, &clock, 0);
    ProcessAndEnqueue(2, &clock);
}

int main(int argc, char *argv[]) {
    int provide; // Armazena o nível de suporte a threads do MPI
    int rank;    // Identificador do processo

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provide); // Inicializa o ambiente MPI
    // Verifica se o ambiente MPI foi inicializado corretamente
    if (provide != MPI_THREAD_MULTIPLE) {
        printf("Error: MPI_THREAD_MULTIPLE não suportado\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    initQueue(&input_queue);
    initQueue(&output_queue);

    int arg1 = rank;
    int arg2 = rank;
    int arg3 = rank;

    pthread_t input, process, output;
    pthread_create(&input, NULL, input_thread, &arg1);
    pthread_create(&process, NULL, process_thread, &arg2);
    pthread_create(&output, NULL, output_thread, &arg3);

    if(rank == 0) {
        process0();
    } else if(rank == 1) {
        process1();
    } else if(rank == 2) {
        process2();
    }

    pthread_join(input, NULL);
    pthread_join(process, NULL);
    pthread_join(output, NULL);

    MPI_Finalize();
    return 0;
}
