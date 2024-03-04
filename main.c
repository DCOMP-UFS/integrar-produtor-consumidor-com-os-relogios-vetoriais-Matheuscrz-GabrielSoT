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
    Task t[MAX_QUEUE];
    int front, rear, size;
    pthread_mutex_t lock;
    pthread_cond_t can_produce, can_consume;
} Queue;

pthread_mutex_t mutex_input_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_input_queue = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_output_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_output_queue = PTHREAD_COND_INITIALIZER;

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
    pthread_mutex_lock(&q->lock);
    input_queue[0] = item;
    pthread_mutex_unlock(&q->lock);
}

/**
 * @brief - Remove um item da fila.
 * 
 * @param item - Item a ser desenfileirado.
 * @param q - Fila.
 */
void Input_Dequeue(Task *item, Queue *q){
    pthread_mutex_lock(&q->lock);
    *item = input_queue[0];
    pthread_mutex_unlock(&q->lock);
}

/**
 * @brief - Adiciona um item à fila de saída.
 * 
 * @param item - Item a ser enfileirado.
 * @param q - Fila.
 */
void Output_Enqueue(Task item, Queue *q){
    pthread_mutex_lock(&q->lock);
    output_queue[0] = item;
    pthread_mutex_unlock(&q->lock);
}

/**
 * @brief - Remove um item da fila de saída.
 * 
 * @param item - Item a ser desenfileirado.
 * @param q - Fila.
 */
void Output_Dequeue(Task *item, Queue *q){
    pthread_mutex_lock(&q->lock);
    *item = output_queue[0];
    pthread_mutex_unlock(&q->lock);
}

/**
 * @brief - Função que incrementa o relógio vetorial.
 * 
 * @param input_queue - Fila de entrada.
 * @param output_queue - Fila de saída.
 * @return void* 
 */
void *process_thread(Queue *input_queue, Queue *output_queue){
    Task task_aux = input_queue[0]; // Cria uma cópia da tarefa
    Input_Dequeue(&task_aux, *input_queue); // Remove a tarefa da fila de entrada
    for (int i = 0; i < 3; i++) {
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
 * @param task - Tarefa a ser processada.
 * @param source - Processo de origem.
 * @return void* 
 */
void *receive_thread(Task *task, int source){
    Task task_aux = *task // Cria uma tarefa auxiliar
    MPI_Recv(&task_aux, sizeof(Task), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // Recebe a tarefa
    Input_Enqueue(task_aux, *input_queue); // Adiciona a tarefa à fila de entrada
    pthread_exit(NULL);
}
/**
 * @brief - Função que envia uma tarefa.
 * 
 * @param task - Tarefa a ser processada.
 * @param dest - Processo de destino.
 * @return void* 
 */
void *send_thread(Task *task, int dest){
    Task task_aux = *task; // Cria uma cópia da tarefa
    Output_Dequeue(&task_aux, *output_queue); // Remove a tarefa da fila de saída
    MPI_Send(&task_aux, sizeof(Task), MPI_BYTE, dest, 0, MPI_COMM_WORLD); // Envia a tarefa
    pthread_exit(NULL);
}

void process0(){
    Clock local_clock = {0, 0, 0}; // Inicializa o relógio vetorial
    Queue input_queue, output_queue; // Inicializa as filas
    ClockIncrement(0, &local_clock); // Incrementa o relógio
    printf("Process 0: %d %d %d\n", local_clock.p[0], local_clock.p[1], local_clock.p[2]);
    Task task = {0, local_clock}; // Inicializa a tarefa
    pthread_t thread1, thread2, thread3;
    pthread_create(&thread1, NULL, process_thread, &input_queue);
    pthread_create(&thread2, NULL, send_thread, &task);
    pthread_create(&thread3, NULL, receive_thread, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    send_thread(task, 1); // Envia a tarefa
    receive_thread(task, 1); // Recebe a tarefa
    Input_Enqueue(task, input_queue); // Adiciona a tarefa à fila de entrada
    process_thread(input_queue, output_queue); // Processa a tarefa
    send_thread(task, 2); // Envia a tarefa
    receive_thread(task, 2); // Recebe a tarefa
    Input_Enqueue(task, input_queue); // Adiciona a tarefa à fila de entrada
    process_thread(input_queue, output_queue); // Processa a tarefa
    send_thread(task, 1); // Envia a tarefa
    ClockIncrement(0, &local_clock); // Incrementa o relógio
    printf("Process 0: %d %d %d\n", local_clock.p[0], local_clock.p[1], local_clock.p[2]);
}


void process1(){
    Clock local_clock = {0, 0, 0}; // Inicializa o relógio vetorial
    Queue input_queue, output_queue; // Inicializa as filas
    Task task = {1, local_clock}; // Inicializa a tarefa
    pthread_t thread1, thread2, thread3;
    pthread_create(&thread1, NULL, process_thread, &input_queue);
    pthread_create(&thread2, NULL, send_thread, &task);
    pthread_create(&thread3, NULL, receive_thread, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    printf("Process 1: %d %d %d\n", local_clock.p[0], local_clock.p[1], local_clock.p[2]);
    send_thread(task, 0); // Envia a tarefa
    receive_thread(task, 0); // Recebe a tarefa
    Input_Enqueue(task, input_queue); // Adiciona a tarefa à fila de entrada
    process_thread(input_queue, output_queue); // Processa a tarefa
    receive_thread(task, 2); // Recebe a tarefa
    Input_Enqueue(task, input_queue); // Adiciona a tarefa à fila de entrada
    process_thread(input_queue, output_queue); // Processa a tarefa
}

void process2(){
    Clock local_clock = {0, 0, 0}; // Inicializa o relógio vetorial
    Queue input_queue, output_queue; // Inicializa as filas
    Task task = {2, local_clock}; // Inicializa a tarefa
    pthread_t thread1, thread2, thread3;
    pthread_create(&thread1, NULL, process_thread, &input_queue);
    pthread_create(&thread2, NULL, send_thread, &task);
    pthread_create(&thread3, NULL, receive_thread, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    printf("Process 2: %d %d %d\n", local_clock.p[0], local_clock.p[1], local_clock.p[2]);
    ClockIncrement(2, &local_clock); // Incrementa o relógio
    printf("Process 2: %d %d %d\n", local_clock.p[0], local_clock.p[1], local_clock.p[2]);
    send_thread(task, 0); // Envia a tarefa
    receive_thread(task, 0); // Recebe a tarefa
    Input_Enqueue(task, input_queue); // Adiciona a tarefa à fila de entrada
    process_thread(input_queue, output_queue); // Processa a tarefa
}

int main(void){
    int my_rank;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if (my_rank == 0){
        process0();
    } else if (my_rank == 1){
        process1();
    } else if (my_rank == 2){
        process2();
    }
    MPI_Finalize();

    return 0;
}