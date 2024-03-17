#include <stdio.h>
#include <string.h>
#include <mpi.h>

#define MAX_QUEUE_SIZE 100

// Estrutura para representar o rel�gio de um processo
typedef struct Clock {
    int p[3]; // Array de inteiros para os rel�gios dos processos
} Clock;

// Estrutura para representar a fila FIFO de rel�gios recebidos
typedef struct {
    Clock data[MAX_QUEUE_SIZE]; // Array para armazenar os rel�gios
    int front, rear; // �ndices de frente e tr�s da fila
} ClockQueue;

// Prot�tipos das fun��es
void enqueue(ClockQueue *queue, Clock *clock);
int isEmpty(ClockQueue *queue);
Clock dequeue(ClockQueue *queue);

// Fun��o para incrementar o rel�gio de um processo ap�s um evento
void Event(int pid, Clock *clock){
    clock->p[pid]++;
}

// Fun��o para enviar um rel�gio para um processo destino
void Send(int pid, Clock *clock, int dest, ClockQueue *queue){
    clock->p[pid]++; // Incrementa o rel�gio do processo emissor

    // Se uma fila for fornecida, adiciona o rel�gio � fila
    if (queue != NULL) {
        enqueue(queue, clock);
    }

    // Envia o rel�gio para o processo destino usando MPI_Send
    MPI_Send(clock, 3, MPI_INT, dest, 0, MPI_COMM_WORLD);
    printf("Processo %d enviou para o processo %d: Clock(%d, %d, %d)\n", pid, dest, clock->p[0], clock->p[1], clock->p[2]);
}

// Fun��o para receber um rel�gio de um processo origem
void Receive(int pid, Clock *clock, int source, ClockQueue *queue){
    // Se uma fila for fornecida e n�o estiver vazia, retira um rel�gio da fila
    if (queue != NULL && !isEmpty(queue)) {
        *clock = dequeue(queue);
    }

    // Recebe o novo rel�gio do processo origem usando MPI_Recv
    Clock received;
    MPI_Recv(&received, 3, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Atualiza o rel�gio local com base no rel�gio recebido
    for(int i = 0; i < 3; i++){
        if(received.p[i] > clock->p[i]){
            clock->p[i] = received.p[i];
        }
    }

    // Incrementa o rel�gio local do processo receptor
    clock->p[pid]++;

    printf("Processo %d recebeu do processo %d: Clock(%d, %d, %d)\n", pid, source, clock->p[0], clock->p[1], clock->p[2]);
}

// Fun��o para inicializar a fila
void initQueue(ClockQueue *queue) {
    queue->front = -1;
    queue->rear = -1;
}

// Fun��o para verificar se a fila est� vazia
int isEmpty(ClockQueue *queue) {
    return (queue->front == -1 && queue->rear == -1);
}

// Fun��o para verificar se a fila est� cheia
int isFull(ClockQueue *queue) {
    return ((queue->rear + 1) % MAX_QUEUE_SIZE == queue->front);
}

// Fun��o para adicionar um rel�gio � fila
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

// Fun��o para remover um rel�gio da fila
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

// Fun��o para representar o processo de rank 0
void process0(){
    Clock clock = {{0,0,0}}; // Inicializa o rel�gio do processo 0
    Event(0, &clock); // Atualiza o rel�gio ap�s um evento
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);

    ClockQueue queue; // Inicializa a fila de rel�gios recebidos
    initQueue(&queue);

    // Envia e recebe mensagens para outros processos
    Send(0, &clock, 1, &queue); // Envia para processo 1
    Receive(0, &clock, 1, &queue); // Recebe de processo 1

    Send(0, &clock, 2, &queue); // Envia para processo 2
    Receive(0, &clock, 2, &queue); // Recebe de processo 2

    Send(0, &clock, 1, &queue); // Envia novamente para processo 1

    Event(0, &clock); // Atualiza o rel�gio ap�s outro evento

    printf("Processo %d, Clock troca com o processo 1: (%d, %d, %d)\n", 0, clock.p[0], clock.p[1], clock.p[2]);
}

// Fun��o para representar o processo de rank 1
void process1(){
    Clock clock = {{0,0,0}}; // Inicializa o rel�gio do processo 1
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 1, clock.p[0], clock.p[1], clock.p[2]);
    Send(1, &clock, 0, NULL); // Envia para processo 0
    Receive(1, &clock, 0, NULL); // Recebe de processo 0

    Receive(1, &clock, 0, NULL); // Recebe novamente de processo 0
}

// Fun��o para representar o processo de rank 2
void process2(){
    Clock clock = {{0,0,0}}; // Inicializa o rel�gio do processo 2
    Event(2, &clock); // Atualiza o rel�gio ap�s um evento
    printf("Processo: %d, Clock: (%d, %d, %d)\n", 2, clock.p[0], clock.p[1], clock.p[2]);
    Send(2, &clock, 0, NULL); // Envia para processo 0
    Receive(2, &clock, 0, NULL); // Recebe de processo 0
}

// Fun��o principal
int main(void) {
    int my_rank;

    MPI_Init(NULL, NULL); // Inicializa o ambiente MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); // Obt�m o rank do processo

    // Chama a fun��o correspondente ao processo com base no seu rank
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
