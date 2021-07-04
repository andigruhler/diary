#ifndef DIARY_H
#define DIARY_H

#ifdef __MACH__
	#include <CoreFoundation/CoreFoundation.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <ncurses.h>
#include <locale.h>
#include <langinfo.h>
#include "utils.h"
#include "caldav.h"

#define XDG_CONFIG_HOME_FALLBACK "~/.config"
#define CONFIG_FILE_PATH "diary/diary.cfg"
#define DIARY_VERSION "0.6-unstable"

static const char* WEEKDAYS[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};

void setup_cal_timeframe();
void draw_wdays(WINDOW* head);
void draw_calendar(WINDOW* number_pad, WINDOW* month_pad, const char* diary_dir, size_t diary_dir_size);

bool go_to(WINDOW* calendar, WINDOW* aside, time_t date, int* cur_pad_pos);
void display_entry(const char* dir, size_t dir_size, const struct tm* date, WINDOW* win, int width);
void edit_cmd(const char* dir, size_t dir_size, const struct tm* date, char** rcmd, size_t rcmd_size);

bool date_has_entry(const char* dir, size_t dir_size, const struct tm* i);

#endif