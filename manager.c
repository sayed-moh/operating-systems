#include "manager.h"
#include <errno.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/signal.h>
#include <unistd.h>

#include <stdarg.h>
#include "log_memory_manager.h"
#include "timestamp.h"

FILE *manager_fp;
char manager_log_name[] = "manager_log.txt";

void manager_printf(char* fmt, ...)
{
    char str[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(str, fmt, args);
    va_end(args);

    manager_fp = fopen(manager_log_name, "a");
    fputs(str, manager_fp);
    fclose(manager_fp);
}

/*
    Blocks untill we can log.
*/
bool sendManagerLogRequest(key_t log_memory_queue_key)
{
    int return_value;
    struct LogMemory_Queue_Message msg;
    msg.mtype = LogMemoryAcquire;
    msg.pid = getpid();

    return_value = msgsnd(log_memory_queue_key, &msg, sizeof(msg)-sizeof(msg.mtype), !IPC_NOWAIT);

    if(return_value == -1)
    {
        perror("manager send error: ");
        printf("manager log_memory_queue_key %d %d\n", log_memory_queue_key, errno);
    }
    printf("mgr zcq\n");
    raise(SIGSTOP);
    printf("mgr zcq2\n");
    
    return return_value != -1;
}

bool sendManagerReleaseRequest(key_t log_memory_queue_key)
{
    int return_value;
    struct LogMemory_Queue_Message msg;
    msg.mtype = LogMemoryRelease;
    msg.pid = getpid();

    return_value = msgsnd(log_memory_queue_key, &msg, sizeof(msg)-sizeof(msg.mtype), !IPC_NOWAIT);

    if(return_value == -1)
    {
        perror("manager send error: ");
        printf("manager log_memory_queue_key %d %d\n", log_memory_queue_key, errno);
    }

    
    return return_value != -1;
}

bool receiveClientCommand(key_t database_message_queue_key, struct DataBase_Queue_Message *msg)
{
    int rcv_val;
    rcv_val = msgrcv(database_message_queue_key, msg, sizeof(*msg) - sizeof(msg->mtype), 0, !IPC_NOWAIT);

    
    if (rcv_val == -1)
    {
        if(errno == EINTR) // ignore errors due to interrupts (interrupt is a signal i.e. SIGUSR2 while receiving)
        {
            //printf("pc %d EINTR\n", pid);
            //continue;
        }
        else
            perror("Manager: Error in pc receive");
        //break;
        return false;
    }
    else
    {
        return true;
    }
}

void executeManagerAdd(struct DataBase_Queue_Add_Message msg, struct DataBase_Table *table, struct LogMemory *log_memory, key_t log_memory_queue_key)
{
	int index=table->row_count;
    char timestamp[27] = {0};
    get_time_stamp(timestamp);

    table->rows[index].key=index;
    table->rows[index].salary=msg.salary;
    for(int i=0;i<DATABASE_NAME_LENGTH;i++)
    {
    		table->rows[index].name[i]=msg.name[i];
    }
    (table->row_count)++;
    manager_printf("%s add: name: %s salary:%d key:%d\n",timestamp, table->rows[index].name,table->rows[index].salary,table->rows[index].key);

    //sendManagerLogRequest(log_memory_queue_key);
    //sprintf(log_memory->log_message, "%s: manager: add %s %d index: %d", timestamp ,msg.name, msg.salary, index);
    //sendManagerReleaseRequest(log_memory_queue_key);

}

void executeManagerModify(struct DataBase_Queue_Modify_Message msg, struct DataBase_Table *table, struct LogMemory *log_memory, key_t log_memory_queue_key)
{
    bool should_add = msg.should_add;
    int amount = msg.amount;
    int key = msg.key;
    int old_salary = table->rows[key].salary;
    char timestamp[27] = {0};
    get_time_stamp(timestamp);
    if(should_add)
        table->rows[key].salary += amount;
    else
        table->rows[key].salary -= amount;
    manager_printf("%s modify: key: %d old_salary: %d  %s amount:%d  new_salary:%d\n", timestamp, key, old_salary, (should_add? "+" : "-"), amount,table->rows[key].salary);

    //sendManagerLogRequest(log_memory_queue_key);
    //sprintf(log_memory->log_message, "%s: manager: modify %d %s %d", timestamp ,msg.key, (should_add? "+" : "-"), amount);
    //sendManagerReleaseRequest(log_memory_queue_key);
    
}

void executeManagerAcquire(struct DataBase_Queue_Acquire_Message msg, struct DataBase_Table *table, struct LogMemory *log_memory, key_t log_memory_queue_key)
{
    pid_t pid = msg.child_pid;
    bool success = SemaphoreAcquire(&table->semaphores[msg.key],pid);
    char timestamp[27] = {0};
    get_time_stamp(timestamp);
	if (success)
	{
		kill(pid, SIGCONT);
	}
    manager_printf("%s acquire: %d %d %s\n", timestamp, msg.key, msg.child_pid, (success? "success" : "fail"));
    //sendManagerLogRequest(log_memory_queue_key);
    //sprintf(log_memory->log_message, "%s: manager: acquire %d", timestamp ,msg.key);
    //sendManagerReleaseRequest(log_memory_queue_key);
}

void executeManagerRelease(struct DataBase_Queue_Release_Message msg, struct DataBase_Table *table, struct LogMemory *log_memory, key_t log_memory_queue_key)
{
	pid_t pid = SemaphoreRelease(&table->semaphores[msg.key]);
    char timestamp[27] = {0};
    get_time_stamp(timestamp);
	if (pid != SEMAPHORE_QUEUE_EMPTY)
	{
		kill(pid, SIGCONT);
	}
    manager_printf("%s release: key:%d nextpid:%d\n", timestamp, msg.key, pid);
    //sendManagerLogRequest(log_memory_queue_key);
   // sprintf(log_memory->log_message, "%s: manager: release %d %d", timestamp ,msg.key, pid);
    //sendManagerReleaseRequest(log_memory_queue_key);
}

void executeManagerCommand(struct DataBase_Queue_Message msg, int database_table_id, struct LogMemory *log_memory, key_t log_memory_queue_key)
{
    struct DataBase_Table *table = shmat(database_table_id, NULL, 0);
    switch (msg.mtype)
    {
        case DataBase_Client_Add:
            executeManagerAdd(msg.queue_sub_message.add_message, table, log_memory, log_memory_queue_key);
            break;
        case DataBase_Client_Modify:
            executeManagerModify(msg.queue_sub_message.modify_message, table, log_memory, log_memory_queue_key);
            break;
        case DataBase_Client_Acquire:
            executeManagerAcquire(msg.queue_sub_message.acquire_message, table, log_memory, log_memory_queue_key);
            break;
        case DataBase_Client_Release:
            executeManagerRelease(msg.queue_sub_message.release_message, table, log_memory, log_memory_queue_key);
            break;
    default:
        //TODO: log error.
        break;
    }
}

void doManagerWork(int database_table_id, key_t database_message_queue_key, int log_memory_id, key_t log_memory_queue_key)
{
    struct DataBase_Queue_Message msg;
    struct LogMemory *log_memory = shmat(log_memory_id, NULL, 0);
    struct DataBase_Table *table = shmat(database_table_id, NULL, 0);
    char timestamp[27] = {0};
    get_time_stamp(timestamp);
    //sendManagerLogRequest(log_memory_queue_key);
    //sprintf(log_memory->log_message, "%s: manager started, pid: %d", timestamp ,getpid());
    //sendManagerReleaseRequest(log_memory_queue_key);
    
	manager_fp = fopen(manager_log_name, "w");
    fclose(manager_fp);

    manager_printf("%s manager started pid: %d\n", timestamp, getpid());
    while (true)
    {
        if(receiveClientCommand(database_message_queue_key, &msg))
        {
            executeManagerCommand(msg, database_table_id, log_memory, log_memory_queue_key);
        }
    }
}