#include "semaphore.h"

bool QueueEmpty(struct queue_head_type *pid_queue_head)
{
	return pid_queue_head->tqh_first == NULL;
}

void EnqueuePID(struct queue_head_type *pid_queue_head, pid_t pid)
{
	struct queue_entry *entry;
	entry = malloc(sizeof(struct queue_entry));
	entry->pid = pid;
	TAILQ_INSERT_TAIL(pid_queue_head, entry, queue_entries);
}

void SemaphoreInit(struct BinarySemaphore *s)
{
	TAILQ_INIT(&s->pid_queue_head);
	s->state = 1;
}

void CountingSemaphoreInit(struct CountingSemaphore *s, int resource_count)
{
	TAILQ_INIT(&s->pid_queue_head);
	s->state = resource_count;
}

bool SemaphoreAcquire(struct BinarySemaphore *s, pid_t pid)
{
	if(s->state == 1)
	{
		s->state = 0;
		return true;
	}
	else
	{
		EnqueuePID(&s->pid_queue_head, pid);
		return false;
	}
}

bool CountingSemaphoreAcquire(struct CountingSemaphore *s, pid_t pid)
{
	s->state--;

	if (s->state < 0)
	{
		EnqueuePID(&s->pid_queue_head, pid);
		return false;
	}
	else
	{
		return true;
	}
}

pid_t SemaphoreRelease(struct BinarySemaphore *s)
{
	pid_t pid = SEMAPHORE_QUEUE_EMPTY;
	if(QueueEmpty(&s->pid_queue_head))
	{
		s->state = 1;
	}
	else
	{
		pid = s->pid_queue_head.tqh_first->pid;
		TAILQ_REMOVE(&s->pid_queue_head, s->pid_queue_head.tqh_first, queue_entries);
	}
	return pid;
}

pid_t CountingSemaphoreRelease(struct CountingSemaphore *s)
{
	pid_t pid = SEMAPHORE_QUEUE_EMPTY;
	s->state++;
	if (s->state <= 0)
	{		
		pid = s->pid_queue_head.tqh_first->pid;
		TAILQ_REMOVE(&s->pid_queue_head, s->pid_queue_head.tqh_first, queue_entries);

	}
	return pid;
	

}