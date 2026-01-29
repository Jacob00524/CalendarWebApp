#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "cJSON.h"
#include "calendar.h"

static task_settings g_task_settings;
static pthread_mutex_t server_task_mutex = PTHREAD_MUTEX_INITIALIZER;

task_settings get_task_settings(void)
{
    task_settings copy;
    pthread_mutex_lock(&server_task_mutex);
    copy = g_task_settings;
    pthread_mutex_unlock(&server_task_mutex);
    return copy;
}

void set_task_settings(task_settings settings_in)
{
    pthread_mutex_lock(&server_task_mutex);
    memcpy(&g_task_settings, &settings_in, sizeof(task_settings));
    pthread_mutex_unlock(&server_task_mutex);
}

/*
    Initialize task library, must be callled first.
    returns 1 on sucess and settings_out is filled with task config
    returns 0 on failure
*/
int TASK_initialize(char *save_folder)
{
    if (!save_folder)
        return 0;

    memset(&g_task_settings, 0, sizeof(g_task_settings));
    memcpy(g_task_settings.save_folder, save_folder, strlen(save_folder) + 1);
    g_task_settings.task_base_offset = time(NULL);

    /* only use settings beyond this point. */
    if (!folder_create(g_task_settings.save_folder))
        return 0;
    sprintf(g_task_settings.groups_folder, "%s/%s", g_task_settings.save_folder, "/groups");
    if (!folder_create(g_task_settings.groups_folder))
        return 0;
    return 1;
}

static void settings_add_list(Task_List list)
{
    task_settings settings = get_task_settings();
    if (settings.list_count == 0 || !settings.lists)
    {
        settings.lists = malloc(sizeof(list));
        settings.list_count = 0;
    }
    else
        settings.lists = realloc(settings.lists, sizeof(Task_List) * (settings.list_count + 1));
    settings.lists[settings.list_count] = list;
    settings.list_count++;
    set_task_settings(settings);
}

/*
    creates a task with mandatory default values: name, description, and due date
    other values are initialized to zero.
    returns null on failure.
*/
Task TASK_create_task(char *name, char *description, struct tm *due)
{
    struct tm *ret;
    Task t = calloc(1, sizeof(_task));
    time_t time_now = time(NULL);
    task_settings settings;

    settings = get_task_settings();
    if (!t)
        return NULL;
    t->task_id = ++settings.task_base_offset;
    set_task_settings(settings);

    ret = gmtime(&time_now);
    if (!ret)
    {
        free(t);
        return NULL;
    }
    t->date_created = *ret;
    t->time_due = *due;

    snprintf(t->name, sizeof(t->name), "%s", name);
    snprintf(t->description, sizeof(t->description), "%s", description);
    t->status = 0;
    t->sort_order = 0;
    t->parent = NULL;

    return t;
}

/*
    Creates a task for testing, auto fills description and name for easy use
    returns NULL on failure.
*/
Task TASK_create_test_task()
{
    struct tm *ret, ret_cpy;
    time_t time_now = time(NULL);
    Task t;

    time_now += 60000;
    ret = gmtime(&time_now);
    ret_cpy = *ret;

    t = TASK_create_task("A test Task", "A test task!!!", &ret_cpy);

    return t;
}

/*
    Copies the contents of src into a new allocated task and returns the new task
    returns NULL on failure.
*/
Task TASK_create_copy(Task src)
{
    Task copy = calloc(1, sizeof(_task));
    if (!copy)
        return NULL;

    memcpy(copy, src, sizeof(_task));
    return copy;
}

/* Properly frees task data structure */
void TASK_free_task(Task task)
{
    free(task);
}

