#include "config.h"

bool readParentConfig(char * filename, int* num_childern)
{
	FILE * pFile;
    char s[100];
	pFile = fopen (filename , "r");
    if (pFile == NULL)
		return false;
	
	fgets(s , 100 , pFile);
	fclose (pFile);	
	*num_childern = atoi(s);
	return true;
}

bool readClientConfig(char * filename, struct DataBase_Client_Command* cmds, int *cmd_count, int max_cmd_count)
{
	FILE * pFile;
    char s[100];
	char modify_type;
	struct DataBase_Client_Command* cmd;
	pFile = fopen (filename , "r");
    if (pFile == NULL)
		return false;
	*cmd_count = 0;
	while ( fgets (s , 100 , pFile) != NULL )
    {
		if(*cmd_count > max_cmd_count)
			break;
		if(strncasecmp(s, "sleep", 5) == 0)
		{
			cmd = &cmds[(*cmd_count)++];
			sscanf(s, "sleep %d", &cmd->sub_command.sleep_command.time_in_seconds);
			cmd->type = DataBase_Client_Sleep;
		}
		else if(strncasecmp(s, "print", 5) == 0)
		{
			cmd = &cmds[(*cmd_count)++];
			cmd->type = DataBase_Client_Print;
		}
		else if(strncasecmp(s, "add", 3) == 0)
		{
			cmd = &cmds[(*cmd_count)++];
			sscanf(s, "add %s %d", cmd->sub_command.queue_message.queue_sub_message.add_message.name, &cmd->sub_command.queue_message.queue_sub_message.add_message.salary);
			cmd->type = DataBase_Client_Add;
		}
		else if(strncasecmp(s, "acquire", 7) == 0)
		{
			cmd = &cmds[(*cmd_count)++];
			sscanf(s, "acquire %d", &cmd->sub_command.queue_message.queue_sub_message.acquire_message.key);
			cmd->type = DataBase_Client_Acquire;
		}
		else if(strncasecmp(s, "modify", 6) == 0)
		{
			cmd = &cmds[(*cmd_count)++];
			sscanf(s, "modify %d %c %d", &cmd->sub_command.queue_message.queue_sub_message.modify_message.key, &modify_type, &cmd->sub_command.queue_message.queue_sub_message.modify_message.amount);
			cmd->sub_command.queue_message.queue_sub_message.modify_message.should_add = modify_type == '+';
			cmd->type = DataBase_Client_Modify;
		}
		else if(strncasecmp(s, "release", 7) == 0)
		{
			cmd = &cmds[(*cmd_count)++];
			sscanf(s, "release %d", &cmd->sub_command.queue_message.queue_sub_message.release_message.key);
			cmd->type = DataBase_Client_Release;
		}
		else if(strncasecmp(s, "query", 5) == 0)
		{
			char *name_begin, *salary_begin;
			char salary_operator[3] = {0};
			int search_prefix;
			enum Salary_Match_Operation salary_operation;
			cmd = &cmds[(*cmd_count)++];
			name_begin = strcasestr(s, "name");
			salary_begin = strcasestr(s, "salary");
			if(name_begin != NULL)
			{
				cmd->sub_command.query_command.search_name = true;
				sscanf(name_begin, "name %d %s", &search_prefix, cmd->sub_command.query_command.name);
				cmd->sub_command.query_command.search_name_prefix = search_prefix;
			}

			if(salary_begin != NULL)
			{
				cmd->sub_command.query_command.search_salary = true;
				sscanf(salary_begin, "salary %s %d", salary_operator, &cmd->sub_command.query_command.salary);
				if(strcasecmp(salary_operator, "==") == 0)
				{
					salary_operation = SalaryEquals;
				}
				else if(strcasecmp(salary_operator, ">=") == 0)
				{
					salary_operation = SalaryGreaterThanOrEqual;
				}
				else if(strcasecmp(salary_operator, "<=") == 0)
				{
					salary_operation = SalaryLessThanOrEqual;
				}
				else if(strcasecmp(salary_operator, ">") == 0)
				{
					salary_operation = SalaryGreaterThan;
				}
				else if(strcasecmp(salary_operator, "<") == 0)
				{
					salary_operation = SalaryLessThan;
				}
				else
				{
					printf("client config error: unknow salary operator %s!\n", salary_operator);
				}

				cmd->sub_command.query_command.salary_operation = salary_operation;
				
			}

			
			cmd->type = DataBase_Client_Query;
		}
	}
	fclose (pFile);
	return true;
}