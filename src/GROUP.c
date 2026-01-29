#include <dirent.h>

#include "calendar.h"
#include "cJSON.h"

unsigned long *create_int_from_json(cJSON *task_ids)
{
    const int array_size = cJSON_GetArraySize(task_ids);

    if (array_size == 0)
        return NULL;
    unsigned long *id_array = malloc(array_size * sizeof(long));
    if (!id_array)
        return NULL;

    for (int i = 0; i < array_size; i++)
    {
        id_array[i] = cJSON_GetNumberValue(cJSON_GetArrayItem(task_ids, i));
    }
    return id_array;
}

static char *create_all_fetch(char *directory)
{
    cJSON *json_array;
    struct dirent *entry;
    char *group_content;
    DIR *dir = opendir(directory);
    if (!dir)
        return NULL;

    json_array = cJSON_CreateArray();
    while ((entry = readdir(dir)) != NULL)
    {
        FILE *f;
        size_t file_length, read_length;
        cJSON *tmp;
        char fullpath[1024], *file_content;
        if (entry->d_name[0] == '.')
            continue;
        snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, entry->d_name);
        
        f = fopen(fullpath, "r");
        fseek(f, 0, SEEK_END);
        file_length = ftell(f);
        fseek(f, 0, SEEK_SET);
        file_content = calloc(file_length + 1, sizeof(char));
        read_length = fread(file_content, sizeof(char), file_length, f);
        fclose(f);
        if (read_length != file_length)
        {
            free(file_content);
            return NULL;
        }
        tmp = cJSON_Parse(file_content);
        free(file_content);
        if (!tmp)
            continue;
        cJSON_AddItemToArray(json_array, tmp);
    }
    closedir(dir);
    group_content = cJSON_Print(json_array);
    cJSON_Delete(json_array);
    return group_content;
}

HttpResponse handle_taskgroups(HttpRequest *request)
{
    Server_Settings settings;
    task_settings t_settings;
    HttpResponse response = { 0 };

    settings = http_get_server_settings();
    t_settings = get_task_settings();
    if (*(request->path + 6) == 0)
        return handle_default_HTTP_POST(request);

    if (!strcmp(request->path + 6, "/edit"))
    {
        task_group new_group = NULL;
        cJSON *json_root, *tmp;
        char *name, *content;
        unsigned int colour;
        unsigned long *id_array = NULL;
        unsigned long group_id;
        char header_data[1024];
        HttpResponse response;

        if (!request->content)
            return return_http_error_code(*request, 400, "Bad Request", settings);

        json_root = cJSON_Parse(request->content);
        if (!json_root)
            return return_http_error_code(*request, 400, "Bad Request", settings);

        tmp = cJSON_GetObjectItem(json_root, "group_id");
        if (!tmp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        group_id = tmp->valuedouble;

        if (group_id != 0)
            new_group = TASK_find_task_group(group_id);

        tmp = cJSON_GetObjectItem(json_root, "name");
        if (!tmp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        name = tmp->valuestring;

        tmp = cJSON_GetObjectItem(json_root, "colour");
        if (!tmp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        colour = strtol(tmp->valuestring + 1, NULL, 16);

        tmp = cJSON_GetObjectItem(json_root, "task_ids");
        if (!tmp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        id_array = create_int_from_json(tmp);

        if (!new_group)
        {
            new_group = TASK_create_task_group(name, NULL, id_array, cJSON_GetArraySize(tmp));
            free(id_array);
        }
        else
        {
            snprintf(new_group->name, sizeof(new_group->name), "%s", name);
            if (new_group->ids && new_group->count > 0)
                free(new_group->ids);
            new_group->ids = id_array;
            new_group->count = cJSON_GetArraySize(tmp);
        }
        new_group->colour = colour;
        content = TASK_save_task_group(new_group);
        cJSON_Delete(json_root);
        free(content);

        content = create_all_fetch(t_settings.groups_folder);

        response.return_code = 200;
        response.content_length = strlen(content);
        strcpy(response.msg_code, "OK");
        strcpy(response.connection, "close");
        strcpy(response.content_type, get_content_type(".json"));
        http_make_basic_headers(response, header_data, sizeof(header_data));
        secure_send(request->connection_info.ssl, header_data, strlen(header_data));
        secure_send(request->connection_info.ssl, content, strlen(content));

        return response;
    }
    if (!strcmp(request->path + 6, "/fetch"))
    {
        char header_data[1024], *content;

        content = create_all_fetch(t_settings.groups_folder);

        response.return_code = 200;
        response.content_length = strlen(content);
        strcpy(response.msg_code, "OK");
        strcpy(response.connection, "close");
        strcpy(response.content_type, get_content_type(".json"));
        http_make_basic_headers(response, header_data, sizeof(header_data));

        secure_send(request->connection_info.ssl, header_data, strlen(header_data));
        secure_send(request->connection_info.ssl, content, strlen(content));
        free(content);
    }
    return handle_default_HTTP_POST(request);
}