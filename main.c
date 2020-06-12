#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/msg.h>

#include "database.h"
#include "manager.h"
#include "client.h"
#include "config.h"
#include "semaphore.h"
#include "log_memory_manager.h"
#include "query_logger.h"
#include <stdarg.h>
#include "timestamp.h"

#define FORK_ERROR_PID -1
#define FORK_CHILD_PID 0

#define MULTIPLE_FORK_SUCCESS 0
#define MULTIPLE_FORK_ERROR -1

enum e_child_type {
	DataBaseManager = -4,
	Logger = -2,
	QueryLogger = -3,
	LogMemoryManager = -1
}; 

enum e_child_type_index {
	DataBaseManagerIndex = 0,
	LoggerIndex = 1,
	QueryLoggerIndex = 2,
	LogMemoryManagerIndex = 3
};

FILE *parent_fp;
char parent_log_name[] = "parent_log.txt";

void parent_printf(char* fmt, ...)
{
    char str[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(str, fmt, args);
    va_end(args);

    parent_fp = fopen(parent_log_name, "a");
    fputs(str, parent_fp);
    fclose(parent_fp);
}

#define DATABASE_DEFAULT_PROCESS_COUNT 4 //database manager + logger + log memory man + query logger in the future

void doChildWork(int child_type, int database_table_id, key_t database_message_queue_key, int log_memory_id, key_t log_memory_queue_key, key_t query_logger_queue_key)
{
	enum e_child_type ch_type = child_type;
	if (ch_type == DataBaseManager)
	{
		doManagerWork(database_table_id, database_message_queue_key, log_memory_id, log_memory_queue_key);
	}
	else if (ch_type == Logger)
	{
		//TODO
	}
	else if (ch_type == QueryLogger)
	{
		doQueryLoggerWork(query_logger_queue_key);
	}
	else if(ch_type == LogMemoryManager)
	{
		//doLogMemoryManagerWork(log_memory_id, log_memory_queue_key);
	}
	else
	{
		doClientWork(child_type, database_table_id, database_message_queue_key, query_logger_queue_key);
	}
	
	
	exit(0);
}

/*
Forks multiple processes.

returns MULTIPLE_FORK_SUCCESS or success, or MULTIPLE_FORK_ERROR on error.
returns pids in pid_array.
returns number of failed forks in error_count
calls doChildWork for each child with argument specified in child_method_arguments.
int database_table_id;
	int allocate_return;
	
	key_t database_message_queue_key;
*/
int forkMultipleChildern(int process_count, pid_t *pid_array, int *child_method_arguments, int *error_count, int database_table_id, key_t database_message_queue_key, int log_memory_id, key_t log_memory_queue_key, key_t query_logger_queue_key)
{
	int i;
	int return_code = MULTIPLE_FORK_SUCCESS;
	*error_count = 0;
	for(i = 0; i < process_count; i++)
	{
		pid_array[i] = fork();
		if (pid_array[i] == FORK_CHILD_PID)
			doChildWork(child_method_arguments[i], database_table_id, database_message_queue_key, log_memory_id, log_memory_queue_key, query_logger_queue_key);
		else if (pid_array[i] != FORK_ERROR_PID)
			continue;
		else if(return_code != MULTIPLE_FORK_ERROR)
		{
			return_code = MULTIPLE_FORK_ERROR;
			(*error_count)++;
		}
	}
	return return_code;
}
#define EXIT_CODE_INTERNAL_ISSUE -1
#define EXIT_CODE_EXTERNAL_ISSUE -2
#define EXIT_CODE_NOT_FOUND -3
/*
Waits for all childern to exit.
returns inside exit_code_array exit codes of each child,
or: EXIT_CODE_INTERNAL_ISSUE : child had internal error.
or: EXIT_CODE_EXTERNAL_ISSUE : child had external error(didnt exit normally)
or: EXIT_CODE_NOT_FOUND : child pid wasn't found inside pid_array.
*/
void waitForChildernTermination(int process_count, pid_t *pid_array, int *exit_code_array)
{
	pid_t pid;
	int i, j;
	int stat_loc, exit_code;
	
	for(i = 0; i < process_count; i++)
	{
		exit_code_array[i] = EXIT_CODE_NOT_FOUND;
	}
	
	for(i = 0; i < process_count; i++)
	{
		
		pid = wait(&stat_loc);
		if(!WIFEXITED(stat_loc))
        {
			
			exit_code = EXIT_CODE_EXTERNAL_ISSUE;
        }
        else
        {
			exit_code = WEXITSTATUS(stat_loc);
        }
		for(j = 0; j < process_count; j++)
		{
			if(pid_array[j] == pid)
			{
				exit_code_array[j] = exit_code;
				break;
			}
		}
	}
}

#define SHMGET_ERROR -1
bool createDataBaseTableMemory(int *database_table_id)
{
	key_t key = ftok("shmfile",DATABASE_SHAREDMEMORY_PROJECT_ID); 

    struct DataBase_Table *table;
    int shmid = shmget(key,sizeof(struct DataBase_Table),DATABASE_SHAREDMEMORY_PERMISSIONS|IPC_CREAT); 
	if(shmid == SHMGET_ERROR)
		return false;
	
	table = shmat(shmid, NULL, 0);
	memset(table, 0, sizeof(*table));

	for(int i =0; i < DATABASE_ROWS_COUNT; i++)
	{
		SemaphoreInit(&table->semaphores[i]);
	}
	
	*database_table_id = shmid;
	return true;
}


#define MESSAGE_QUEUE_ERROR -1
bool createDataBaseMessageQueue(key_t *database_message_queue_key)
{
	key_t queue_id;
	
	queue_id = msgget(IPC_PRIVATE, DATABASE_MESSAGE_QUEUE_PERMISSIONS);
	
	if(queue_id == -1)
		return false;
	
	*database_message_queue_key = queue_id;
	return true;
}

bool createDataBaseTable(struct DataBase_Table **database_table)
{
	*database_table = malloc(sizeof(struct DataBase_Table));
	if(*database_table == NULL)
		return false;
	return true;
}

bool createLogMemory(int *log_memory_id)
{
	key_t key = ftok("shmfile2",66); 

    struct LogMemory *memory;
    int shmid = shmget(key,sizeof(struct LogMemory),DATABASE_SHAREDMEMORY_PERMISSIONS|IPC_CREAT); 
	if(shmid == SHMGET_ERROR)
		return false;
	
	memory = shmat(shmid, NULL, 0);
	memset(memory, 0, sizeof(*memory));

	/*for(int i =0; i < DATABASE_ROWS_COUNT; i++)
	{
		SemaphoreInit(&table->semaphores[i]);
	}*/
	
	*log_memory_id = shmid;
	return true;
}

bool createLogMemoryQueue(key_t *log_memory_queue_key)
{
	key_t queue_id;
	
	queue_id = msgget(IPC_PRIVATE, 0666);
	
	if(queue_id == -1)
		return false;
	
	*log_memory_queue_key = queue_id;
	return true;
}


bool createQueryLoggerQueue(key_t *query_logger_queue_key)
{
	key_t queue_id;
	
	queue_id = msgget(IPC_PRIVATE, 0666);
	
	if(queue_id == -1)
		return false;
	
	*query_logger_queue_key = queue_id;
	return true;
}
#define ALLOCATE_TABLE_MEMORY_ERROR -1
#define ALLOCATE_MESSAGE_QUEUE_ERROR -2
#define ALLOCATE_TABLE_ERROR -2
#define ALLOCATE_LOGGER_MEMORY_ERROR -3
#define ALLOCATE_LOGGER_QUEUE_ERROR -4
#define ALLOCATE_QUERY_LOGGER_QUEUE_ERROR -5
int allocateDataBaseResources(int *database_table_id, key_t *database_message_queue_key, struct DataBase_Table **database_table, int* log_memory_id, key_t *log_memory_queue_key, key_t *query_logger_queue_key)
{
	if(!createDataBaseTableMemory(database_table_id))
		return ALLOCATE_TABLE_MEMORY_ERROR;

	if(!createDataBaseMessageQueue(database_message_queue_key))
		return ALLOCATE_MESSAGE_QUEUE_ERROR;

	
	if(!createLogMemory(log_memory_id))
		return ALLOCATE_LOGGER_MEMORY_ERROR;

	if(!createLogMemoryQueue(log_memory_queue_key))
		return ALLOCATE_LOGGER_QUEUE_ERROR;

	if(!createQueryLoggerQueue(query_logger_queue_key))
		return ALLOCATE_QUERY_LOGGER_QUEUE_ERROR;
}


void deleteDataBaseTableMemory(int database_table_id)
{
	// destroy the shared memory
	shmctl(database_table_id,IPC_RMID,NULL);
}


void deleteDataBaseMessageQueue(int database_message_queue_key)
{
	// destroy the queue
	msgctl(database_message_queue_key, IPC_RMID, (struct msqid_ds *) 0);
}

void deleteDataBaseTable(struct DataBase_Table *database_table)
{
	free(database_table);
}

void deleteLogMemory(int log_memory_id)
{
	// destroy the shared memory
	shmctl(log_memory_id,IPC_RMID,NULL);
}

void deleteLogMemoryQueue(key_t log_memory_queue_key)
{
	// destroy the queue
	msgctl(log_memory_queue_key, IPC_RMID, (struct msqid_ds *) 0);
}

void deleteQueryLoggerQueue(key_t key)
{
	// destroy the queue
	msgctl(key, IPC_RMID, (struct msqid_ds *) 0);
}
void freeDataBaseResources(int database_table_id, key_t database_message_queue_key, struct DataBase_Table *database_table, int log_memory_id, pid_t log_memory_queue_key, key_t query_log_queue_key)
{
	deleteDataBaseTableMemory(database_table_id);
	deleteDataBaseMessageQueue(database_message_queue_key);
	deleteLogMemory(log_memory_id);
	deleteLogMemoryQueue(log_memory_queue_key);
	deleteQueryLoggerQueue(query_log_queue_key);
}


int main()
{
	int database_client_count;
	int process_count;
	pid_t *pid_array;
	int *child_method_arguments, *exit_code_array;
	int fork_status, fork_error_count;
	int i, next_free_index = 0;
	
	int database_table_id;
	int log_memory_id;
	int allocate_return;
	
	key_t database_message_queue_key;
	key_t log_memory_queue_key;
	key_t query_logger_queue_key;
	struct DataBase_Table *database_table;
	char timestamp[27];
	get_time_stamp(timestamp);

	readParentConfig("parent_config.txt", &database_client_count);
	process_count = database_client_count+DATABASE_DEFAULT_PROCESS_COUNT;

	parent_fp = fopen(parent_log_name, "w");
    fclose(parent_fp);

	parent_printf("%s parent started pid: %d\n", timestamp, getpid());
	parent_printf("%s process_count: %d\n", timestamp, process_count);
	//exit(0);
	pid_array = malloc(sizeof(pid_t) * process_count);
	child_method_arguments = malloc(sizeof(int) * process_count);
	exit_code_array = malloc(sizeof(int) * process_count);
	
	if((allocate_return = allocateDataBaseResources(&database_table_id, &database_message_queue_key, &database_table, &log_memory_id, &log_memory_queue_key, &query_logger_queue_key)) < 0)
	{
		get_time_stamp(timestamp);
		perror("allocation error");
		parent_printf("%s allocation error code: %d\n", timestamp, allocate_return);
		exit(1);
	}
	
	child_method_arguments[DataBaseManagerIndex] = DataBaseManager;
	child_method_arguments[LoggerIndex] = Logger;
	child_method_arguments[QueryLoggerIndex] = QueryLogger;
	child_method_arguments[LogMemoryManagerIndex] = LogMemoryManager;
	
	for(i = LogMemoryManagerIndex+1; i < process_count; i++)
	{
		child_method_arguments[i] = next_free_index++;
	}
	
	fork_status = forkMultipleChildern(process_count, pid_array, child_method_arguments, &fork_error_count, database_table_id, database_message_queue_key, log_memory_id, log_memory_queue_key, query_logger_queue_key);
	
	get_time_stamp(timestamp);
	if(fork_status == MULTIPLE_FORK_ERROR)
	{
		parent_printf("%s Error forking one or more childern!\n", timestamp);
	}
	else
	{
		parent_printf("%s Forked all childern successfully.!\n", timestamp);
	}
	
	waitForChildernTermination(process_count - fork_error_count, pid_array, exit_code_array);
	
	for(i =0; i < process_count - fork_error_count; i++)
	{
		if(exit_code_array[i] <= EXIT_CODE_EXTERNAL_ISSUE)
		{
			get_time_stamp(timestamp);
			parent_printf("%s Child index %d error %d\n",timestamp, i, exit_code_array[i]);
		}
	}
	
	
	free(pid_array);
	free(child_method_arguments);
	free(exit_code_array);
	
	freeDataBaseResources(database_table_id, database_message_queue_key, database_table, log_memory_id, log_memory_queue_key, query_logger_queue_key);
	get_time_stamp(timestamp);
	parent_printf("%s Parent PID %d terminated\n\n", timestamp, getpid());
}
