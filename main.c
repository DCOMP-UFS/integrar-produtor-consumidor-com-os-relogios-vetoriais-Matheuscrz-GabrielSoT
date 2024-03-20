#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <mpi.h>

#define MAX_QUEUE_SIZE 5 // Tamanho máximo da fila de relógios

// Estrutura para representar o relógio de um processo
typedef struct Clock {
    int p[3]; // Array de inteiros para os relógios dos processos
} Clock;

// Estrutura para representar um buffer de produtor/consumidor
typedef struct {
    Clock data[MAX_QUEUE_SIZE]; // Array para armazenar os relógios
    int front, rear; // Índices de frente e trás da fila
    pthread_mutex_t mutex; // Mutex para controle de acesso ao buffer
    pthread_cond_t empty, full; // Variáveis de condição para sincronização
} ClockQueue;

// Protótipos das funções
void enqueue(ClockQueue *queue, Clock *clock);
Clock dequeue(ClockQueue *queue);

// Função para inicializar a fila
void initQueue(ClockQueue *queue) {
    queue->front = -1;
    queue->rear = -1;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->empty, NULL);
    pthread_cond_init(&queue->full, NULL);
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
    pthread_mutex_lock(&queue->mutex); // Bloqueia o acesso ao buffer com o mutex

    // Aguarda até que haja espaço disponível no buffer
    while (isFull(queue)) {
        pthread_cond_wait(&queue->full, &queue->mutex);
    }

    // Insere o relógio no buffer
    if (isEmpty(queue)) {
        queue->front = 0;
        queue->rear = 0;
    } else {
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    }
    queue->data[queue->rear] = *clock;

    // Sinaliza que o buffer não está mais vazio e está cheio
    pthread_cond_signal(&queue->empty);
    pthread_cond_signal(&queue->full);

    pthread_mutex_unlock(&queue->mutex); // Libera o acesso ao buffer
}

// Função para remover um relógio da fila
Clock dequeue(ClockQueue *queue) {
    pthread_mutex_lock(&queue->mutex); // Bloqueia o acesso ao buffer com o mutex

    // Aguarda até que haja um item disponível no buffer
    while (isEmpty(queue)) {
        pthread_cond_wait(&queue->empty, &queue->mutex);
    }

    // Remove o relógio do buffer
    Clock emptyClock = {{0, 0, 0}};
    if (queue->front == queue->rear) {
        Clock item = queue->data[queue->front];
        queue->front = -1;
        queue->rear = -1;
        pthread_cond_signal(&queue->full); // Sinaliza que o buffer não está mais cheio
        pthread_mutex_unlock(&queue->mutex); // Libera o acesso ao buffer
        return item;
    } else {
        Clock item = queue->data[queue->front];
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;

        // Sinaliza que o buffer não está mais cheio
        pthread_cond_signal(&queue->full);
        pthread_mutex_unlock(&queue->mutex); // Libera o acesso ao buffer
        return item;
    }
}

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
        printf("Processo %d: Enviou para a fila de saída. Itens na fila de saída: %d\n", pid, (queue->rear - queue->front + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE + 1);
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
        printf("Processo %d: Recebeu da fila de entrada. Itens na fila de entrada: %d\n", pid, (queue->rear - queue->front + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE);
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
