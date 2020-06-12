#pragma once
#include <sys/queue.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdlib.h>

#define SEMAPHORE_QUEUE_EMPTY -1

struct queue_head_type *headp;                 /* List head. */
struct queue_entry {
    TAILQ_ENTRY(queue_entry) queue_entries;          /* List. */
	pid_t pid;
};

TAILQ_HEAD(queue_head_type, queue_entry);
struct BinarySemaphore
{
	int state;
	struct queue_head_type pid_queue_head;
	
};

struct CountingSemaphore
{
	int state;
	struct queue_head_type pid_queue_head;
	
};

/*
Initializes semaphore.
*/
void SemaphoreInit(struct BinarySemaphore *s);
void CountingSemaphoreInit(struct CountingSemaphore *s, int resource_count);

/*
Attempt to acquire.
returns true if success.
returns false if failure + enqueue pid to internal queue.
*/
bool SemaphoreAcquire(struct BinarySemaphore *s, pid_t pid);
bool CountingSemaphoreAcquire(struct CountingSemaphore *s, pid_t pid);

/*
Attempt to release.
returns SEMAPHORE_QUEUE_EMPTY if no remaining waiting processes in queue.
Otherwise, returns pid of next process waiting in queue.
*/
pid_t SemaphoreRelease(struct BinarySemaphore *s);
pid_t CountingSemaphoreRelease(struct CountingSemaphore *s);