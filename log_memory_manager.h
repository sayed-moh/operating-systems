#include <string.h>
#include <sys/msg.h>

#include <stdbool.h>
#include "semaphore.h"
#define LOG_MESSAGE_LENGTH 100

struct LogMemory
{
    char log_message[LOG_MESSAGE_LENGTH];
};
enum LogMemory_Queue_Message_Type
{
    LogMemoryAcquire = 1,
    LogMemoryRelease
};
struct LogMemory_Queue_Message
{
    long mtype;
    pid_t pid;
};
void doLogMemoryManagerWork(int log_memory_id, key_t log_memory_queue_key);