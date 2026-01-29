#pragma once

typedef struct _task_group
{
    char name[256];
    char description[1024];
    unsigned int colour;
    int enabled;

    int count;
    unsigned long *ids;

    unsigned long group_id;
}_task_group;

typedef struct _task_group *task_group;

task_group TASK_create_task_group(char *name, char *description, unsigned long *tasks, int count);
char *TASK_save_task_group(task_group group);
void TASK_free_group(task_group group);
task_group TASK_find_task_group(unsigned long group_id);
