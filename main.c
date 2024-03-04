#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_QUEUE 10
#define NUM_THREADS 3

/**
 * @brief - Estrutura que representa o relógio vetorial.
 * 
 */
typedef struct Clock {
    int p[3];
} Clock;

/**
 * @brief - Estrutura que representa uma Task.
 * 
 */
typedef struct Task {
    int pid;
    Clock clock;
} Task;

/**
 * @brief - Estrutura que representa uma fila.
 * 
 */
typedef struct Queue {
    Task data[MAX_QUEUE];
    int front, rear, size;
    pthread_mutex_t lock;
    pthread_cond_t can_produce, can_consume;
} Queue;

/**
 * @brief - Inicializa a fila.
 * 
 * @param pid - Identificador do processo.
 * @param clock - Relógio vetorial.
 */
void ClockIncrement(int pid, Clock *clock) {
    clock->p[pid]++;
}

/**
 * @brief - Inicializa a fila.
 * 
 * @param item - Item a ser enfileirado.
 * @param q - Fila.
 */
void Input_Enqueue(Task item, Queue *q) {
    pthread_mutex_lock(&mutex_queue);
    input_queue[0] = item;
    pthread_mutex_unlock(&mutex_queue);
}

/**
 * @brief - Remove um item da fila.
 * 
 * @param item - Item a ser desenfileirado.
 * @param q - Fila.
 */
void Input_Dequeue(Task *item, Queue *q){
    pthread_mutex_lock(&mutex_queue);
    *item = input_queue[0];
    pthread_mutex_unlock(&mutex_queue);
}

/**
 * @brief - Adiciona um item à fila de saída.
 * 
 * @param item - Item a ser enfileirado.
 * @param q - Fila.
 */
void Output_Enqueue(Task item, Queue *q){
    pthread_mutex_lock(&mutex_queue);
    output_queue[0] = item;
    pthread_mutex_unlock(&mutex_queue);
}

/**
 * @brief - Remove um item da fila de saída.
 * 
 * @param item - Item a ser desenfileirado.
 * @param q - Fila.
 */
void Output_Dequeue(Task *item, Queue *q){
    pthread_mutex_lock(&mutex_queue);
    *item = output_queue[0];
    pthread_mutex_unlock(&mutex_queue);
}

/**
 * @brief - Função que incrementa o relógio vetorial.
 * 
 * @param task - Tarefa a ser processada.
 * @param input_queue - Fila de entrada.
 * @param output_queue - Fila de saída.
 * @return void* 
 */
void *process_thread(Task *task, Queue *input_queue, Queue *output_queue){
    Task task_aux = *task; // Cria uma cópia da tarefa
    Input_Dequeue(&task_aux, *input_queue);
    for (int i = 0; i < NUM_THREADS; i++) {
        if (task_aux.clock.p[i] > local_clock.p[i]) {
            local_clock.p[i] = task_aux.clock.p[i];
        }
    }
    ClockIncrement(task_aux.pid, &local_clock); // Incrementa o relógio
    Output_Enqueue(task_aux, *output_queue); // Adiciona a tarefa à fila de saída
}

/**
 * @brief - Função que recebe uma tarefa.
 * 
 * @param pid - Identificador do processo.
 * @param task - Tarefa a ser processada.
 * @param source - Processo de origem.
 * @param input_queue - Fila de entrada.
 * @return void* 
 */
void *receive_thread(int pid, Task *task, int source, Queue *input_queue){
    Task task_aux; // Cria uma tarefa auxiliar
    MPI_Recv(&task_aux, sizeof(Task), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // Recebe a tarefa
    Input_Enqueue(task_aux, *input_queue); // Adiciona a tarefa à fila de entrada
}
/**
 * @brief - Função que envia uma tarefa.
 * 
 * @param pid - Identificador do processo.
 * @param task - Tarefa a ser processada.
 * @param dest - Processo de destino.
 * @param output_queue - Fila de saída.
 * @return void* 
 */
void *send_thread(int pid, Task *task, int dest, Queue *output_queue){
    Task task_aux = *task; // Cria uma cópia da tarefa
    Output_Dequeue(&task_aux, *output_queue); // Remove a tarefa da fila de saída
    MPI_Send(&task_aux, sizeof(Task), MPI_BYTE, dest, 0, MPI_COMM_WORLD); // Envia a tarefa
}