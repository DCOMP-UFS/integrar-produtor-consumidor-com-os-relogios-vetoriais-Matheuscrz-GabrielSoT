#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <pthread.h>
#include <unistd.h>
#include <omp.h>

#define MAX_QUEUE 3
#define CLOCK_SIZE 3
#define MESSAGE_SIZE 100

typedef struct Clock {
    int p[CLOCK_SIZE];
} Clock;

typedef enum {
    EVENT,
    SEND,
    RECEIVE
} MessageType;

typedef struct QueueItem {
    int pid;
    int target;
    MessageType type;
    char message[MESSAGE_SIZE];
} QueueItem;

typedef struct Queue {
    QueueItem items[MAX_QUEUE];
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

void enqueue(Queue *q, QueueItem item) {
    pthread_mutex_lock(&q->lock);
    while (q->size == MAX_QUEUE) {
        pthread_cond_wait(&q->notFull, &q->lock);
    }
    q->items[q->rear] = item;
    q->rear = (q->rear + 1) % MAX_QUEUE;
    q->size++;
    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->lock);
}

QueueItem dequeue(Queue *q) {
    pthread_mutex_lock(&q->lock);
    while (q->size == 0) {
        pthread_cond_wait(&q->notEmpty, &q->lock);
    }
    QueueItem item = q->items[q->front];
    q->front = (q->front + 1) % MAX_QUEUE;
    q->size--;
    pthread_cond_signal(&q->notFull);
    pthread_mutex_unlock(&q->lock);
    return item;
}

char* Increment(int pid, Clock *clock) {
    clock->p[pid]++;
    char* message = malloc(MESSAGE_SIZE * sizeof(char));
    sprintf(message, "Processo %d: Clock(%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
    return message;
}

void updateClock(Clock *clock, Clock *received) {
    for (int i = 0; i < CLOCK_SIZE; i++) {
        if (received->p[i] > clock->p[i]) {
            clock->p[i] = received->p[i];
        }
    }
}

char* Send_MPI(int pid, Clock *clock, int dest){
    clock->p[pid]++;
    int error = MPI_Send(clock, sizeof(Clock), MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    if (error != MPI_SUCCESS) {
        fprintf(stderr, "Falha ao enviar mensagem MPI\n");
        MPI_Abort(MPI_COMM_WORLD, error);
    }
    char* message = malloc(MESSAGE_SIZE * sizeof(char));
    sprintf(message, "Processo %d enviou para o processo %d: Clock(%d, %d, %d)\n", pid, dest, clock->p[0], clock->p[1], clock->p[2]);
    return message;
}

char* Recv_MPI(int pid, Clock *clock, int source){
    Clock received;
    int error = MPI_Recv(&received, sizeof(Clock), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (error != MPI_SUCCESS) {
        fprintf(stderr, "Falha ao receber mensagem MPI\n");
        MPI_Abort(MPI_COMM_WORLD, error);
    }
    updateClock(clock, &received);
    char* message = malloc(MESSAGE_SIZE * sizeof(char));
    sprintf(message, "Processo %d recebeu do processo %d: Clock(%d, %d, %d)\n", pid, source, received.p[0], received.p[1], received.p[2]);
    return message;
}

void* processThread(void *arg) {
    Clock clock = {{0, 0, 0}};
    while(1){
        QueueItem item = dequeue(&input_queue);
        if(item.type == EVENT){
            char* message = Increment(item.pid, &clock);
            strcpy(item.message, message);
            free(message);
        } else if(item.type == SEND){
            char* message = Send_MPI(item.pid, &clock, item.target);
            strcpy(item.message, message);
            free(message);
        } else if(item.type == RECEIVE){
            char* message = Recv_MPI(item.pid, &clock, item.target);
            strcpy(item.message, message);
            free(message);
        }
        enqueue(&output_queue, item);
        if (input_queue.size == 0 && item.type != EVENT) {
            break;
        }
    }
    return NULL;
}

void* outputThread(void *arg) {
    while(1) {
        QueueItem item = dequeue(&output_queue);
        if(item.message != NULL){
            printf("%s", item.message);
        } else {
            break;
        }
    }
    return NULL;
}

void Event(int pid){
    MessageType type = EVENT;
    QueueItem item;
    item.pid = pid;
    item.type = type;
    enqueue(&input_queue, item);
}

void Send(int pid, int dest){
    MessageType type = SEND;
    QueueItem item;
    item.pid = pid;
    item.type = type;
    item.target = dest;
    enqueue(&input_queue, item);
}

void Receive(int pid, int source){
    MessageType type = RECEIVE;
    QueueItem item;
    item.pid = pid;
    item.type = type;
    item.target = source;
    enqueue(&input_queue, item);
}

void process0(){
    Event(0);
    Send(0, 1);
    Receive(0, 1);
    Send(0, 2);
    Receive(0, 2);
    Send(0, 1);
    Event(0);
}

void process1(){
    Send(1, 0);
    Receive(1, 0);
    Receive(1, 0);
}

void process2(){
    Event(2);
    Send(2, 0);
    Receive(2, 0);
}

int main() {
    int my_rank;
    int error = MPI_Init(NULL, NULL);
    if (error != MPI_SUCCESS) {
        fprintf(stderr, "Falha ao inicializar MPI\n");
        MPI_Abort(MPI_COMM_WORLD, error);
    }
    error = MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    if (error != MPI_SUCCESS) {
        fprintf(stderr, "Falha ao obter o rank MPI\n");
        MPI_Abort(MPI_COMM_WORLD, error);
    }

    initQueue(&input_queue);
    initQueue(&output_queue);
    pthread_t process, output;
    pthread_create(&process, NULL, processThread, NULL);
    pthread_create(&output, NULL, outputThread, NULL);

    if(my_rank == 0){
        process0();
    } else if(my_rank == 1){
        process1();
    } else if(my_rank == 2){
        process2();
    }
    pthread_join(process, NULL);
    pthread_join(output, NULL);
    MPI_Finalize();
    return 0;
}