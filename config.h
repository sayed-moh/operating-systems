#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "database.h"

bool readParentConfig(char * filename, int* num_childern);

bool readClientConfig(char * filename, struct DataBase_Client_Command* cmds, int *cmd_count, int max_cmd_count);