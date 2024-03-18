#include <stdio.h>
#include <string.h>
#include <mpi.h>

#define MAX_QUEUE_SIZE 100

// Estrutura para representar o relógio de um processo
typedef struct Clock {
    int p[3]; // Array de inteiros para os relógios dos processos
} Clock;

// Estrutura para representar a fila FIFO de relógios recebidos
typedef struct {
    Clock data[MAX_QUEUE_SIZE]; // Array para armazenar os relógios
    int front, rear; // Índices de frente e trás da fila
} ClockQueue;

// Protótipos das funções
void enqueue(ClockQueue *queue, Clock *clock);
int isEmpty(ClockQueue *queue);
Clock dequeue(ClockQueue *queue);

// Função para incrementar o relógio de um processo após um evento
void Event(int pid, Clock *clock){
    clock->p[pid]++;
}

// Função para enviar um relógio para um processo destino
void Send(int pid, Clock *clock, int dest, ClockQueue *queue){
    clock->p[pid]++; // Incrementa o relógio do processo emissor

    // Se uma fila for fornecida, adiciona o relógio à fila
    if (queue != NULL) {
        enqueue(queue, clock);
    }

    // Envia o relógio para o processo destino usando MPI_Send
    MPI_Send(clock, 3, MPI_INT, dest, 0, MPI_COMM_WORLD);
    printf("Processo %d enviou para o processo %d: Clock(%d, %d, %d)\n", pid, dest, clock->p[0], clock->p[1], clock->p[2]);
}

// Função para receber um relógio de um processo origem
void Receive(int pid, Clock *clock, int source, ClockQueue *queue){
    // Se uma fila for fornecida e não estiver vazia, retira um relógio da fila
    if (queue != NULL && !isEmpty(queue)) {
        *clock = dequeue(queue);
    }

    // Recebe o novo relógio do processo origem usando MPI_Recv
    Clock received;
    MPI_Recv(&received, 3, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Atualiza o relógio local com base no relógio recebido
    for(int i = 0; i < 3; i++){
        if(received.p[i] > clock->p[i]){
            clock->p[i] = received.p[i];
        }
    }

    // Incrementa o relógio local do processo receptor
    clock->p[pid]++;

    printf("Processo %d recebeu do processo %d: Clock(%d, %d, %d)\n", pid, source, clock->p[0], clock->p[1], clock->p[2]);
}

// Função para inicializar a fila
void initQueue(ClockQueue *queue) {
    queue->front = -1;
    queue->rear = -1;
}

// Função para verificar se a fila está vazia
int isEmpty(ClockQueue *queue) {
    return (queue->front == -1 && queue->rear == -1);
}

// Função para verificar se a fila está cheia
int isFull(ClockQueue *queue) {
    return ((queue->rear + 1) % MAX_QUEUE_SIZE == queue->front);
}

// Função para adicionar um relógio à fila
void enqueue(ClockQueue *queue, Clock *clock) {
    if (isFull(queue)) {
        printf("Queue is full\n");
        return;
    } else if (isEmpty(queue)) {
        queue->front = 0;
        queue->rear = 0;
    } else {
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    }
    queue->data[queue->rear] = *clock;
}

// Função para remover um relógio da fila
Clock dequeue(ClockQueue *queue) {
    Clock emptyClock = {{0, 0, 0}};
    if (isEmpty(queue)) {
        printf("Queue is empty\n");
        return emptyClock;
    } else if (queue->front == queue->rear) {
        Clock item = queue->data[queue->front];
        queue->front = -1;
        queue->rear = -1;
        return item;
    } else {
        Clock item = queue->data[queue->front];
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
        return item;
    }
}

// Função para representar o processo de rank 0
void process0(){
    Clock clock = {{0,0,0}}; // Inicializa o relógio do processo 0
    Event(0, &clock); // Atualiza o relógio após um evento
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);

    ClockQueue queue; // Inicializa a fila de relógios recebidos
    initQueue(&queue);

    // Envia e recebe mensagens para outros processos
    Send(0, &clock, 1, &queue); // Envia para processo 1
    Receive(0, &clock, 1, &queue); // Recebe de processo 1

    Send(0, &clock, 2, &queue); // Envia para processo 2
    Receive(0, &clock, 2, &queue); // Recebe de processo 2

    Send(0, &clock, 1, &queue); // Envia novamente para processo 1

    Event(0, &clock); // Atualiza o relógio após outro evento

    printf("Processo %d, Clock troca com o processo 1: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
}

// Função para representar o processo de rank 1
void process1(){
    Clock clock = {{0,0,0}}; // Inicializa o relógio do processo 1
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 1, clock.p[0], clock.p[1], clock.p[2]);
    Send(1, &clock, 0, NULL); // Envia para processo 0
    Receive(1, &clock, 0, NULL); // Recebe de processo 0

    Receive(1, &clock, 0, NULL); // Recebe novamente de processo 0
}

// Função para representar o processo de rank 2
void process2(){
    Clock clock = {{0,0,0}}; // Inicializa o relógio do processo 2
    Event(2, &clock); // Atualiza o relógio após um evento
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 2, clock.p[0], clock.p[1], clock.p[2]);
    Send(2, &clock, 0, NULL); // Envia para processo 0
    Receive(2, &clock, 0, NULL); // Recebe de processo 0
}

// Função principal
int main(void) {
    int my_rank;

    MPI_Init(NULL, NULL); // Inicializa o ambiente MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); // Obtém o rank do processo

    // Chama a função correspondente ao processo com base no seu rank
    if (my_rank == 0) {
        process0();
    } else if (my_rank == 1) {
        process1();
    } else if (my_rank == 2) {
        process2();
    }

    MPI_Finalize(); // Finaliza o ambiente MPI

    return 0;
}
