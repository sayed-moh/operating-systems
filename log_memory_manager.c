#include <sys/signal.h>
#include "log_memory_manager.h"
#include <errno.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>

bool receiveQueueCommand(key_t log_memory_queue_key, struct LogMemory_Queue_Message *msg)
{
    int rcv_val;
    rcv_val = msgrcv(log_memory_queue_key, msg, sizeof(*msg) - sizeof(msg->mtype), 0, !IPC_NOWAIT);

    //printf("LogMemoryManager rcv val: %d\n", rcv_val);
    if (rcv_val == -1)
    {
        if(errno == EINTR) // ignore errors due to interrupts (interrupt is a signal i.e. SIGUSR2 while receiving)
        {
            //printf("pc %d EINTR\n", pid);
            //continue;
        }
        else
            perror("LogManager: Error in pc receive");
        //break;
        return false;
    }
    else
    {
        return true;
    }
}

void executeLog(struct LogMemory *log_memory)
{
    printf("logger:: %s\n", log_memory->log_message);
    FILE *fp = fopen("out_log.txt", "a");
    fputs(log_memory->log_message, fp);
    fputs("\n", fp);
    fclose(fp);
}

void executeLogMemoryManagerCommand(struct LogMemory_Queue_Message msg, struct LogMemory *log_memory, struct BinarySemaphore* log_memory_semaphore)
{
    if(msg.mtype == LogMemoryAcquire)
    {
        printf("LogMemoryManager acq\n");
        if(SemaphoreAcquire(log_memory_semaphore, msg.pid))
        {
            printf("LogMemoryManager acq pid\n");
            kill(msg.pid, SIGCONT);
        }
    }
    else if(msg.mtype == LogMemoryRelease)
    {
        pid_t pid = SemaphoreRelease(log_memory_semaphore);
        executeLog(log_memory);
        printf("LogMemoryManager rel\n");
        if(pid != SEMAPHORE_QUEUE_EMPTY)
        {
            printf("LogMemoryManager rel pid\n");
            kill(pid, SIGCONT);
        }
    }
}

void doLogMemoryManagerWork(int log_memory_id, key_t log_memory_queue_key)
{
    struct BinarySemaphore log_memory_semaphore;
    struct LogMemory_Queue_Message msg;
    struct LogMemory *log_memory = shmat(log_memory_id, NULL, 0);

    FILE *fp = fopen("out_log.txt", "w");
    fclose(fp);
    SemaphoreInit(&log_memory_semaphore);
    //printf("LogMemoryManager queue id: %d\n", log_memory_queue_key);
    //printf("LogMemoryManager init done, executing commands...\n");
    while (true)
    {
        if(receiveQueueCommand(log_memory_queue_key, &msg))
        {
            printf("LogMemoryManager rcv\n");
            executeLogMemoryManagerCommand(msg, log_memory, &log_memory_semaphore);
        }
    }
}