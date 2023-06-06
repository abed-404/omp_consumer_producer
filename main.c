#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

#define MAX_SENTENCE_LENGTH 1000

typedef struct q_node{
    char* msg;
    int src;
    struct q_node* next;
} q_node;

typedef struct msg_queue{
    struct q_node* front;
    struct q_node* rear;
    int enqueued;
    int dequeued;
    omp_lock_t lock;
} msg_queue;

void Read_chunk_of_messages (char* filename, int thread_num, int chunk_size, msg_queue* dest);
void Read_chunk_of_messages_randomly (char* filename, int thread_num, int chunk_size, msg_queue* queues[4]);
msg_queue* Allocate_queue();
void Free_queue(msg_queue* q);
void Print_queue(msg_queue* q, int queue_num);
void Enqueue(msg_queue* q, int src, char* sentence);
q_node* Dequeue(msg_queue* q, int my_rank);
void Procces_msg(const char* sentence, int thread_num, int* word_counter, int* char_counter);

int main() {
    srand(time(NULL));
    char* fileName = "text.txt";
    int totalSentences = 40;
    int producers_num = 8, consumers_num = 4;
    int sentencesPerChunk = totalSentences / producers_num;
    int thread_count = producers_num + consumers_num;
    int done_sending = 0;
    int global_word_count = 0, global_characters_count = 0;
    // consumer queues
    msg_queue* consumers_queues[consumers_num];
    for(int i = 0; i < consumers_num; i++){
        consumers_queues[i] = Allocate_queue();
    }

    #pragma omp parallel num_threads(thread_count)
    {
        int my_rank = omp_get_thread_num();
        if (my_rank < producers_num){
            int dest = (my_rank / 2) % consumers_num; // to find the destination queue
            Read_chunk_of_messages(fileName, my_rank, sentencesPerChunk, consumers_queues[dest]);
            // Read_chunk_of_messages_randomly(fileName, my_rank, sentencesPerChunk, consumers_queues);
            #pragma omp atomic
            done_sending++;
        }
        else {
            int my_queue = my_rank - producers_num;
            int queue_size = consumers_queues[my_queue]->enqueued - consumers_queues[my_queue]->dequeued;
            q_node* n = NULL;
            int my_words_count = 0, my_characters_count = 0;
            while (queue_size != 0 || done_sending < producers_num){
                if(queue_size == 1){
                    omp_set_lock(&consumers_queues[my_queue]->lock);
                    n = Dequeue(consumers_queues[my_queue], my_rank);
                    omp_unset_lock(&consumers_queues[my_queue]->lock);
                }
                else
                    n = Dequeue(consumers_queues[my_queue], my_rank);
                if (n != NULL){
                    Procces_msg(n->msg, my_rank, &my_words_count, &my_characters_count);
                }
                queue_size = consumers_queues[my_queue]->enqueued - consumers_queues[my_queue]->dequeued;
            }
            #pragma omp critical (counting)
            {
                global_characters_count += my_characters_count;
                global_word_count += my_words_count;
            }
            printf("Thread %d counted %d words and %d characters\n", my_rank, my_words_count, my_characters_count);
        }
    }
    printf("\n------------------------------------\n");
    printf("Number of words in document = %d, and char = %d\n", global_word_count, global_characters_count);
    return 0;
}
void Read_chunk_of_messages (char* filename, int thread_num, int chunk_size, msg_queue* dest){
    FILE* file = fopen(filename, "r"); // Open the file for reading
    if (file == NULL) {
        printf("Failed to open the file.\n");
        return 1;
    }
    fseek(file, 0, SEEK_SET);
    char line[MAX_SENTENCE_LENGTH];

    // Find the starting position for the current chunk
    int start = thread_num * chunk_size;
    // Move to the starting position
    fseek(file, 0, SEEK_SET);
    for (int j = 0; j < start; j++) {
        fgets(line, sizeof(line), file);
    }
    // Read and print the sentences in the current chunk
    for (int k = 0; k < chunk_size; k++) {
        if (fgets(line, sizeof(line), file) != NULL) {
            //printf("thread %d read >> %s", thread_num, line);
            omp_set_lock(&dest->lock);
            Enqueue(dest, thread_num, line);
            omp_unset_lock(&dest->lock);
        } else {
            printf("\n");
            break; // Reached the end of the file
        }
    }
    fclose(file);

}
msg_queue* Allocate_queue() {
    msg_queue* q = malloc(sizeof(msg_queue));
    q->enqueued = q->dequeued = 0;
    q->front = NULL;
    q->rear = NULL;
    omp_init_lock(&q->lock);
    return q;
}