static cJSON *create_task_json(Task task)
{
    cJSON *json_root;
    char date[128];

    json_root = cJSON_CreateObject();
    if (!json_root)
        return NULL;

    if (!tm_to_str(task->date_created, date, sizeof(date) - 1))
        goto ERR;
    if (!cJSON_AddStringToObject(json_root, "date_created", date))
        goto ERR;

    if (!tm_to_str(task->time_due, date, sizeof(date) - 1))
        goto ERR;
    if (!cJSON_AddStringToObject(json_root, "time_due", date))
        goto ERR;

    if (!cJSON_AddNumberToObject(json_root, "task_id", task->task_id))
        goto ERR;
    if (!cJSON_AddStringToObject(json_root, "name", task->name))
        goto ERR;
    if (!cJSON_AddStringToObject(json_root, "description", task->description))
        goto ERR;
    if (!cJSON_AddNumberToObject(json_root, "status", task->status))
        goto ERR;
    if (!cJSON_AddNumberToObject(json_root, "sort_order", task->sort_order))
        goto ERR;
    return json_root;

ERR:
    cJSON_Delete(json_root);
    return NULL;
}

static Task create_task_from_json(cJSON *json_root)
{
    cJSON *json_tmp;
    Task t = calloc(1, sizeof(_task));

    if (!t)
        return NULL;

    json_tmp = cJSON_GetObjectItem(json_root, "date_created");
    if (!json_tmp)
        return NULL;
        
    if (!str_to_tm(json_tmp->valuestring, &t->date_created))
        return NULL;

    json_tmp = cJSON_GetObjectItem(json_root, "time_due");
    if (!json_tmp)
        return NULL;
    if (!str_to_tm(json_tmp->valuestring, &t->time_due))
        return NULL;

    json_tmp = cJSON_GetObjectItem(json_root, "task_id");
    if (!json_tmp)
        return NULL;
    t->task_id = json_tmp->valueint;

    json_tmp = cJSON_GetObjectItem(json_root, "name");
    if (!json_tmp)
        return NULL;
    if (!strncpy(t->name, json_tmp->valuestring, sizeof(t->name)))
        return NULL;

    json_tmp = cJSON_GetObjectItem(json_root, "description");
    if (!json_tmp)
        return NULL;
    if (!strncpy(t->description, json_tmp->valuestring, sizeof(t->description)))
        return NULL;

    json_tmp = cJSON_GetObjectItem(json_root, "status");
    if (!json_tmp)
        return NULL;
    t->status = json_tmp->valueint;

    json_tmp = cJSON_GetObjectItem(json_root, "sort_order");
    if (!json_tmp)
        return NULL;
    t->sort_order = json_tmp->valueint;

    return t;
}

/*
    Allocates a new task list. Use this before loading a task list or modifying values of a task list
    Returns NULL on failure.
*/
Task_List TASK_create_new_task_list()
{
    struct tm* time_ret;
    time_t time_now;
    Task_List t_list = calloc(1, sizeof(_task_list));

    if (!t_list)
        return NULL;

    time_now = time(NULL);

    time_ret = gmtime(&time_now);
    t_list->date_created = *time_ret;
    t_list->date_due = t_list->date_created;
    settings_add_list(t_list);

    return t_list;
}

Task TASK_get_task_from_list(Task_List list, unsigned long ID)
{
    for (unsigned long i = 0; i < list->task_count; i++)
    {
        if (list->tasks[i]->task_id == ID)
            return list->tasks[i];
    }
    return NULL;
}

/*
    Adds a task to a task list.
    returns 0 on failure.
    returns 1 on success.
*/
int TASK_add_task_to_list(Task_List list, Task task)
{
    Task task_cpy;

    task_cpy = TASK_create_copy(task);
    if (!task_cpy)
        return 0;

    if (list->task_count == 0 || !list->tasks)
    {
        list->tasks = malloc(sizeof(Task));
        if (!list->tasks)
            return 0;
        list->task_count = 0;
    }
    else
    {
        list->tasks = realloc(list->tasks, sizeof(task) * (list->task_count + 1));
        if (!list->tasks)
            return 0;
    }

    list->tasks[list->task_count] = task_cpy;
    list->task_count += 1;
    task->parent = list;
    return 1;
}

