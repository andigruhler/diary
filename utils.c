#include "utils.h"

config CONFIG = {
    .range = 1,
    .weekday = 1,
    .fmt = "%Y-%m-%d",
    .editor = ""
};

void get_date_str(const struct tm* date, char* date_str, size_t date_str_size, const char* fmt)
{
    strftime(date_str, date_str_size, fmt, date);
}
