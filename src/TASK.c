#include "calendar.h"
#include "cJSON.h"

static int parse_json_dates(cJSON *json, int *year, int *month, int *day)
{
    cJSON *temp;

    if (!json)
        return 0;

    temp = cJSON_GetObjectItem(json, "year");
    if (!temp)
        return 0;
    *year = cJSON_GetNumberValue(temp);

    temp = cJSON_GetObjectItem(json, "month");
    if (!temp)
        return 0;
    *month = cJSON_GetNumberValue(temp);

    temp = cJSON_GetObjectItem(json, "day");
    if (!temp)
        return 0;
    *day = cJSON_GetNumberValue(temp);

    return 1;
}

static int parse_js_dates(char *date, int *year, int *month, int *day)
{
    sscanf(date, "%d-%d-%d", year, month, day);
    return 1;
}

static int parse_js_time(char *time, int *hours, int *minutes)
{
    sscanf(time, "%d:%d", hours, minutes);
    return 1;
}

HttpResponse handle_tasks(HttpRequest *request)
{
    Server_Settings settings;
    task_settings t_settings;
    cJSON *json_root;
    char header_data[1024];
    HttpResponse response = { 0 };

    settings = http_get_server_settings();
    t_settings = get_task_settings();
    if (*(request->path + 5) == 0)
        return handle_default_HTTP_POST(request);

    if (!strcmp(request->path + 5, "/get"))
    {
        int year,month,day;
        char *file_data;

        json_root = cJSON_Parse(request->content);
        if (!json_root)
            return return_http_error_code(*request, 400, "Bad Request", settings);

        if (!parse_json_dates(json_root, &year, &month, &day))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }

        if (!(file_data = TASK_open_task_list(year, month, day, &t_settings)))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 200, "OK", settings);
        }

        strcpy(response.connection, "close");
        strcpy(response.content_type, get_content_type(".json"));
        strcpy(response.msg_code, "OK");
        response.return_code = 200;
        response.content_length = strlen(file_data);
        http_make_basic_headers(response, header_data, sizeof(header_data));

        secure_send(request->connection_info.ssl, header_data, strlen(header_data));
        secure_send(request->connection_info.ssl, file_data, response.content_length);
        free(file_data);
        cJSON_Delete(json_root);
        return response;
    }

    if (!strcmp(request->path + 5, "/status"))
    {
        int year,month,day;
        Task_List list;
        Task t;
        cJSON *tmp;

        if (!request->content)
            return return_http_error_code(*request, 400, "Bad Request", settings);

        json_root = cJSON_Parse(request->content);

        if (!json_root)
            return return_http_error_code(*request, 400, "Bad Request", settings);

        if (!parse_json_dates(json_root, &year, &month, &day))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }

        if(!(tmp = cJSON_GetObjectItem(json_root, "task_id")))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad request", settings);
        }

        if (!(list = TASK_find_task_list(year, month, day)))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 404, "Not Found", settings);
        }

        t = TASK_get_task_from_list(list, cJSON_GetNumberValue(tmp));
        if (!t)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 404, "Not Found", settings);
        }

        if(!(tmp = cJSON_GetObjectItem(json_root, "status")))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad request", settings);
        }

        if (!TASK_SET_status(t, &t_settings, cJSON_GetNumberValue(tmp)))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 500, "Internal Server Error", settings);
        }
        cJSON_Delete(json_root);
        return return_http_error_code(*request, 200, "OK", settings);
    }

    if (!strcmp(request->path + 5, "/add"))
    {
        cJSON *temp;
        char *date_due_str, *current_set_date_str, *title, *description;
        int year, month, day, sort_order, hour, minutes, oldYear, oldMonth, oldDay;
        Task_List list, oldList = NULL;
        struct tm time_due = { 0 };
        Task new_task;
        char header_data[1024], *file_data;
        HttpResponse response;

        json_root = cJSON_Parse(request->content);
        if (!json_root)
            return return_http_error_code(*request, 400, "Bad Request", settings);
        
        temp = cJSON_GetObjectItem(json_root, "date_due");
        if (!temp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        date_due_str = cJSON_GetStringValue(temp);
        parse_js_dates(date_due_str, &year, &month, &day);
        list = TASK_find_task_list(year, month, day);

        temp = cJSON_GetObjectItem(json_root, "title");
        if (!temp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        title = temp->valuestring;

        temp = cJSON_GetObjectItem(json_root, "description");
        if (!temp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        description = temp->valuestring;

        temp = cJSON_GetObjectItem(json_root, "time_due");
        if (!temp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        if (*temp->valuestring)
            parse_js_time(temp->valuestring, &hour, &minutes);
        else
        {
            hour = 23;
            minutes = 59;
        }
        temp = cJSON_GetObjectItem(json_root, "sort_order");
        if (!temp)
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }
        sort_order = (int)strtol(temp->valuestring, NULL, 10);

        printf("before: %d %d %d\n", year, month, day);
        time_due.tm_year = year - 1900;
        time_due.tm_mon = month - 1;
        time_due.tm_mday = day;
        time_due.tm_hour = hour;
        time_due.tm_min = minutes;

        mktime(&time_due);
        printf("after: %d %d %d\n", time_due.tm_year, time_due.tm_mon, time_due.tm_mday);

        temp = cJSON_GetObjectItem(json_root, "current_set_date");
        if (temp)
        {
            current_set_date_str = cJSON_GetStringValue(temp);
            parse_js_dates(current_set_date_str, &oldYear, &oldMonth, &oldDay);
            oldList = TASK_find_task_list(oldYear, oldMonth, oldDay);
        }

        temp = cJSON_GetObjectItem(json_root, "task_id");
        if (temp)
        {
            unsigned long id_numb; 
            id_numb = cJSON_GetNumberValue(temp);
            new_task = TASK_get_task_from_list(oldList ? oldList : list, id_numb);
            
            if (!new_task)
            {
                cJSON_Delete(json_root);
                return return_http_error_code(*request, 400, "Bad Request", settings);
            }
            snprintf(new_task->name, sizeof(new_task->name), "%s", title);
            snprintf(new_task->description, sizeof(new_task->description), "%s", description);
            TASK_SET_time_due(new_task, &t_settings, &time_due);
            new_task->sort_order = sort_order;
            if (!oldList)
                TASK_save_task_list(list, &t_settings, &file_data);
            else
                file_data = TASK_phys_move_task_in_list(oldList, list, new_task, &t_settings);
        }else
        {
            new_task = TASK_create_task(title, description, &time_due);
            new_task->sort_order = sort_order;
            TASK_add_task_to_list(list, new_task);
            TASK_save_task_list(list, &t_settings, &file_data);
        }

        strcpy(response.connection, "close");
        strcpy(response.content_type, get_content_type(".json"));
        strcpy(response.msg_code, "OK");
        response.return_code = 200;
        response.content_length = strlen(file_data);
        http_make_basic_headers(response, header_data, sizeof(header_data));

        secure_send(request->connection_info.ssl, header_data, strlen(header_data));
        secure_send(request->connection_info.ssl, file_data, response.content_length);
        free(file_data);
        cJSON_Delete(json_root);

        return response;
    }
    if (!strcmp(request->path + 5, "/delete"))
    {
        int year,month,day;
        Task_List list;
        cJSON *tmp;

        if (!request->content)
            return return_http_error_code(*request, 400, "Bad Request", settings);

        json_root = cJSON_Parse(request->content);

        if (!json_root)
            return return_http_error_code(*request, 400, "Bad Request", settings);

        if (!parse_json_dates(json_root, &year, &month, &day))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad Request", settings);
        }

        if(!(tmp = cJSON_GetObjectItem(json_root, "task_id")))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 400, "Bad request", settings);
        }

        if (!(list = TASK_find_task_list(year, month, day)))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 404, "Not Found", settings);
        }

        if (!TASK_remove_task_from_list(list, cJSON_GetNumberValue(tmp)))
        {
            cJSON_Delete(json_root);
            return return_http_error_code(*request, 404, "Not Found", settings);
        }

        TASK_save_task_list(list, &t_settings, NULL);
        
        cJSON_Delete(json_root);
        return return_http_error_code(*request, 200, "OK", settings);
    }

    return handle_default_HTTP_POST(request);
}