/*
    Removes a task from a list
    returns 0 on failure
    returns 1 on success
*/
int TASK_remove_task_from_list(Task_List list, unsigned long task_ID)
{
    for (unsigned long t = 0; t < list->task_count; t++)
    {
        if (list->tasks[t]->task_id == task_ID)
        {
            if (t != (list->task_count - 1))
            {
                int part2_count = list->task_count - t; /* Dont sub (t + 1) because 1 is added to for loop below */

                TASK_free_task(list->tasks[t]);
                for (int i = 1; i < part2_count; i++)
                    list->tasks[t] = list->tasks[t + i];
                list->task_count--;
                list->tasks = realloc(list->tasks, sizeof(Task) * list->task_count);
                if (!list->tasks)
                    return 0;
            }else
            {
                list->task_count--;
                if (list->task_count == 0)
                {
                    TASK_free_task(list->tasks[0]);
                    return 1;
                }else
                    list->tasks = realloc(list->tasks, sizeof(Task) * list->task_count);
                if (!list->tasks)
                    return 0;
            }
            return 1;
        }
    }
    return 0;
}

/*
    properly frees a task list
*/
static void TASK_free_task_list(Task_List list)
{
    for (unsigned long i = 0; i < list->task_count; i++)
        TASK_free_task(list->tasks[i]);
    free(list->tasks);
    memset(list, 0, sizeof(*list));
    free(list);
}

void TASK_uninitialize()
{
    task_settings settings = get_task_settings();

    for (int i = 0; i < settings.list_count; i++)
        TASK_free_task_list(settings.lists[i]);
    if (settings.lists) free(settings.lists);

    for (int i = 0; i < settings.group_count; i++)
        TASK_free_group(settings.groups[i]);
    if (settings.groups) free(settings.groups);
}

/*
    saves a task list to file.
    file path is a follows <set_path>/year/month/mday.json
    return 0 on failure.
    returns 1 on success.
*/
int TASK_save_task_list(Task_List list, task_settings *settings, char** final_string)
{
    cJSON *json_root;
    char month_path[1124];
    char file_path[1180];
    char date[128];
    char *json_str;
    FILE *f;

    if (!settings)
        return 0;

    sprintf(month_path, "%s/%d", settings->save_folder, list->date_due.tm_year + 1900);
    if (!folder_create(month_path))
        return 0;
    sprintf(month_path, "%s/%d/%d", settings->save_folder, list->date_due.tm_year + 1900, list->date_due.tm_mon + 1);
    if (!folder_create(month_path))
        return 0;

    json_root = cJSON_CreateObject();
    if (!json_root)
        return 0;

    if (!tm_to_str(list->date_created, date, sizeof(date) - 1))
        goto ERR;
    if (!cJSON_AddStringToObject(json_root, "date_created", date))
        goto ERR;

    if (!tm_to_str(list->date_due, date, sizeof(date) - 1))
        goto ERR;
    if (!cJSON_AddStringToObject(json_root, "date_due", date))
        goto ERR;

    if (!cJSON_AddNumberToObject(json_root, "task_priority", list->task_priority))
        goto ERR;
    if (!cJSON_AddNumberToObject(json_root, "task_count", list->task_count))
        goto ERR;

    for (unsigned long i = 0; i < list->task_count; i++)
    {
        char id[128];
        cJSON *task_json = create_task_json(list->tasks[i]);
        if (!task_json)
            goto ERR;
        sprintf(id, "%lu", list->tasks[i]->task_id);
        if (!cJSON_AddItemToObject(json_root, id, task_json))
        {
            cJSON_Delete(task_json); /* ownership was not transferred yet. */
            goto ERR;
        }
    }

    json_str = cJSON_Print(json_root);
    sprintf(file_path, "%s/%d.json", month_path, list->date_due.tm_mday);
    f = fopen(file_path, "w");
    if (!f)
    {
        free(json_str);
        goto ERR;
    }

    fwrite(json_str, sizeof(char), strlen(json_str), f);
    fflush(f);
    fclose(f);
    cJSON_Delete(json_root);
    if (final_string)
        *final_string = json_str;
    else
        free(json_str);
    return 1;

ERR:
    cJSON_Delete(json_root);
    return 0;
}

