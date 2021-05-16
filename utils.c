#include "utils.h"

config CONFIG = {
    .range = 1,
    .weekday = 1,
    .fmt = "%Y-%m-%d",
    .editor = "",
    .google_tokenfile = GOOGLE_OAUTH_TOKEN_FILE,
    .google_clientid = GOOGLE_OAUTH_CLIENT_ID,
    .google_secretid = GOOGLE_OAUTH_CLIENT_SECRET,
    .google_calendar = ""
};

void get_date_str(const struct tm* date, char* date_str, size_t date_str_size, const char* fmt)
{
    strftime(date_str, date_str_size, fmt, date);
}
