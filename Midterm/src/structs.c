struct queue {
    int front;
    int rear;
    int capacity;
    int size;
    struct client **elements;
};

struct client {
    pid_t pid;
    int tryConnect;
};

struct client *dequeue(struct queue *que)
{
    struct client *item;
    if (que->size == 0)
        return NULL;
    item = que->elements[que->front]; // Get the client pointer at the front of the queue
    que->elements[que->front] = NULL; // Set the front element to NULL to indicate it's no longer in the queue

    que->front = (que->front == que->capacity - 1) ? 0 : que->front + 1;
    que->size -= 1;
    return item; // Return the dequeued client pointer
}

int resize_queue(struct queue *que) 
{
    int i, new_capa;
    struct client **new_elements;

    // Calculate new capacity (doubling the current capacity)
    new_capa = que->capacity == 0 ? 1 : 2 * que->capacity;  
    if ((new_elements = malloc(new_capa * sizeof(struct queue *))) == NULL) {
        return -1; // Memory allocation failed
    }

    for (i = 0; i < que->size; ++i) { 
        new_elements[i] = que->elements[(que->front + i) % que->capacity]; // Copy elements to new array
    }

    free(que->elements); // Free memory of old array

    // Update queue properties
    que->elements = new_elements;
    que->capacity = new_capa;
    que->front = 0;
    que->rear = que->size;  
    return 0; 
}

int enqueue(struct queue *que, struct client *item) 
{
    if (que->size == que->capacity && resize_queue(que) == -1)
        return -1; // Queue is full and resizing failed, cannot enqueue

    if ((que->elements[que->rear] = malloc(sizeof(struct client))) == NULL)
        return -1; // Memory allocation failed
    *(que->elements[que->rear]) = *item; // Copy client data to the queue
    que->rear = (que->rear == que->capacity - 1) ? 0 : que->rear + 1; // Update rear position
    que->size += 1; 
    return 0; 
}

int queue_constructor(struct queue *que) 
{
    // Initialize data
    que->elements = NULL; 
    que->front = 0; 
    que->rear = 0;
    que->capacity = 0;
    que->size = 0;
    return 0;
}

void queue_destructor(struct queue *q) {
    if (q == NULL) {
        return; // Queue is NULL, nothing to free
    }

    if (q->elements != NULL) {
        for (int i = 0; i < q->capacity; ++i) {
            if (q->elements[i] != NULL) {
                free(q->elements[i]); // Free memory of each client structure
            }
        }
        free(q->elements); // Free memory of array of client pointers
    }
}
