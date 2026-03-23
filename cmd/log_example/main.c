#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define MAX_LOG_MESSAGE 256
#define MAX_QUEUE_SIZE  1000

// Structure for a log record
typedef struct {
    char   message[MAX_LOG_MESSAGE];
    int    severity;
    time_t timestamp;
} LogRecord;

// Circular queue for log records
typedef struct {
    LogRecord records[MAX_QUEUE_SIZE];
    int       head;
    int       tail;
    int       count;
    mtx_t     mutex;
    cnd_t     not_full;
    cnd_t     not_empty;
} LogQueue;

// Global logger state
typedef struct {
    LogQueue queue;
    FILE    *log_file;
    bool     running;
} Logger;

Logger logger;

// Initialize the logging system
void fl_logger_init(char const *filename) {
    logger.queue.head  = 0;
    logger.queue.tail  = 0;
    logger.queue.count = 0;
    mtx_init(&logger.queue.mutex, mtx_plain);
    cnd_init(&logger.queue.not_full);
    cnd_init(&logger.queue.not_empty);

    errno_t err = fopen_s(&logger.log_file, filename, "a");
    if (err != 0) {
        perror("Failed to open log file");
        exit(1);
    }
    logger.running = true;
}

// Clean up logging resources
void fl_logger_cleanup(void) {
    logger.running = false;
    mtx_destroy(&logger.queue.mutex);
    cnd_destroy(&logger.queue.not_full);
    cnd_destroy(&logger.queue.not_empty);
    if (logger.log_file) {
        fclose(logger.log_file);
    }
}

// Add a log record to the queue
bool enqueue_log(char const *message, int severity) {
    mtx_lock(&logger.queue.mutex);

    while ((logger.queue.count >= MAX_QUEUE_SIZE) & logger.running) {
        cnd_wait(&logger.queue.not_full, &logger.queue.mutex);
    }

    if (!logger.running) {
        mtx_unlock(&logger.queue.mutex);
        return false;
    }

    LogRecord record;
    strncpy_s(record.message, sizeof record.message, message, MAX_LOG_MESSAGE - 1);
    record.message[MAX_LOG_MESSAGE - 1] = '\0';
    record.severity                     = severity;
    record.timestamp                    = time(NULL);

    logger.queue.records[logger.queue.tail] = record;
    logger.queue.tail                       = (logger.queue.tail + 1) % MAX_QUEUE_SIZE;
    logger.queue.count++;

    cnd_signal(&logger.queue.not_empty);
    mtx_unlock(&logger.queue.mutex);
    return true;
}

// Main logging thread function
int fl_logger_thread(void *arg) {
    (void)arg; // ignored in this example
    while (logger.running) {
        mtx_lock(&logger.queue.mutex);

        while ((logger.queue.count == 0) & logger.running) {
            cnd_wait(&logger.queue.not_empty, &logger.queue.mutex);
        }

        if (!logger.running & (logger.queue.count == 0)) {
            mtx_unlock(&logger.queue.mutex);
            break;
        }

        // Dequeue a record
        LogRecord record  = logger.queue.records[logger.queue.head];
        logger.queue.head = (logger.queue.head + 1) % MAX_QUEUE_SIZE;
        logger.queue.count--;

        cnd_signal(&logger.queue.not_full);
        mtx_unlock(&logger.queue.mutex);

        // Format and write the log message
        fprintf(logger.log_file, "[%lld] [%d] %s\n", record.timestamp, record.severity,
                record.message);
        fflush(logger.log_file);
    }
    return 0;
}

// Worker thread function - moved outside of main
int worker_thread(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 5; i++) {
        char message[MAX_LOG_MESSAGE];
        snprintf(message, MAX_LOG_MESSAGE, "Message from thread %d", id);
        enqueue_log(message, 1);
        thrd_sleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 100000000},
                   NULL); // Sleep for 100ms
    }
    free(arg);
    return 0;
}

int main(void) {
    fl_logger_init("app.log");

    // Start logger thread
    thrd_t fl_logger_tid;
    thrd_create(&fl_logger_tid, fl_logger_thread, 0);

// Example of multiple threads logging messages
#define NUM_THREADS 3
    thrd_t worker_tids[NUM_THREADS];

    // Create worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        int *id = malloc(sizeof(int));
        *id     = i;
        // thrd_create( thrd_t *thr, thrd_start_t func, void *arg );
        thrd_create(&worker_tids[i], worker_thread, id);
    }

    // Wait for worker threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(worker_tids[i], NULL);
    }

    // Cleanup and shutdown
    logger.running = false;
    cnd_signal(&logger.queue.not_empty); // Wake up logger thread
    thrd_join(fl_logger_tid, NULL);
    fl_logger_cleanup();

    return 0;
}
