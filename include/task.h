#include <time.h>

#include "group.h"

struct _task;
struct _task_list;

typedef struct _task *Task;
typedef struct _task_list *Task_List;

typedef struct _task
{
    struct tm date_created;
    struct tm time_due;
    
    unsigned long task_id;
    char name[256];
    char description[256];
    int status; /* 1 = complete, 0 = not complete */
    int sort_order; /* 0 = highest priority */

    Task_List parent;
}_task;

typedef struct _task_list
{
    struct tm date_created;
    struct tm date_due;
    int task_priority;

    Task *tasks;
    unsigned long task_count; /* number of valid tasks pointed to by tasks */
}_task_list;

typedef struct task_settings
{
    char save_folder[512];
    char groups_folder[530];
    unsigned long task_base_offset;
    int list_count;
    int group_count;
    Task_List *lists; /* keep track of all loaded lists */
    task_group *groups; /* keep track of all loaded lists */
}task_settings;

/*
    Initialize task library, must be callled first.
    returns 1 on sucess and settings_out is filled with task config
    returns 0 on failure
*/
int TASK_initialize(char *save_folder);
void TASK_uninitialize();
/*
    creates a task with mandatory default values: name, description, and due date
    other values are initialized to zero.
    returns null on failure.
*/
Task TASK_create_task(char *name, char *description, struct tm *due);

/*
    Creates a task for testing, auto fills description and name for easy use
    returns NULL on failure.
*/
Task TASK_create_test_task();

/*
    Copies the contents of src into a new allocated task and returns the new task
    returns NULL on failure.
*/
Task TASK_create_copy(Task src);

/* Properly frees task data structure */
void TASK_free_task(Task task);

/*
    Allocates a new task list. Use this before loading a task list or modifying values of a task list
    Returns NULL on failure.
*/
Task_List TASK_create_new_task_list();

/* 
    find a task from a task list using its ID
    returns NULL on failure
    returns Task on success 
*/
Task TASK_get_task_from_list(Task_List list, unsigned long ID);

/*
    Adds a task to a task list.
    returns 0 on failure.
    returns 1 on success.
*/
int TASK_add_task_to_list(Task_List list, Task task);

/*
    Removes a task from a list
    returns 0 on failure
    returns 1 on success
*/
int TASK_remove_task_from_list(Task_List list, unsigned long task_ID);

/*
    saves a task list to file.
    file path is a follows <set_path>/year/month/mday.json
    return 0 on failure.
    returns 1 on success.
*/
int TASK_save_task_list(Task_List list, task_settings *settings, char** final_string);

/* 
    opens the task list file and returns it as a string
    Caller is responsible to free it
*/
char *TASK_open_task_list(int year, int month, int day, task_settings *settings);

/*
    Moves task from src to dst.
    Note: The lists must be saved after to have changes show in save file.
    Or use TASK_phys_move_task_in_list.
    returns 1 on success.
    returns 0 on failure.
*/
int TASK_move_task_in_list(Task_List src, Task_List dst, Task task);

/*
    Moves task from src to dst.
    Note: Saves the lists to file afterwards. Use TASK_move_task_in_list to move without a save.
    returns 1 on success.
    returns 0 on failure.
*/
char *TASK_phys_move_task_in_list(Task_List src, Task_List dst, Task task, task_settings *settings);

int TASK_SET_date_created(Task task, task_settings *settings, struct tm *new_date);
int TASK_SET_time_due(Task task, task_settings *settings, struct tm *new_time);
int TASK_SET_id(Task task, task_settings *settings, unsigned long new_id);
int TASK_SET_name(Task task, task_settings *settings, char *new_name);
int TASK_SET_description(Task task, task_settings *settings, char *new_description);
int TASK_SET_status(Task task, task_settings *settings, int new_status);
int TASK_SET_sort_order(Task task, task_settings *settings, int new_order);

int TASK_SET_list_priority(Task_List list, task_settings *settings, int new_priority);

Task_List TASK_find_task_list(int year, int month, int day);

task_settings get_task_settings(void);
void set_task_settings(task_settings settings_in);
