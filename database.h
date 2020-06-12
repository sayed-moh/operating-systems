#pragma once
#include <stdbool.h>
#include <sys/msg.h>
#include "semaphore.h"

#define DATABASE_SHAREDMEMORY_PROJECT_ID 65
#define DATABASE_SHAREDMEMORY_PERMISSIONS 0666
#define DATABASE_MESSAGE_QUEUE_PERMISSIONS 0644
#define DATABASE_NAME_LENGTH 100
#define DATABASE_ROWS_COUNT 1000
struct DataBase_Row
{
	int key;
	char name[DATABASE_NAME_LENGTH];
	int salary;
};

struct DataBase_Table
{
	int row_count;
	struct DataBase_Row rows[DATABASE_ROWS_COUNT];
	struct BinarySemaphore semaphores[DATABASE_ROWS_COUNT];
};

struct DataBase_Queue_Add_Message
{
	char name[DATABASE_NAME_LENGTH];
	int salary;
};

struct DataBase_Queue_Modify_Message
{
	int key;
	bool should_add;
	int amount;
};

struct DataBase_Queue_Acquire_Message
{
	int key;
	int child_pid;
};

struct DataBase_Queue_Release_Message
{
	int key;
};

union DataBase_Queue_Sub_Message
{
	struct DataBase_Queue_Add_Message add_message;
	struct DataBase_Queue_Modify_Message modify_message;
	struct DataBase_Queue_Acquire_Message acquire_message;
	struct DataBase_Queue_Release_Message release_message;
};

struct DataBase_Queue_Message
{
	long mtype;
	union DataBase_Queue_Sub_Message queue_sub_message;
};

struct DataBase_Client_Sleep_Command
{
	int time_in_seconds;
};

enum Salary_Match_Operation
{
	SalaryEquals,
	SalaryGreaterThan,
	SalaryLessThan,
	SalaryGreaterThanOrEqual,
	SalaryLessThanOrEqual
};

struct DataBase_Client_Query_Command
{
	bool search_name;
	bool search_salary;
	bool search_name_prefix;
	char name[DATABASE_NAME_LENGTH];
	int salary;
	enum Salary_Match_Operation salary_operation;
};

union DataBase_Client_Sub_Command
{
	struct DataBase_Queue_Message queue_message;
	struct DataBase_Client_Sleep_Command sleep_command;
	struct DataBase_Client_Query_Command query_command;
};

enum DataBase_Client_Command_Type
{
	DataBase_Client_Sleep,
	DataBase_Client_Print,
	DataBase_Client_Add,
	DataBase_Client_Modify,
	DataBase_Client_Acquire,
	DataBase_Client_Release,
	DataBase_Client_Query
};
struct DataBase_Client_Command
{
	enum DataBase_Client_Command_Type type;
	union DataBase_Client_Sub_Command sub_command;
	
};