char *TASK_open_task_list(int year, int month, int day, task_settings *settings)
{
    FILE *f;
    char file_path[1180], *file_content;
    size_t file_length;

    sprintf(file_path, "%s/%d/%d/%d.json", settings->save_folder, year, month, day);
    f = fopen(file_path, "r");
    if (!f)
        return 0;

    fseek(f, 0, SEEK_END);
    file_length = ftell(f);
    fseek(f, 0, SEEK_SET);

    file_content = calloc(file_length + 1, sizeof(char));
    fread(file_content, sizeof(char), file_length, f);
    fclose(f);
    return file_content;
}

/*
    loads a task list from a file <set_path>/year/month/mday.json relative to executable
    task list must be first initialized with TASK_create_task_list
    retutrn 0 on failure.
    returns 1 on success.
*/
static int TASK_load_task_list(Task_List list_out, int year, int month, int day, task_settings *settings)
{
    cJSON *json_root, *json_tmp;
    char *file_content;
    int fixed_year = year, fixed_month = month;

    if (fixed_year < 1000)
    {
        fixed_year += 1900;
        fixed_month += 1;
    }

    file_content = TASK_open_task_list(fixed_year, fixed_month, day, settings);
    if (!file_content)
        return 0;

    memset(list_out, 0, sizeof(*list_out));
    json_root = cJSON_Parse(file_content);
    if (!json_root)
        return 0;

    json_tmp = cJSON_GetObjectItem(json_root, "date_created");
    if (!json_tmp)
        goto ERR;
    if (!str_to_tm(json_tmp->valuestring, &list_out->date_created))
        goto ERR;

    json_tmp = cJSON_GetObjectItem(json_root, "date_due");
    if (!json_tmp)
        goto ERR;
    if (!str_to_tm(json_tmp->valuestring, &list_out->date_due))
        goto ERR;

    json_tmp = cJSON_GetObjectItem(json_root, "task_priority");
    if (!json_tmp)
        goto ERR;
    list_out->task_priority = json_tmp->valueint;

    json_tmp = cJSON_GetObjectItem(json_root, "task_count");
    if (!json_tmp)
        goto ERR;
    list_out->task_count = json_tmp->valueint;

    list_out->tasks = calloc(list_out->task_count, sizeof(Task));
    if (!json_tmp)
        goto ERR;

    int _t = 0;
    for (unsigned long i = 0; i < list_out->task_count; i++)
    {
        json_tmp = json_tmp->next;
        if (!json_tmp)
            break;
        list_out->tasks[_t] = create_task_from_json(json_tmp);
        if (!list_out->tasks[_t])
        {
            free(list_out->tasks);
            goto ERR;
        }
        _t++;
        list_out->tasks[i]->parent = list_out;
    }
    list_out->task_count = _t;

    cJSON_Delete(json_root);
    return 1;

ERR:
    cJSON_Delete(json_root);
    return 0;
}

/*
    Moves task from src to dst.
    Note: The lists must be saved after to have changes show in save file.
    Or use TASK_phys_move_task_in_list.
    returns 1 on success.
    returns 0 on failure.
*/
int TASK_move_task_in_list(Task_List src, Task_List dst, Task task)
{
    for (unsigned long i = 0; i < src->task_count; i++)
    {
        if (src->tasks[i]->task_id == task->task_id)
        {
            if (!TASK_add_task_to_list(dst, task))
                return 0;
            if (!TASK_remove_task_from_list(src, task->task_id))
                return 0;
            return 1;
        }
    }
    return 0;
}

/*
    Moves task from src to dst.
    Note: Saves the lists to file afterwards. Use TASK_move_task_in_list to move without a save.
    returns 1 on success.
    returns 0 on failure.
*/
char *TASK_phys_move_task_in_list(Task_List src, Task_List dst, Task task, task_settings *settings)
{
    char *return_str = NULL;
    if (!TASK_move_task_in_list(src, dst, task))
        return NULL;
    if (!TASK_save_task_list(src, settings, NULL))
        return NULL;
    if (!TASK_save_task_list(dst, settings, &return_str))
        return NULL;
    return return_str;
}

