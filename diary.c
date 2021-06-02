#include "diary.h"

int cy, cx;
time_t raw_time;
struct tm today;
struct tm curs_date;
struct tm cal_start;
struct tm cal_end;

// normally leap is every 4 years,
// but is skipped every 100 years,
// unless it is divisible by 400
#define is_leap(yr) ((yr % 400 == 0) || (yr % 4 == 0 && yr % 100 != 0))

void setup_cal_timeframe() {
    raw_time = time(NULL);
    localtime_r(&raw_time, &today);
    curs_date = today;

    cal_start = today;
    cal_start.tm_year -= CONFIG.range;
    cal_start.tm_mon = 0;
    cal_start.tm_mday = 1;
    mktime(&cal_start);

    if (cal_start.tm_wday != CONFIG.weekday) {
        // adjust start date to weekday before 01.01
        cal_start.tm_year--;
        cal_start.tm_mon = 11;
        cal_start.tm_mday = 31 - (cal_start.tm_wday - CONFIG.weekday) + 1;
        mktime(&cal_start);
    }

    cal_end = today;
    cal_end.tm_year += CONFIG.range;
    cal_end.tm_mon = 11;
    cal_end.tm_mday = 31;
    mktime(&cal_end);
}

void draw_wdays(WINDOW* head) {
    int i;
    for (i = CONFIG.weekday; i < CONFIG.weekday + 7; i++) {
        waddstr(head, WEEKDAYS[i % 7]);
        waddch(head, ' ');
    }
    wrefresh(head);
}

void draw_calendar(WINDOW* number_pad, WINDOW* month_pad, const char* diary_dir, size_t diary_dir_size) {
    struct tm i = cal_start;
    char month[10];
    bool has_entry;

    while (mktime(&i) <= mktime(&cal_end)) {
        has_entry = date_has_entry(diary_dir, diary_dir_size, &i);

        if (has_entry)
            wattron(number_pad, A_BOLD);

        wprintw(number_pad, "%2i", i.tm_mday);

        if (has_entry)
            wattroff(number_pad, A_BOLD);

        waddch(number_pad, ' ');

        // print month in sidebar
        if (i.tm_mday == 1) {
            strftime(month, sizeof month, "%b", &i);
            getyx(number_pad, cy, cx);
            mvwprintw(month_pad, cy, 0, "%s ", month);
        }

        i.tm_mday++;
        mktime(&i);
    }
}

bool go_to(WINDOW* calendar, WINDOW* aside, time_t date, int* cur_pad_pos) {
    if (date < mktime(&cal_start) || date > mktime(&cal_end))
        return false;

    int diff_seconds = date - mktime(&cal_start);
    int diff_days = diff_seconds / 60 / 60 / 24;
    int diff_weeks = diff_days / 7;
    int diff_wdays = diff_days % 7;

    localtime_r(&date, &curs_date);

    getyx(calendar, cy, cx);

    // remove the STANDOUT attribute from the day we are leaving
    chtype current_attrs = mvwinch(calendar, cy, cx) & A_ATTRIBUTES;
    // leave every attr as is, but turn off STANDOUT
    current_attrs &= ~A_STANDOUT;
    mvwchgat(calendar, cy, cx, 2, current_attrs, 0, NULL);

    // add the STANDOUT attribute to the day we are entering
    chtype new_attrs =  mvwinch(calendar, diff_weeks, diff_wdays * 3) & A_ATTRIBUTES;
    new_attrs |= A_STANDOUT;
    mvwchgat(calendar, diff_weeks, diff_wdays * 3, 2, new_attrs, 0, NULL);

    if (diff_weeks < *cur_pad_pos)
        *cur_pad_pos = diff_weeks;
    if (diff_weeks > *cur_pad_pos + LINES - 2)
        *cur_pad_pos = diff_weeks - LINES + 2;
    prefresh(aside, *cur_pad_pos, 0, 1, 0, LINES - 1, ASIDE_WIDTH);
    prefresh(calendar, *cur_pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);

    return true;
}

