#ifndef DIARY
#define DIARY


#ifdef __MACH__
	#include <CoreFoundation/CoreFoundation.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <ncurses.h>
#include <locale.h>
#include <langinfo.h>

#define YEAR_RANGE 1
#define CAL_WIDTH 21
#define ASIDE_WIDTH 4
#define MAX_MONTH_HEIGHT 6

static const char* WEEKDAYS[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};

struct app_state;

void setup_cal_timeframe(struct app_state*);
void draw_wdays(WINDOW* head);
void draw_calendar(struct app_state*, WINDOW* number_pad, WINDOW* month_pad);
void update_date(struct app_state*, WINDOW* header);

bool go_to(struct app_state*, WINDOW* calendar, WINDOW* aside, time_t date, int* cur_pad_pos);
void display_entry(const struct app_state*, const struct tm* date, WINDOW* win, int width);
void edit_cmd(const struct app_state*, const struct tm* date, char** rcmd, size_t rcmd_size);

bool date_has_entry(const struct app_state*, const struct tm* i);
void get_date_str(const struct tm* date, char* date_str, size_t date_str_size);
void fpath(const struct app_state*, const struct tm* date, char** rpath, size_t rpath_size);

#endif
