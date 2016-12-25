#ifndef DIARY
#define DIARY


#ifdef __MACH__
	#include <CoreFoundation/CoreFoundation.h>
#endif
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <ncurses.h>
#include <locale.h>
#include <langinfo.h>
#include <sys/param.h>

#define YEAR_RANGE 1
#define CAL_WIDTH 21
#define ASIDE_WIDTH 4
#define MAX_MONTH_HEIGHT 6

static const char* WEEKDAYS[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};

void setup_cal_timeframe();
void draw_wdays(WINDOW* head);
void draw_calendar(WINDOW* number_pad, WINDOW* month_pad, char* diary_dir, size_t diary_dir_size);
void update_date(WINDOW* header);

bool go_to(WINDOW* calendar, WINDOW* aside, time_t date, int* cur_pad_pos);
void read_entry(const char* dir, size_t dir_size, const struct tm* date,
	        WINDOW* prev, int *y, int* x, int* height, int* width);
void scroll_entry(WINDOW* prev, int* y, int* x, int height, int width);
void edit_cmd(const char* dir, size_t dir_size, const struct tm* date, char** rcmd, size_t rcmd_size);

bool date_has_entry(const char* dir, size_t dir_size, const struct tm* i);
void get_date_str(const struct tm* date, char* date_str, size_t date_str_size);
void fpath(const char* dir, size_t dir_size, const struct tm* date, char** rpath, size_t rpath_size);

#endif