/* Update window 'win' with diary entry from date 'date' */
void display_entry(const char* dir, size_t dir_size, const struct tm* date, WINDOW* win, int width) {
    char path[100];
    char* ppath = path;
    int c;

    // get entry path
    fpath(dir, dir_size, date, &ppath, sizeof path);
    if (ppath == NULL) {
        fprintf(stderr, "Error while retrieving file path for diary reading");
        return;
    }

    wclear(win);

    if (date_has_entry(dir, dir_size, date)) {
        FILE* fp = fopen(path, "r");
        if (fp == NULL) perror("Error opening file");

        wmove(win, 0, 0);
        while((c = getc(fp)) != EOF) waddch(win, c);

        fclose(fp);
    }

    wrefresh(win);
}

/* Writes edit command for 'date' entry to 'rcmd'. '*rcmd' is NULL on error. */
void edit_cmd(const char* dir, size_t dir_size, const struct tm* date, char** rcmd, size_t rcmd_size) {
    // set the edit command to env editor
    if (strlen(CONFIG.editor) + 2 > rcmd_size) {
        fprintf(stderr, "Binary path of default editor too long");
        *rcmd = NULL;
        return;
    }
    strcpy(*rcmd, CONFIG.editor);
    strcat(*rcmd, " ");

    // get path of entry
    char path[100];
    char* ppath = path;
    fpath(dir, dir_size, date, &ppath, sizeof path);

    if (ppath == NULL) {
        fprintf(stderr, "Error while retrieving file path for editing");
        *rcmd = NULL;
        return;
    }

    // concatenate editor command with entry path
    if (strlen(*rcmd) + strlen(path) + 1 > rcmd_size) {
        fprintf(stderr, "Edit command too long");
        return;
    }
    strcat(*rcmd, path);
}

bool date_has_entry(const char* dir, size_t dir_size, const struct tm* i) {
    char epath[100];
    char* pepath = epath;

    // get entry path and check for existence
    fpath(dir, dir_size, i, &pepath, sizeof epath);

    if (pepath == NULL) {
        fprintf(stderr, "Error while retrieving file path for checking entry existence");
        return false;
    }

    return (access(epath, F_OK) != -1);
}

/*
 * Finds the date with a diary entry closest to <current>.
 * Depending on <search_backwards>, it will find the most recent
 * previous date or the oldest next date with an entry. If no
 * entry is found within the calendar size, <current> is returned.
 */
struct tm find_closest_entry(const struct tm current,
                             bool search_backwards,
                             const char* diary_dir,
                             size_t diary_dir_size) {
    time_t end_time = mktime(&cal_end);
    time_t start_time = mktime(&cal_start);

    int step = search_backwards ? -1 : +1;

    struct tm it = current;
    it.tm_mday += step;
    time_t it_time = mktime(&it);

    for( ; it_time >= start_time && it_time <= end_time; it_time = mktime(&it)) {

        if (date_has_entry(diary_dir, diary_dir_size, &it)) {
            return it;
        }
        it.tm_mday += step;
    }

    return current;
}

