#define _XOPEN_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/*
    return values:
        0 for error
        1 for exists
        2 for exists and not directory
*/
int folder_exists(char *path)
{
    struct stat buf = { 0 };
    if (!stat(path, &buf))
    {
        if ((buf.st_mode & S_IFMT) == S_IFDIR)
            return 1;
        else
            return 2;
    }
    return 0;
}

/*
    return values:
        0 for error
        1 for success
        2 for already exists
*/
int folder_create(char *path)
{
    if (folder_exists(path) == 1)
        return 2;

    if (mkdir(path, S_IRWXU))
        return 0;
    return 1;
}

int folder_create_name(char *path, char *name)
{
    char new_path[strlen(path) + strlen(name) + 1];
    sprintf(new_path, "%s/%s\n", path, name);

    if (folder_exists(new_path) == 1)
        return 2;

    if (mkdir(new_path, S_IRWXU))
        return 0;
    return 1;
}

int tm_to_str(struct tm time, char *str_out, int str_max)
{
    char *t = asctime(&time);
    if (!t)
        return 0;
    strncpy(str_out, t, str_max);
    return 1;
}

int str_to_tm(char *str_in, struct tm *time_out)
{
    if (!strptime(str_in, "%a %b %d %T %Y ", time_out))
        return 0;
    return 1;
}
