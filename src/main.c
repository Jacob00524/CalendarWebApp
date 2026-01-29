#include <signal.h>
#include <time.h>

#include "calendar.h"

char *server_settings_path = "server_settings.json";

static void on_sigint(int sig)
{
    (void)sig;
    http_stop_server();
}

void setup_signal_handlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

HttpResponse handle_HTTP_POST(HttpRequest *request)
{
    if (!strncmp(request->path, "/task", 5) != 0)
        return handle_tasks(request);

    if (!strncmp(request->path, "/group", 6) != 0)
        return handle_taskgroups(request);

    return handle_default_HTTP_POST(request);
}


int main()
{
    int server_fd;
    time_t now;
    char time_str[26];
    HttpExtraArgs extra_args = { 0 };

    setup_signal_handlers();

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Starting server at %s", time_str);
    server_fd = init_http_server(server_settings_path);
    if (server_fd == -1)
    {
        http_cleanup_server();
        ERR("There was a problem initializing http server.\n");
        return 1;
    }

    extra_args.POST_handler = handle_HTTP_POST;

    TASK_initialize("save_data");
    start_http_server_listen(server_fd, &extra_args, 1); /* set secure to 1 for https and 0 for http */

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Stopping server at %s", time_str);
    http_cleanup_server();
    return 0;
}