bool read_config(const char* file_path) {
    char* expaned_value;
    char config_file_path[256];

    expaned_value = expand_path(file_path);
    strcpy(config_file_path, expaned_value);
    free(expaned_value);

    // check if config file is readable
    if( access( config_file_path, R_OK ) != 0 ) {
        fprintf(stderr, "Config file '%s' not readable, skipping\n", config_file_path);
        return false;
    }

    char key_buf[80];
    char value_buf[80];
    char line[256];
    FILE * pfile;

    // read config file line by line
    pfile = fopen(config_file_path, "r");
    while (fgets(line, sizeof line, pfile)) {
        // ignore comment lines
        if (*line == '#' || *line == ';') continue;

        if (sscanf(line, "%s = %s", key_buf, value_buf) == 2) {
            if (strcmp("dir", key_buf) == 0) {
                expaned_value = expand_path(value_buf);
                strcpy(CONFIG.dir, expaned_value);
                free(expaned_value);
            } else if (strcmp("range", key_buf) == 0) {
                CONFIG.range = atoi(value_buf);
            } else if (strcmp("weekday", key_buf) == 0) {
                CONFIG.weekday = atoi(value_buf);
            } else if (strcmp("fmt", key_buf) == 0) {
                CONFIG.fmt = (char *) malloc(strlen(value_buf) + 1 * sizeof(char));
                strcpy(CONFIG.fmt, value_buf);
            } else if (strcmp("editor", key_buf) == 0) {
                CONFIG.editor = (char *) malloc(strlen(value_buf) + 1 * sizeof(char));
                strcpy(CONFIG.editor, value_buf);
            } else if (strcmp("google_tokenfile", key_buf) == 0) {
                expaned_value = expand_path(value_buf);
                strcpy(CONFIG.google_tokenfile, expaned_value);
                free(expaned_value);
            } else if (strcmp("google_clientid", key_buf) == 0) {
                CONFIG.google_clientid = (char *) malloc(strlen(value_buf) + 1 * sizeof(char));
                strcpy(CONFIG.google_clientid, value_buf);
            } else if (strcmp("google_secretid", key_buf) == 0) {
                CONFIG.google_secretid = (char *) malloc(strlen(value_buf) + 1 * sizeof(char));
                strcpy(CONFIG.google_secretid, value_buf);
            } else if (strcmp("google_calendar", key_buf) == 0) {
                CONFIG.google_calendar = (char *) malloc(strlen(value_buf) + 1 * sizeof(char));
                strcpy(CONFIG.google_calendar, value_buf);
            }
        }
    }
    fclose (pfile);
    return true;
}