void Free_queue(msg_queue* q) {
    q_node* curr = q->front;
    q_node* temp;
    while(curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    q->enqueued = q->dequeued = 0;
    q->front= q->rear = NULL;
    omp_destroy_lock(&q->lock);
}

void Print_queue(msg_queue* q, int queue_num) {
    q_node* curr = q->front;
    printf("queue %d = \n", queue_num);
    while(curr != NULL) {
        printf(" src = %d. thread, mesg = %s\n", curr->src, curr->msg);
        curr = curr->next;
    }
    printf("enqueued = %d, dequeued = %d\n", q->enqueued, q->dequeued);
    printf("\n");
}

void Enqueue(msg_queue* q, int src, char* sentence) {
    q_node* n = malloc(sizeof(q_node));
    n->src = src;
    n->msg = malloc(strlen(sentence) + 1);
    strcpy(n->msg, sentence);
    n->next = NULL;
    if (q->rear == NULL) { /* Empty Queue */
        q->front = n;
        q->rear = n;
    } else {
        q->rear->next = n;
        q->rear = n;
    }
    q->enqueued++;
}

q_node* Dequeue(msg_queue* q, int my_rank) {
    q_node* tmp_node = NULL;
    if (q->front == NULL) // empty
        return NULL;
    if (q->front->next == NULL) // last node
        q->rear = q->rear->next;
    tmp_node = q->front ;
    q->front  = q->front->next;
    q->dequeued ++;
    return tmp_node;
}

void Procces_msg(const char* sentence, int thread_num, int* word_counter, int* char_counter) {
    int num_chars = 0;
    int num_words = 0;
    int c = 0;
    int len = strlen(sentence);

    for (c = 0; c < len; c++) {
        // sentence[c] != ' ' && sentence[c] != '\n' space does not count
        // sentence[c] != '\n' space is counted
        if (sentence[c] != '\n') {
            num_chars++;
        }
        if ((c == 0 || sentence[c - 1] == ' ') && sentence[c] != ' ') {
            num_words++;
        }
    }
    *word_counter += num_words;
    *char_counter += num_chars;
}
void Read_chunk_of_messages_randomly (char* filename, int thread_num, int chunk_size, msg_queue* queues[4]){
    FILE* file = fopen(filename, "r"); // Open the file for reading
    if (file == NULL) {
        printf("Failed to open the file.\n");
        return 1;
    }
    fseek(file, 0, SEEK_SET);
    char line[MAX_SENTENCE_LENGTH];

    // Find the starting position for the current chunk
    int start = thread_num * chunk_size;
    // Move to the starting position
    fseek(file, 0, SEEK_SET);
    for (int j = 0; j < start; j++) {
        fgets(line, sizeof(line), file);
    }
    // Read and print the sentences in the current chunk
    for (int k = 0; k < chunk_size; k++) {
        if (fgets(line, sizeof(line), file) != NULL) {
            //printf("thread %d read >> %s", thread_num, line);
            int index = rand() % 4;
            omp_set_lock(&queues[index]->lock);
            Enqueue(queues[index], thread_num, line);
            omp_unset_lock(&queues[index]->lock);
        } else {
            printf("\n");
            break; // Reached the end of the file
        }
    }
    fclose(file);

}
