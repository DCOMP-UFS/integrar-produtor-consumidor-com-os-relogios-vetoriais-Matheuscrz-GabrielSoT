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

typedef struct Task {
    int pid;
    Clock clock;
} Task;

typedef struct ThreadInputArgs {
    int pid;
    Clock* clock;
    int source;
} ThreadInputArgs;

typedef struct ThreadOutputArgs {
    Task task;
} ThreadOutputArgs;

typedef struct Queue {
    Task t[MAX_QUEUE];
    int front, rear, size;
    pthread_mutex_t lock;
    pthread_cond_t notFull, notEmpty;
} Queue;

typedef struct ThreadProcessArgs {
    Queue *input;
    Queue *output;
} ThreadProcessArgs;

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

Clock Receive(int pid, Clock *clock, int source) {
    Clock received;
    MPI_Recv(&received, sizeof(Clock), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    for (int i = 0; i < CLOCK_SIZE; i++) {
        if (received.p[i] > clock->p[i]) {
            clock->p[i] = received.p[i];
        }
    }
    clock->p[pid]++;
    printf("Processo %d recebeu do processo %d: Clock(%d, %d, %d)\n", pid, source, clock->p[0], clock->p[1], clock->p[2]);
    return *clock;
}

void Send(int pid, Clock *clock, int dest) {
    clock->p[pid]++;
    MPI_Send(clock, sizeof(Clock), MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    printf("Processo %d enviou para o processo %d: Clock(%d, %d, %d)\n", pid, dest, clock->p[0], clock->p[1], clock->p[2]);
}

void create_input_task(int pid, Clock *clock) {
    Task t;
    t.pid = pid;
    t.clock = *clock;
    enqueue(&input_queue, t);
}

void create_output_task(int pid, Clock *clock) {
    Task t;
    t.pid = pid;
    t.clock = *clock;
    enqueue(&output_queue, t);
}

void *input_thread(void *arg) {
    ThreadInputArgs *InputArgs = (ThreadInputArgs *)arg;
    int pid = InputArgs->pid;
    Clock *clock = InputArgs->clock;
    int source = InputArgs->source;
    sleep(1);
    Event(pid, clock);
    Send(pid, clock, 1); 
    Clock received_clock = Receive(pid, clock, 1);
    create_input_task(pid, &received_clock);

    free(InputArgs);
    return NULL;
}

void *process_thread(void *arg) {
    ThreadProcessArgs *ProcessArgs = (ThreadProcessArgs*)arg;
    Queue *input_queue = ProcessArgs->input;
    Queue *output_queue = ProcessArgs->output;
    Task t;
    while (1) {
        dequeue(input_queue, &t);
        Event(t.pid, &t.clock);
        printf("Processo %d executou: Clock(%d, %d, %d)\n", t.pid, t.clock.p[0], t.clock.p[1], t.clock.p[2]);
        sleep(2);
        create_output_task(t.pid, &t.clock);
    }
    return NULL;
}

void *output_thread(void *arg) {
    ThreadOutputArgs *OutputArgs = (ThreadOutputArgs *)arg;
    Task t = OutputArgs->task;
    sleep(2);
    Send(t.pid, &t.clock, t.pid == 0 ? 1 : 0);
    free(OutputArgs);
    return NULL;
}

void process0(int pid) {
    Clock clock = {0, 0, 0};
    initQueue(&input_queue);
    initQueue(&output_queue);

    ThreadInputArgs *InputArgs = malloc(sizeof(ThreadInputArgs));
    InputArgs->pid = pid;
    InputArgs->clock = &clock;
    InputArgs->source = 1;

    ThreadProcessArgs ProcessArgs;
    ProcessArgs.input = &input_queue;
    ProcessArgs.output = &output_queue;

    pthread_t input, process, output;
    pthread_create(&input, NULL, input_thread, InputArgs);
    pthread_create(&process, NULL, process_thread, &ProcessArgs);
    pthread_create(&output, NULL, output_thread, NULL);

    pthread_join(input, NULL);
    pthread_join(process, NULL);
    pthread_join(output, NULL);

    free(InputArgs);
}

void process1(int pid) {
    Clock clock = {0, 0, 0};
    initQueue(&input_queue);
    initQueue(&output_queue);

    ThreadInputArgs *InputArgs = malloc(sizeof(ThreadInputArgs));
    InputArgs->pid = pid;
    InputArgs->clock = &clock;
    InputArgs->source = 0;

    pthread_t input, process, output;
    pthread_create(&input, NULL, input_thread, InputArgs);

    ThreadProcessArgs ProcessArgs;
    ProcessArgs.input = &input_queue;
    ProcessArgs.output = &output_queue;
    pthread_create(&process, NULL, process_thread, &ProcessArgs);
    pthread_create(&output, NULL, output_thread, NULL);

    pthread_join(input, NULL);
    pthread_join(process, NULL);
    pthread_join(output, NULL);

    free(InputArgs);
}

void process2(int pid) {
    Clock clock = {0, 0, 0};
    initQueue(&input_queue);
    initQueue(&output_queue);

    ThreadInputArgs *InputArgs = malloc(sizeof(ThreadInputArgs));
    InputArgs->pid = pid;
    InputArgs->clock = &clock;
    InputArgs->source = 0;

    pthread_t input, process, output;
    pthread_create(&input, NULL, input_thread, InputArgs);

    ThreadProcessArgs ProcessArgs;
    ProcessArgs.input = &input_queue;
    ProcessArgs.output = &output_queue;
    pthread_create(&process, NULL, process_thread, &ProcessArgs);
    pthread_create(&output, NULL, output_thread, NULL);

    pthread_join(input, NULL);
    pthread_join(process, NULL);
    pthread_join(output, NULL);

    free(InputArgs);
}

int main(void) {
    int my_rank;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if (my_rank == 0) {
        process0(my_rank);
    } else if (my_rank == 1) {
        process1(my_rank);
    } else if (my_rank == 2) {
        process2(my_rank);
    }

    return 0;
}
