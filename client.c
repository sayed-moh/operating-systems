#include "client.h"
#include "config.h"
#include <signal.h>
#include <sys/shm.h>
#include <stdarg.h>

#include "query_logger.h"
#include "timestamp.h"

#define CLIENT_MAX_CMDS 100
FILE *client_fp;
char child_file_log_name[] = "0_log.txt";

void client_printf(char* fmt, ...)
{
    char str[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(str, fmt, args);
    va_end(args);

    client_fp = fopen(child_file_log_name, "a");
    fputs(str, client_fp);
    fclose(client_fp);
}

bool sendClientMessage(struct DataBase_Queue_Message msg, key_t database_message_queue_key)
{
    int return_value;
    return_value = msgsnd(database_message_queue_key, &msg, sizeof(msg)-sizeof(msg.mtype), !IPC_NOWAIT);
    return return_value != -1;
}

bool sendClientQueryMessage(struct QueryLog msg, key_t database_message_queue_key)
{
    int return_value;
    return_value = msgsnd(database_message_queue_key, &msg, sizeof(msg)-sizeof(msg.mtype), !IPC_NOWAIT);
    return return_value != -1;
}

void executeClientSleep(struct DataBase_Client_Command cmd)
{
    sleep(cmd.sub_command.sleep_command.time_in_seconds);
}

void executeClientPrint()
{
    //TODO: execute print in logger ?
}

void executeClientAdd(struct DataBase_Client_Command cmd, key_t database_message_queue_key)
{
    char str[27];
    get_time_stamp(str);
    cmd.sub_command.queue_message.mtype = (long)cmd.type;
    sendClientMessage(cmd.sub_command.queue_message, database_message_queue_key);
    client_printf("%s client add: %s %d\n", str, cmd.sub_command.queue_message.queue_sub_message.add_message.name, cmd.sub_command.queue_message.queue_sub_message.add_message.salary);
}

void executeClientModify(struct DataBase_Client_Command cmd, key_t database_message_queue_key)
{
    char str[27];
    get_time_stamp(str);
    cmd.sub_command.queue_message.mtype = (long)cmd.type;
    sendClientMessage(cmd.sub_command.queue_message, database_message_queue_key);
    client_printf("%s: client modify command, %d %s %d\n", str, cmd.sub_command.queue_message.queue_sub_message.modify_message.key, (cmd.sub_command.queue_message.queue_sub_message.modify_message.should_add? "+" : "-"), cmd.sub_command.queue_message.queue_sub_message.modify_message.amount);

}


void executeClientAcquire(struct DataBase_Client_Command cmd, key_t database_message_queue_key)
{
    char str[27];
    get_time_stamp(str);
    cmd.sub_command.queue_message.mtype = (long)cmd.type;
    cmd.sub_command.queue_message.queue_sub_message.acquire_message.child_pid = getpid();
    sendClientMessage(cmd.sub_command.queue_message, database_message_queue_key);

    client_printf("%s: client acquire: %d\n", str, cmd.sub_command.queue_message.queue_sub_message.acquire_message.key);
    //pause till acquire granted (signal sent from database manager)
    raise(SIGSTOP);
}

void executeClientRelease(struct DataBase_Client_Command cmd, key_t database_message_queue_key)
{
    char str[27];
    get_time_stamp(str);
    cmd.sub_command.queue_message.mtype = (long)cmd.type;
    sendClientMessage(cmd.sub_command.queue_message, database_message_queue_key);
    client_printf("%s: client release: %d\n", str, cmd.sub_command.queue_message.queue_sub_message.release_message.key);

}

void executeClientQuery(struct DataBase_Client_Command cmd, struct DataBase_Table *table, key_t query_logger_queue_key)
{
    struct DataBase_Client_Query_Command query_cmd = cmd.sub_command.query_command;
    struct QueryLog log_msg;
    struct DataBase_Row query_result[MAX_QUERY_ROWS];
    int matches = 0;
    get_time_stamp(log_msg.time_stamp);
    client_printf("%s: CLient query ", log_msg.time_stamp);
    if(query_cmd.search_name)
    {
        client_printf("name: %s ", query_cmd.name);
        client_printf("prefix: %s ", (query_cmd.search_name_prefix ? "true" : "false"));
    }

    if(query_cmd.search_salary)
    {
        client_printf("salary: ");
        switch (query_cmd.salary_operation)
        {
        case SalaryEquals:
            client_printf("==");
            break;
        case SalaryGreaterThan:
            client_printf(">");
            break;
        case SalaryLessThan:
            client_printf("<");
            break;
        case SalaryGreaterThanOrEqual:
            client_printf(">=");
            break;
        case SalaryLessThanOrEqual:
            client_printf("<=");
            break;
        
        default:
            break;
        }
        
        client_printf(" %d", query_cmd.salary);
    }

    client_printf("\n");

    //TODO: execute query here.
    for (int i = 0; i < table->row_count; i++)
    {
        bool match = query_cmd.search_name || query_cmd.search_salary;
        if (query_cmd.search_name)
        {
            if (query_cmd.search_name_prefix)
            {
                match = (strncasecmp(table->rows[i].name, query_cmd.name, strlen(query_cmd.name)) == 0);
            }
            else
            {
                match = (strcasecmp(table->rows[i].name, query_cmd.name) == 0);
            }
            
        }

        if (query_cmd.search_salary)
        {
            switch (query_cmd.salary_operation)
            {
            case SalaryEquals:
                match = match && (table->rows[i].salary == query_cmd.salary);
                break;
            case SalaryGreaterThan:
                match = match && (table->rows[i].salary > query_cmd.salary);
                break;
            case SalaryLessThan:
                match = match && (table->rows[i].salary < query_cmd.salary);
                break;
            case SalaryGreaterThanOrEqual:
                match = match && (table->rows[i].salary >= query_cmd.salary);
                break;
            case SalaryLessThanOrEqual:
                match = match && (table->rows[i].salary <= query_cmd.salary);
                break;
            
            default:
                match = false;
                break;
            }
        }

        if (match)
        {
            log_msg.rows[matches].key = table->rows[i].key;
            log_msg.rows[matches].salary = table->rows[i].salary;
            strcpy(log_msg.rows[matches].name, table->rows[i].name);
            //client_printf("query match: %d %s %d\n", log_msg.rows[matches].key, log_msg.rows[matches].name, log_msg.rows[matches].salary);
            matches++;
        }
        if (matches>= MAX_QUERY_ROWS)
        {
            break;
        }
        
        
    }

    log_msg.row_count = matches;
    log_msg.client_pid = getpid();
    log_msg.mtype = 1;
    sendClientQueryMessage(log_msg, query_logger_queue_key);
}

void executeClientCommand(struct DataBase_Client_Command cmd, key_t database_message_queue_key, struct DataBase_Table *table, key_t query_logger_queue_key)
{
    switch (cmd.type)
    {
        case DataBase_Client_Sleep:
            executeClientSleep(cmd);
            break;
        case DataBase_Client_Print:
            executeClientPrint();
            break;
        case DataBase_Client_Add:
            executeClientAdd(cmd, database_message_queue_key);
            break;
        case DataBase_Client_Modify:
            executeClientModify(cmd, database_message_queue_key);
            break;
        case DataBase_Client_Acquire:
            executeClientAcquire(cmd, database_message_queue_key);
            break;
        case DataBase_Client_Release:
            executeClientRelease(cmd, database_message_queue_key);
            break;
        case DataBase_Client_Query:
            executeClientQuery(cmd, table, query_logger_queue_key);
            break;
    default:
        //TODO: log error.
        break;
    }
}

void doClientWork(int client_num, int database_table_id, key_t database_message_queue_key, key_t query_logger_queue_key)
{
    struct DataBase_Client_Command cmds[CLIENT_MAX_CMDS] = {0};
	int child_num;
    bool t;
	int cmd_count;
    struct DataBase_Table *table = shmat(database_table_id, NULL, 0);
    char child_file_name[] = "0_config.txt";
    char str[27];
    get_time_stamp(str);
    child_file_name[0] += client_num;
    child_file_log_name[0] += client_num;
	client_fp = fopen(child_file_log_name, "w");
    fclose(client_fp);
    

    t  = readClientConfig(child_file_name, cmds, &cmd_count, CLIENT_MAX_CMDS);
	
    client_printf("%s client %d started pid: %d\n", str, client_num, getpid());

    for(int i = 0; i < cmd_count; i++)
    {
        executeClientCommand(cmds[i], database_message_queue_key, table, query_logger_queue_key);
    }

    exit(0);
}