void usage() {
  printf("Usage : diary [OPTION]... [DIRECTORY]...\n");
  printf("\n");
  printf("Diary, journaling TUI (v%s)\n", DIARY_VERSION);
  printf("Edit journal entries from the command line\n");
  printf("\n");
  printf("Options:\n");
  printf("  -v, --version                 : Print diary version\n");
  printf("  -h, --help                    : Show diary help text\n");
  printf("  -d, --dir           DIARY_DIR : Diary storage directory DIARY_DIR\n");
  printf("  -e, --editor        EDITOR    : Editor to open journal files with\n");
  printf("  -f, --fmt           FMT       : Date and file format, change with care\n");
  printf("  -r, --range         RANGE     : RANGE is the number of years to show before/after today's date\n");
  printf("  -w, --weekday       DAY       : First day of the week, 0 = Sun, 1 = Mon, ..., 6 = Sat\n");
  printf("\n");
  printf("Full docs and keyboard shortcuts: 'man diary'\n");
  printf("or online via: <https://github.com/in0rdr/diary>\n");
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    char* env_var;
    char* config_home;
    char* config_file_path;
    chtype atrs;

    // the diary directory defaults to the diary_dir specified in the config file
    config_home = getenv("XDG_CONFIG_HOME");
    if (config_home == NULL) config_home = XDG_CONFIG_HOME_FALLBACK;
    // concat config home with the file path to the config file
    config_file_path = (char *) calloc(strlen(config_home) + strlen(CONFIG_FILE_PATH) + 1, sizeof(char));
    sprintf(config_file_path, "%s/%s", config_home, CONFIG_FILE_PATH);
    // read config from config file path
    read_config(config_file_path);

    // get diary directory from environment
    env_var = getenv("DIARY_DIR");
    if (env_var != NULL) {
        // if available, overwrite the diary directory with the environment variable
        CONFIG.dir = (char *) calloc(strlen(env_var) + 1, sizeof(char));
        strcpy(CONFIG.dir, env_var);
    }

    // get editor from environment
    env_var = getenv("EDITOR");
    if (env_var != NULL) {
        // if available, overwrite the editor with the environments EDITOR
        CONFIG.editor = (char *) calloc(strlen(env_var) + 1, sizeof(char));
        strcpy(CONFIG.editor, env_var);
    }

    // get locale from environment variable LANG
    // https://www.gnu.org/software/gettext/manual/html_node/Locale-Environment-Variables.html
    env_var = getenv("LANG");
    if (env_var != NULL) {
        // if available, overwrite the editor with the environments locale
        #ifdef __GNU_LIBRARY__
            // references: locale(5) and util-linux's cal.c
            // get the base date, 8-digit integer (YYYYMMDD) returned as char *
            #ifdef _NL_TIME_WEEK_1STDAY
                unsigned long d = (uintptr_t) nl_langinfo(_NL_TIME_WEEK_1STDAY);
            // reference: https://sourceware.org/glibc/wiki/Locales
            // assign a static date value 19971130 (a Sunday)
            #else
                unsigned long d = 19971130;
            #endif
            struct tm base = {
                .tm_sec = 0,
                .tm_min = 0,
                .tm_hour = 0,
                .tm_mday = d % 100,
                .tm_mon = (d / 100) % 100 - 1,
                .tm_year = d / (100 * 100) - 1900
            };
            mktime(&base);
            // weekday is base date's day of the week offset by (_NL_TIME_FIRST_WEEKDAY - 1)
            #ifdef __linux__
                CONFIG.weekday = (base.tm_wday + *nl_langinfo(_NL_TIME_FIRST_WEEKDAY) - 1) % 7;
            #elif defined __MACH__
                CFIndex first_day_of_week;
                CFCalendarRef currentCalendar = CFCalendarCopyCurrent();
                first_day_of_week = CFCalendarGetFirstWeekday(currentCalendar);
                CFRelease(currentCalendar);
                CONFIG.weekday = (base.tm_wday + first_day_of_week - 1) % 7;
            #endif
        #endif
    }

    // get the diary directory via argument, this takes precedence over env/config
    if (argc < 2) {
        if (CONFIG.dir == NULL) {
            fprintf(stderr, "The diary directory must be provided as (non-option) arg, `--dir` arg,\n"
                            "or in the DIARY_DIR environment variable, see `diary --help` or DIARY(1)\n");
            return 1;
        }
    } else {
        int option_char;
        int option_index = 0;

        // define options, see GETOPT(3)
        static const struct option long_options[] = {
            { "version",       no_argument,       0, 'v' },
            { "help",          no_argument,       0, 'h' },
            { "dir",           required_argument, 0, 'd' },
            { "editor",        required_argument, 0, 'e' },
            { "fmt",           required_argument, 0, 'f' },
            { "range",         required_argument, 0, 'r' },
            { "weekday",       required_argument, 0, 'w' },
            { 0,               0,                 0,  0  }
        };

        // read option characters
        while (1) {
            option_char = getopt_long(argc, argv, "vhd:r:w:f:e:", long_options, &option_index);

            if (option_char == -1) {
                break;
            }

            switch (option_char) {
                case 'v':
                    // show program version
                    printf("v%s\n", DIARY_VERSION);
                    return 0;
                    break;
                case 'h':
                    // show help text
                    // printf("see man(1) diary\n");
                    usage();
                    return 0;
                    break;
                case 'd':
                    // set diary directory from option character
                    CONFIG.dir = (char *) calloc(strlen(optarg) + 1, sizeof(char));
                    strcpy(CONFIG.dir, optarg);
                    break;
                case 'r':
                    // set year range from option character
                    CONFIG.range = atoi(optarg);
                    break;
                case 'w':
                    // set first week day from option character
                    fprintf(stderr, "%i\n", atoi(optarg));
                    CONFIG.weekday = atoi(optarg);
                    break;
                case 'f':
                    // set date format from option character
                    CONFIG.fmt = (char *) calloc(strlen(optarg) + 1, sizeof(char));
                    strcpy(CONFIG.fmt, optarg);
                    break;
                case 'e':
                    // set default editor from option character
                    CONFIG.editor = (char *) calloc(strlen(optarg) + 1, sizeof(char));
                    strcpy(CONFIG.editor, optarg);
                    break;
                default:
                    printf("?? getopt returned character code 0%o ??\n", option_char);
            }
        }

        if (optind < argc) {
            // set diary directory from first non-option argv-element,
            // required for backwarad compatibility with diary <= 0.4
            CONFIG.dir = (char *) calloc(strlen(argv[optind]) + 1, sizeof(char));
            strcpy(CONFIG.dir, argv[optind]);
        }
    }

    // check if that directory exists
    DIR* diary_dir_ptr = opendir(CONFIG.dir);
    if (diary_dir_ptr) {
        // directory exists, continue
        closedir(diary_dir_ptr);
    } else if (errno == ENOENT) {
        fprintf(stderr, "The directory '%s' does not exist\n", CONFIG.dir);
        return 2;
    } else {
        fprintf(stderr, "The directory '%s' could not be opened\n", CONFIG.dir);
        return 1;
    }

    setup_cal_timeframe();

    initscr();
    raw();
    curs_set(0);

    WINDOW* header = newwin(1, COLS - CAL_WIDTH - ASIDE_WIDTH, 0, ASIDE_WIDTH + CAL_WIDTH);
    wattron(header, A_BOLD);
    update_date(header, &curs_date);
    WINDOW* wdays = newwin(1, 3 * 7, 0, ASIDE_WIDTH);
    draw_wdays(wdays);

    WINDOW* aside = newpad((CONFIG.range * 2 + 1) * 12 * MAX_MONTH_HEIGHT, ASIDE_WIDTH);
    WINDOW* cal = newpad((CONFIG.range * 2 + 1) * 12 * MAX_MONTH_HEIGHT, CAL_WIDTH);
    keypad(cal, TRUE);
    draw_calendar(cal, aside, CONFIG.dir, strlen(CONFIG.dir));

    int ch, conf_ch;
    int pad_pos = 0;
    int syear = 0, smonth = 0, sday = 0;
    struct tm new_date;
    int prev_width = COLS - ASIDE_WIDTH - CAL_WIDTH;
    int prev_height = LINES - 1;
    size_t diary_dir_size = strlen(CONFIG.dir);

    bool mv_valid = go_to(cal, aside, raw_time, &pad_pos);
    // mark current day
    atrs = winch(cal) & A_ATTRIBUTES;
    wchgat(cal, 2, atrs | A_UNDERLINE, 0, NULL);
    prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);

    WINDOW* prev = newwin(prev_height, prev_width, 1, ASIDE_WIDTH + CAL_WIDTH);
    display_entry(CONFIG.dir, diary_dir_size, &today, prev, prev_width);


    do {
        ch = wgetch(cal);
        // new_date represents the desired date the user wants to go_to(),
        // which may not be a feasible date at all
        new_date = curs_date;
        char ecmd[150];
        char* pecmd = ecmd;
        char pth[100];
        char* ppth = pth;
        char dstr[16];
        edit_cmd(CONFIG.dir, diary_dir_size, &new_date, &pecmd, sizeof ecmd);

        switch(ch) {
            // basic movements
            case 'j':
            case KEY_DOWN:
                new_date.tm_mday += 7;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'k':
            case KEY_UP:
                new_date.tm_mday -= 7;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'l':
            case KEY_RIGHT:
                new_date.tm_mday++;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'h':
            case KEY_LEFT:
                new_date.tm_mday--;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;

            // jump to top/bottom of page
            case 'g':
                new_date = find_closest_entry(cal_start, false, CONFIG.dir, diary_dir_size);
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'G':
                new_date = find_closest_entry(cal_end, true, CONFIG.dir, diary_dir_size);
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;

            // jump backward/forward by a month
            case 'K':
                if (new_date.tm_mday == 1)
                    new_date.tm_mon--;
                new_date.tm_mday = 1;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'J':
                new_date.tm_mon++;
                new_date.tm_mday = 1;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;

            // find specific date
            case 'f':
                wclear(header);
                curs_set(2);
                mvwprintw(header, 0, 0, "Go to date [YYYY-MM-DD]: ");
                if (wscanw(header, "%4i-%2i-%2i", &syear, &smonth, &sday) == 3) {
                    // struct tm.tm_year: years since 1900
                    new_date.tm_year = syear - 1900;
                    // struct tm.tm_mon in range [0, 11]
                    new_date.tm_mon = smonth - 1;
                    new_date.tm_mday = sday;
                    mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                }
                curs_set(0);
                break;
            // today shortcut
            case 't':
                new_date = today;
                mv_valid = go_to(cal, aside, raw_time, &pad_pos);
                break;
            // delete entry
            case 'd':
            case 'x':
                if (date_has_entry(CONFIG.dir, diary_dir_size, &curs_date)) {
                    // get file path of entry and delete entry
                    fpath(CONFIG.dir, diary_dir_size, &curs_date, &ppth, sizeof pth);
                    if (ppth == NULL) {
                        fprintf(stderr, "Error retrieving file path for entry removal");
                        break;
                    }

                    // prepare header for confirmation dialogue
                    wclear(header);
                    curs_set(2);
                    noecho();

                    // ask for confirmation
                    strftime(dstr, sizeof dstr, CONFIG.fmt, &curs_date);
                    mvwprintw(header, 0, 0, "Delete entry '%s'? [Y/n] ", dstr);
                    bool conf = false;
                    while (!conf) {
                        conf_ch = wgetch(header);
                        if (conf_ch == 'y' || conf_ch == 'Y' || conf_ch == '\n') {
                            if (unlink(pth) != -1) {
                                // file successfully deleted, remove entry highlight
                                atrs = winch(cal) & A_ATTRIBUTES;
                                wchgat(cal, 2, atrs & ~A_BOLD, 0, NULL);
                                prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH,
                                         LINES - 1, ASIDE_WIDTH + CAL_WIDTH);
                            }
                        } else if (conf_ch == 27 || conf_ch == 'n') {
                            update_date(header, &curs_date);
                        }
                        break;
                    }

                    echo();
                    curs_set(0);
                }
                break;
            // edit/create a diary entry
            case 'e':
            case '\n':
                if (pecmd == NULL) {
                    fprintf(stderr, "Error retrieving edit command");
                    break;
                }
                curs_set(1);
                system(ecmd);
                curs_set(0);
                keypad(cal, TRUE);

                // mark newly created entry
                if (date_has_entry(CONFIG.dir, diary_dir_size, &curs_date)) {
                    atrs = winch(cal) & A_ATTRIBUTES;
                    wchgat(cal, 2, atrs | A_BOLD, 0, NULL);

                    // refresh the calendar to add highlighting
                    prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH,
                             LINES - 1, ASIDE_WIDTH + CAL_WIDTH);
                }
                break;
            // Move to the previous diary entry
            case 'N':
                new_date = find_closest_entry(new_date, true, CONFIG.dir, diary_dir_size);
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            // Move to the next diary entry
            case 'n':
                new_date = find_closest_entry(new_date, false, CONFIG.dir, diary_dir_size);
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            // Sync entry with CalDAV server
            case 's':
                caldav_sync(&curs_date, header, cal, pad_pos, CONFIG.dir, diary_dir_size);
                break;
            // Sync all with CalDAV server
//            case 'S':
//                time_t end_time = mktime(&cal_end);
//                time_t start_time = mktime(&cal_start);
//                struct tm it = curs_date;
//                time_t it_time = mktime(&it);
//                for( ; it_time >= start_time && it_time <= end_time; it_time = mktime(&it)) {
//                    it.tm_mday++;
//                    if (date_has_entry(diary_dir, diary_dir_size, &it)) {
//                        caldav_sync(&it, header, cal, pad_pos);
//                    }
//                }
//                break;
        }

        if (mv_valid) {
            update_date(header, &curs_date);

            // adjust prev and header width (if terminal was resized in the mean time)
            prev_width = COLS - ASIDE_WIDTH - CAL_WIDTH;
            wresize(prev, prev_height, prev_width);
            wresize(header, 1, prev_width);

            // read the diary
            display_entry(CONFIG.dir, diary_dir_size, &curs_date, prev, prev_width);
        }
    } while (ch != 'q');

    endwin();
    system("clear");
    return 0;
}
