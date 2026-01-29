#include <time.h>

/*
    return values:
        0 for error
        1 for exists
        2 for exists and not directory
*/
int folder_exists(char *path);

/*
    return values:
        0 for error
        1 for success
        2 for already exists
*/
int folder_create(char *path);
int folder_create_name(char *path, char *name);

int tm_to_str(struct tm time, char *str_out, int str_max);
int str_to_tm(char *str_in, struct tm *time_out);