int TASK_SET_date_created(Task task, task_settings *settings, struct tm *new_date)
{
    task->date_created = *new_date;
    if (task->parent)
        return TASK_save_task_list(task->parent, settings, NULL);
    return 1;
}

Task_List TASK_find_task_list(int year, int month, int day)
{
    Task_List new_list;
    task_settings settings = get_task_settings();

    int fixed_year = year, fixed_month = month;

    if (fixed_year > 1000)
    {
        fixed_year -= 1900;
        fixed_month -= 1;
    } else if (fixed_month == 12)
        fixed_month -= 1;

    /* find a new list that matches the new_time */
    for (int i = 0; i < settings.list_count; i++)
    {
        if (fixed_year == settings.lists[i]->date_due.tm_year && fixed_month == settings.lists[i]->date_due.tm_mon && day == settings.lists[i]->date_due.tm_mday)
        {
            /* found a fitting task list */
            return settings.lists[i];
        }
    }

    new_list = TASK_create_new_task_list();
    if (TASK_load_task_list(new_list, fixed_year, fixed_month, day, &settings))
        return new_list;
    new_list->date_due.tm_year = fixed_year;
    new_list->date_due.tm_mon = fixed_month;
    new_list->date_due.tm_mday = day;
    mktime(&new_list->date_due);
    set_task_settings(settings);
    return new_list;
}

int TASK_SET_time_due(Task task, task_settings *settings, struct tm *new_time)
{
    if (!task->parent)
    {
        task->time_due = *new_time;
        return 1;
    }

    task->time_due = *new_time;
    return TASK_save_task_list(task->parent, settings, NULL);
    /*if (new_time->tm_year == task->parent->date_due.tm_year && new_time->tm_mon == task->parent->date_due.tm_mon && new_time->tm_mday == task->parent->date_due.tm_mday)
    {
        task->time_due = *new_time;
        return TASK_save_task_list(task->parent, settings, NULL);
    }else
    {
        Task_List new_list = TASK_find_task_list(new_time->tm_year, new_time->tm_mon, new_time->tm_mday, settings);
        char *ret = TASK_phys_move_task_in_list(task->parent, new_list, task, settings);
        free(ret);
    }*/   
    return 1;
}
int TASK_SET_id(Task task, task_settings *settings, unsigned long new_id)
{
    task->task_id = new_id;
    if (task->parent)
        return TASK_save_task_list(task->parent, settings, NULL);
    return 1;
}
int TASK_SET_name(Task task, task_settings *settings, char *new_name)
{
    strncpy(task->name, new_name, sizeof(task->name));
    if (task->parent)
        return TASK_save_task_list(task->parent, settings, NULL);
    return 1;
}
int TASK_SET_description(Task task, task_settings *settings, char *new_description)
{
    strncpy(task->name, new_description, sizeof(task->name));
    if (task->parent)
        return TASK_save_task_list(task->parent, settings, NULL);
    return 1;
}
int TASK_SET_status(Task task, task_settings *settings, int new_status)
{
    task->status = new_status;
    if (task->parent)
        return TASK_save_task_list(task->parent, settings, NULL);
    return 1;
}
int TASK_SET_sort_order(Task task, task_settings *settings, int new_order)
{
    task->sort_order = new_order;
    if (task->parent)
        return TASK_save_task_list(task->parent, settings, NULL);
    return 1;
}

/*
typedef struct _task_list
{
    struct tm date_created;
    struct tm date_due;
    int task_priority;

    Task *tasks;
    unsigned long task_count; number of valid tasks pointed to by tasks
}_task_list;
*/

int TASK_SET_list_priority(Task_List list, task_settings *settings, int new_priority)
{
    list->task_priority = new_priority;
    return TASK_save_task_list(list, settings, NULL);
}
