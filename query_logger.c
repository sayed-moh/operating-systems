
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include "query_logger.h"
#include "timestamp.h"

bool receiveQueryQueueCommand(key_t log_memory_queue_key, struct QueryLog *msg)
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
            perror("QueryLogger: Error in msq receive");
        //break;
        return false;
    }
    else
    {
        return true;
    }
}

void doQueryLoggerWork(key_t query_logger_queue_key)
{
    struct QueryLog msg;
    char str[27];
    get_time_stamp(str);
    FILE *fp = fopen("query_log.txt", "w");
    fprintf(fp, "%s: query_logger: started\n", str);
    fclose(fp);
    while (true)
    {
        if(receiveQueryQueueCommand(query_logger_queue_key, &msg))
        {
            fp = fopen("query_log.txt", "a");
            fprintf(fp, "%s: client pid %d query:\n", msg.time_stamp, msg.client_pid);
            if (msg.row_count == 0)
            {
                fputs("\tempty\n", fp);
            }
            
            for (int i = 0; i < msg.row_count; i++)
            {
                fprintf(fp, "\tkey: %d, name %s, salary: %d\n", msg.rows[i].key, msg.rows[i].name, msg.rows[i].salary);
            }
            fclose(fp);
        }
    }
    
}