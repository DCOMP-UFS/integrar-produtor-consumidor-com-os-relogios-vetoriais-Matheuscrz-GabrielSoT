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

typedef struct Queue {
    Task task[MAX_QUEUE];
    int front, rear, size;
    pthread_mutex_t lock;
    pthread_cond_t notEmpty, notFull;
} Queue;

Queue input_queue;
Queue output_queue;
int stop_threads = 0;

void init_queue(Queue *q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
    pthread_cond_init(&q->notFull, NULL);
}

void enqueue(Queue *q, Task task) {
    pthread_mutex_lock(&q->lock);
    while (q->size == MAX_QUEUE) {
        printf("Fila cheia - Processo %d esperando para enviar uma task\n", task.pid);
        pthread_cond_wait(&q->notFull, &q->lock);
    }
    q->rear = (q->rear + 1) % MAX_QUEUE;
    q->task[q->rear] = task;
    q->size++;
    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->lock);
}

Task dequeue(Queue *q) {
    pthread_mutex_lock(&q->lock);
    while (q->size == 0) {
        printf("Fila vazia - Processo esperando por uma task para processar\n");
        pthread_cond_wait(&q->notEmpty, &q->lock);
    }
    Task task = q->task[q->front];
    q->front = (q->front + 1) % MAX_QUEUE;
    q->size--;
    pthread_cond_signal(&q->notFull);
    pthread_mutex_unlock(&q->lock);
    return task;
}

void destroy_queue(Queue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->notEmpty);
    pthread_cond_destroy(&q->notFull);
}

void* receiveThread(void *arg) {
    int my_rank = *((int *)arg);
    init_queue(&input_queue);
    while (!stop_threads) {
        printf("Processo %d esperando por uma task\n", my_rank);
        Task received_task;
        MPI_Recv(&received_task, sizeof(Task), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        enqueue(&input_queue, received_task);
        printf("Processo %d recebeu uma task do processo %d\n", my_rank, received_task.pid);
        fflush(stdout);
    }
    pthread_exit(NULL);
}

void* sendTask(void *arg) {
    int dest = *((int *)arg);
    printf("Thread de Envio para o Processo %d - Iniciada\n", dest);
    Task task = { .pid = dest };
    MPI_Send(&task, sizeof(Task), MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    printf("Processo %d enviou uma task para o processo %d\n", dest, dest);
    fflush(stdout);
    pthread_exit(NULL);
}

void processClock(Task task, Clock *local_clock) {
    for (int i = 0; i < CLOCK_SIZE; i++) {
        if (task.clock.p[i] > local_clock->p[i]) {
            local_clock->p[i] = task.clock.p[i];
        }
    }
    local_clock->p[task.pid]++;
}

void* processingThread(void *arg) {
    int my_rank = *((int *)arg);
    init_queue(&input_queue);
    init_queue(&output_queue);
    Clock* local_clock = (Clock*)malloc(sizeof(Clock));
    while (!stop_threads) {
        printf("Processo %d esperando por uma task para processar\n", my_rank);

        pthread_mutex_lock(&input_queue.lock);
        while (input_queue.size == 0) {
            printf("Fila de entrada vazia - Processo %d esperando por uma task\n", my_rank);
            pthread_cond_wait(&input_queue.notEmpty, &input_queue.lock);
        }
        pthread_mutex_unlock(&input_queue.lock);

        Task task = dequeue(&input_queue);

        pthread_mutex_lock(&output_queue.lock);
        while (output_queue.size == MAX_QUEUE) {
            printf("Fila de saída cheia - Processo %d esperando para enviar uma task\n", my_rank);
            pthread_cond_wait(&output_queue.notFull, &output_queue.lock);
        }
        pthread_mutex_unlock(&output_queue.lock);

        processClock(task, local_clock);

        enqueue(&output_queue, task);

        printf("Processo %d processou uma task\n", my_rank);
        fflush(stdout);
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int my_rank, num_processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);

    pthread_t receive_thread, process_thread, send_thread;

    if (my_rank == 0) {
        pthread_create(&send_thread, NULL, sendTask, (void*)&my_rank);
    } else if (my_rank == 1) {
        pthread_create(&receive_thread, NULL, receiveThread, (void*)&my_rank);
        pthread_create(&process_thread, NULL, processingThread, (void*)&my_rank);
    } else if (my_rank == 2) {
        // Processo 2 apenas aguarda por tarefas, sem envio ou processamento
        pthread_create(&receive_thread, NULL, receiveThread, (void*)&my_rank);
    }

    if (my_rank == 0) {
        pthread_join(send_thread, NULL);
    } else if (my_rank == 1) {
        pthread_join(receive_thread, NULL);
        pthread_join(process_thread, NULL);
    } else if (my_rank == 2) {
        pthread_join(receive_thread, NULL);
    }

    destroy_queue(&input_queue);
    destroy_queue(&output_queue);

    MPI_Finalize();
    return 0;
}
