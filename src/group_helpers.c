#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "calendar.h"

static void settings_add_group(task_group group)
{
    task_settings settings = get_task_settings();
    if (settings.group_count == 0 || !settings.groups)
    {
        settings.groups = malloc(sizeof(group));
        settings.group_count = 0;
    }
    else
        settings.groups = realloc(settings.groups, sizeof(task_group) * (settings.group_count + 1));
    settings.groups[settings.group_count] = group;
    settings.group_count++;
    set_task_settings(settings);
}

task_group TASK_create_task_group(char *name, char *description, unsigned long *tasks, int count)
{
    task_settings settings = get_task_settings();

    task_group group = calloc(1, sizeof(_task_group));
    strncpy(group->name, name, sizeof(group->name) - 1);
    if (description)
        strncpy(group->description, description, sizeof(group->description) - 1);
    if (count > 0 && tasks)
    {
        group->ids = malloc(sizeof(long) * count);
        memcpy(group->ids, tasks, sizeof(long) * count);
        group->count = count;
    }
    group->group_id = ++settings.task_base_offset;
    group->enabled = 1;
    settings_add_group(group);
    return group;
}

void TASK_free_group(task_group group)
{
    if (group->ids)
        free(group->ids);
    free(group);
}

int TASK_add_task_to_group(task_group group, unsigned long task)
{
    if (!group->ids)
        group->ids = malloc(sizeof(long));
    else
        group->ids = realloc(group->ids, sizeof(long) * (group->count + 1));
    group->ids[group->count] = task;
    group->count++;
    return 1;
}

int TASK_remove_task_from_group(task_group group, unsigned long task)
{
    for (int i = 0; i < group->count; i++)
    {
        if (group->ids[i] == task)
        {
            if (i == group->count - 1)
                group->ids = realloc(group->ids, sizeof(long) * (group->count - 1));
            else
            {
                unsigned long last_id = group->ids[group->count - 1];
                group->ids = realloc(group->ids, sizeof(long) * (group->count - 1));
                group->ids[i] = last_id;
            }
            group->count--;
            return 1;
        }
    }
    return 0;
}

char *TASK_save_task_group(task_group group)
{
    char path[1024], *content;
    FILE *f;
    cJSON *json_root, *json_arr;
    task_settings settings = get_task_settings();

    sprintf(path, "%s/%ld.json", settings.groups_folder, group->group_id);
    f = fopen(path, "w");
    if (!f)
        return 0;
    
    json_root = cJSON_CreateObject();
    cJSON_AddStringToObject(json_root, "name", group->name);
    cJSON_AddStringToObject(json_root, "description", group->description);
    cJSON_AddNumberToObject(json_root, "colour", group->colour);
    cJSON_AddNumberToObject(json_root, "enabled", group->enabled);
    cJSON_AddNumberToObject(json_root, "count", group->count);
    cJSON_AddNumberToObject(json_root, "group_id", group->group_id);
    json_arr = cJSON_CreateArray();
    for (int i = 0; i < group->count; i++)
        cJSON_AddItemToArray(json_arr, cJSON_CreateNumber(group->ids[i]));
    cJSON_AddItemToObject(json_root, "ids", json_arr);

    content = cJSON_Print(json_root);
    fwrite(content, sizeof(char), strlen(content), f);
    fflush(f);
    fclose(f);
    cJSON_Delete(json_root);
    return content;
}

static int TASK_load_task_group(task_group group, unsigned long id)
{
    char path[1024], *content;
    size_t length, read_length;
    cJSON *json_root, *temp;
    task_settings settings = get_task_settings();
    FILE *f;

    sprintf(path, "%s/%ld.json", settings.groups_folder, id);
    f = fopen(path, "r");
    if (!f)
        return 0;
    
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);

    content = calloc(length + 1, sizeof(char));
    read_length = fread(content, sizeof(char), length, f);
    fclose(f);
    if (read_length != length)
    {
        free(content);
        return 0;
    }

    json_root = cJSON_Parse(content);
    if (!json_root)
        goto ERR;

    temp = cJSON_GetObjectItem(json_root, "name");
    if (!temp)
        goto ERR;
    snprintf(group->name, sizeof(group->name), "%s", temp->valuestring);
    temp = cJSON_GetObjectItem(json_root, "description");
    if (!temp)
        goto ERR;
    snprintf(group->description, sizeof(group->description), "%s", temp->valuestring);
    temp = cJSON_GetObjectItem(json_root, "colour");
    if (!temp)
        goto ERR;
    group->colour = cJSON_GetNumberValue(temp);
    temp = cJSON_GetObjectItem(json_root, "enabled");
    if (!temp)
        goto ERR;
    group->enabled = cJSON_GetNumberValue(temp);
    temp = cJSON_GetObjectItem(json_root, "count");
    if (!temp)
        goto ERR;
    group->count = cJSON_GetNumberValue(temp);
    temp = cJSON_GetObjectItem(json_root, "group_id");
    if (!temp)
        goto ERR;
    group->group_id = cJSON_GetNumberValue(temp);
    group->ids = malloc(sizeof(long) * group->count);
    temp = cJSON_GetObjectItem(json_root, "ids");
    if (!temp)
        goto ERR;
    for (int i = 0; i < group->count; i++)
    {
        cJSON *id_val = cJSON_GetArrayItem(temp, i);
        group->ids[i] = cJSON_GetNumberValue(id_val);
    }

    free(content);
    cJSON_Delete(json_root);
    return 1;
ERR:
    free(content);
    cJSON_Delete(json_root);
    return 0;
}

task_group TASK_find_task_group(unsigned long group_id)
{
    task_settings settings = get_task_settings();
    task_group new_group;

    /* find a new list that matches the new_time */
    for (int i = 0; i < settings.group_count; i++)
    {
        if (settings.groups[i]->group_id == group_id)
        {
            /* found a fitting task list */
            return settings.groups[i];
        }
    }

    new_group = calloc(1, sizeof(_task_group));
    if (TASK_load_task_group(new_group, group_id))
    {
        settings_add_group(new_group);
        return new_group;
    }
    else
        free(new_group);
    return NULL;
}
