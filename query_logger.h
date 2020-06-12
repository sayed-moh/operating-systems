#include <sys/msg.h>
#include "database.h"

#define MAX_QUERY_ROWS 70
struct QueryLog
{
    long mtype;
    struct DataBase_Row rows[MAX_QUERY_ROWS];
    int row_count;
    pid_t client_pid;
    char time_stamp[27];
};

void doQueryLoggerWork(key_t query_logger_queue_key);