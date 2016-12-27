#include "diary.h"

struct app_state {
    int cy, cx;
    time_t raw_time;
    struct tm today;
    struct tm curs_date;
    struct tm cal_start;
    struct tm cal_end;
    char diary_dir[80];
    size_t diary_dir_size;
};
// 0 = Sunday, 1 = Monday, ..., 6 = Saturday
int first_weekday = 1;

#define DATE_FMT "%Y-%m-%d"

// normally leap is every 4 years,
// but is skipped every 100 years,
// unless it is divisible by 400
#define is_leap(yr) ((yr % 400 == 0) || (yr % 4 == 0 && yr % 100 != 0))

void setup_cal_timeframe(struct app_state* s)
{
    s->raw_time = time(NULL);
    localtime_r(&s->raw_time, &s->today);
    s->curs_date = s->today;

    s->cal_start = s->today;
    s->cal_start.tm_year -= YEAR_RANGE;
    s->cal_start.tm_mon = 0;
    s->cal_start.tm_mday = 1;
    mktime(&s->cal_start);

    if (s->cal_start.tm_wday != first_weekday) {
        // adjust start date to first_weekday before 01.01
        s->cal_start.tm_year--;
        s->cal_start.tm_mon = 11;
        s->cal_start.tm_mday = 31 - (s->cal_start.tm_wday - first_weekday) + 1;
        mktime(&s->cal_start);
    }

    s->cal_end = s->today;
    s->cal_end.tm_year += YEAR_RANGE;
    s->cal_end.tm_mon = 11;
    s->cal_end.tm_mday = 31;
    mktime(&s->cal_end);
}

void draw_wdays(WINDOW* head)
{
    int i;
    for (i = first_weekday; i < first_weekday + 7; i++) {
        waddstr(head, WEEKDAYS[i % 7]);
        waddch(head, ' ');
    }
    wrefresh(head);
}

void draw_calendar(struct app_state* s,
                   WINDOW* number_pad,
                   WINDOW* month_pad)
{
    struct tm i = s->cal_start;
    char month[10];
    bool has_entry;

    while (mktime(&i) <= mktime(&s->cal_end)) {
        has_entry = date_has_entry(s, &i);

        if (has_entry)
            wattron(number_pad, A_BOLD);

        wprintw(number_pad, "%2i", i.tm_mday);

        if (has_entry)
            wattroff(number_pad, A_BOLD);

        waddch(number_pad, ' ');

        // print month in sidebar
        if (i.tm_mday == 1) {
            strftime(month, sizeof month, "%b", &i);
            getyx(number_pad, s->cy, s->cx);
            mvwprintw(month_pad, s->cy, 0, "%s ", month);
        }

        i.tm_mday++;
        mktime(&i);
    }
}

/* Update the header with the cursor date */
void update_date(struct app_state* s, WINDOW* header)
{
    char dstr[16];
    mktime(&s->curs_date);
    get_date_str(&s->curs_date, dstr, sizeof dstr);

    wclear(header);
    mvwaddstr(header, 0, 0, dstr);
    wrefresh(header);
}

bool go_to(struct app_state* s,
           WINDOW* calendar,
           WINDOW* aside,
           time_t date,
           int* cur_pad_pos)
{
    if (date < mktime(&s->cal_start) || date > mktime(&s->cal_end))
        return false;

    int diff_seconds = date - mktime(&s->cal_start);
    int diff_days = diff_seconds / 60 / 60 / 24;
    int diff_weeks = diff_days / 7;
    int diff_wdays = diff_days % 7;

    localtime_r(&date, &s->curs_date);

    getyx(calendar, s->cy, s->cx);

    // remove the STANDOUT attribute from the day we are leaving
    chtype current_attrs = mvwinch(calendar, s->cy, s->cx) & A_ATTRIBUTES;
    // leave every attr as is, but turn off STANDOUT
    current_attrs &= ~A_STANDOUT;
    mvwchgat(calendar, s->cy, s->cx, 2, current_attrs, 0, NULL);

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
void display_entry(const struct app_state* s,
                   const struct tm* date,
                   WINDOW* win,
                   int width)
{
    char path[100];
    char* ppath = path;
    int c;

    // get entry path
    fpath(s, date, &ppath, sizeof path);
    if (ppath == NULL) {
        fprintf(stderr, "Error while retrieving file path for diary reading");
        return;
    }

    wclear(win);

    if (date_has_entry(s, date)) {
        FILE* fp = fopen(path, "r");
        if (fp == NULL) perror("Error opening file");

        wmove(win, 0, 0);
        while((c = getc(fp)) != EOF) waddch(win, c);

        fclose(fp);
    }

    wrefresh(win);
}

/* Writes edit command for 'date' entry to 'rcmd'. '*rcmd' is NULL on error. */
void edit_cmd(const struct app_state* s,
              const struct tm* date,
              char** rcmd,
              size_t rcmd_size)
{
    // get editor from environment
    char* editor = getenv("EDITOR");
    if (editor == NULL) {
        fprintf(stderr, "'EDITOR' environment variable not set");
        *rcmd = NULL;
        return;
    }

    // set the edit command to env editor
    if (strlen(editor) + 2 > rcmd_size) {
        fprintf(stderr, "Binary path of default editor too long");
        *rcmd = NULL;
        return;
    }
    strcpy(*rcmd, editor);
    strcat(*rcmd, " ");

    // get path of entry
    char path[100];
    char* ppath = path;
    fpath(s, date, &ppath, sizeof path);

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

bool date_has_entry(const struct app_state* s, const struct tm* i)
{
    char epath[100];
    char* pepath = epath;

    // get entry path and check for existence
    fpath(s, i, &pepath, sizeof epath);

    if (pepath == NULL) {
        fprintf(stderr, "Error while retrieving file path for checking entry existence");
        return false;
    }

    return (access(epath, F_OK) != -1);
}

void get_date_str(const struct tm* date, char* date_str, size_t date_str_size)
{
    strftime(date_str, date_str_size, DATE_FMT, date);
}

/* Writes file path for 'date' entry to 'rpath'. '*rpath' is NULL on error. */
void fpath(const struct app_state* s, const struct tm* date, char** rpath, size_t rpath_size)
{
    // check size of result path
    if (s->diary_dir_size + 1 > rpath_size) {
        fprintf(stderr, "Directory path too long");
        *rpath = NULL;
        return;
    }

    // add path of the diary dir to result path
    strcpy(*rpath, s->diary_dir);

    // check for terminating '/' in path
    if (s->diary_dir[s->diary_dir_size - 1] != '/') {
        // check size again to accommodate '/'
        if (s->diary_dir_size + 1 > rpath_size) {
            fprintf(stderr, "Directory path too long");
            *rpath = NULL;
            return;
        }
        strcat(*rpath, "/");
    }

    char dstr[16];
    get_date_str(date, dstr, sizeof dstr);

    // append date to the result path
    if (strlen(*rpath) + strlen(dstr) > rpath_size) {
        fprintf(stderr, "File path too long");
        *rpath = NULL;
        return;
    }
    strcat(*rpath, dstr);
}

/*
 * Finds the date with a diary entry closest to <current>.
 * Depending on <search_backwards>, it will find the most recent
 * previous date or the oldest next date with an entry. If no
 * entry is found within the calendar size, <current> is returned.
 */
struct tm find_closest_entry(struct app_state* s,
                             const struct tm current,
                             bool search_backwards)
{
    time_t end_time = mktime(&s->cal_end);
    time_t start_time = mktime(&s->cal_start);

    int step = search_backwards ? -1 : +1;

    struct tm it = current;
    time_t it_time = mktime(&it);

    for( ; it_time >= start_time && it_time <= end_time; it_time = mktime(&it)) {
        it.tm_mday += step;

        if (date_has_entry(s, &it)) {
            return it;
        }

    }

    return current;
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    struct app_state state = { 0 };
    char* env_var;
    chtype atrs;

    // get the diary directory via environment variable or argument
    // if both are given, the argument takes precedence
    if (argc < 2) {
        // the diary directory is not specified via command line argument
        // use the environment variable if available
        env_var = getenv("DIARY_DIR");
        if (env_var == NULL) {
            fprintf(stderr, "The diary directory must be given as command line "
                            "argument or in the DIARY_DIR environment variable\n");
            return 1;
        }

        if (strlen(env_var) + 1 > sizeof state.diary_dir) {
            fprintf(stderr, "Diary directory path too long\n");
            return 1;
        }
        strcpy(state.diary_dir, env_var);
    } else {
        if (strlen(argv[1]) + 1 > sizeof state.diary_dir) {
            fprintf(stderr, "Diary directory path too long\n");
            return 1;
        }
        strcpy(state.diary_dir, argv[1]);
    }

    // check if that directory exists
    DIR* diary_dir_ptr = opendir(state.diary_dir);
    if (diary_dir_ptr) {
        // directory exists, continue
        closedir(diary_dir_ptr);
    } else if (errno == ENOENT) {
        fprintf(stderr, "The directory '%s' does not exist\n", state.diary_dir);
        return 2;
    } else {
        fprintf(stderr, "The directory '%s' could not be opened\n", state.diary_dir);
        return 1;
    }
    state.diary_dir_size = strlen(state.diary_dir);

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
        // first_weekday is base date's day of the week offset by (_NL_TIME_FIRST_WEEKDAY - 1)
        #ifdef __linux__
            first_weekday = (base.tm_wday + *nl_langinfo(_NL_TIME_FIRST_WEEKDAY) - 1) % 7;
        #elif defined __MACH__
            CFIndex first_day_of_week;
            CFCalendarRef currentCalendar = CFCalendarCopyCurrent();
            first_day_of_week = CFCalendarGetFirstWeekday(currentCalendar);
            CFRelease(currentCalendar);
            first_weekday = (base.tm_wday + first_day_of_week - 1) % 7;
        #endif
    #endif

    setup_cal_timeframe(&state);

    initscr();
    raw();
    curs_set(0);

    WINDOW* header = newwin(1, COLS - CAL_WIDTH - ASIDE_WIDTH, 0, ASIDE_WIDTH + CAL_WIDTH);
    wattron(header, A_BOLD);
    update_date(&state, header);
    WINDOW* wdays = newwin(1, 3 * 7, 0, ASIDE_WIDTH);
    draw_wdays(wdays);

    WINDOW* aside = newpad((YEAR_RANGE * 2 + 1) * 12 * MAX_MONTH_HEIGHT, ASIDE_WIDTH);
    WINDOW* cal = newpad((YEAR_RANGE * 2 + 1) * 12 * MAX_MONTH_HEIGHT, CAL_WIDTH);
    keypad(cal, TRUE);
    draw_calendar(&state, cal, aside);

    int ch, conf_ch;
    int pad_pos = 0;
    int syear = 0, smonth = 0, sday = 0;
    struct tm new_date;
    int prev_width = COLS - ASIDE_WIDTH - CAL_WIDTH;
    int prev_height = LINES - 1;

    bool mv_valid = go_to(&state, cal, aside, state.raw_time, &pad_pos);

    // mark current day
    atrs = winch(cal) & A_ATTRIBUTES;
    wchgat(cal, 2, atrs | A_UNDERLINE, 0, NULL);
    prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);

    WINDOW* prev = newwin(prev_height, prev_width, 1, ASIDE_WIDTH + CAL_WIDTH);
    display_entry(&state, &state.today, prev, prev_width);

    do {
        ch = wgetch(cal);
        // new_date represents the desired date the user wants to go_to(),
        // which may not be a feasible date at all
        new_date = state.curs_date;
        char ecmd[150];
        char* pecmd = ecmd;
        char pth[100];
        char* ppth = pth;
        char dstr[16];
        edit_cmd(&state, &new_date, &pecmd, sizeof ecmd);

        switch(ch) {
            case KEY_RESIZE:
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;

            // basic movements
            case 'j':
            case KEY_DOWN:
                new_date.tm_mday += 7;
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'k':
            case KEY_UP:
                new_date.tm_mday -= 7;
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'l':
            case KEY_RIGHT:
                new_date.tm_mday++;
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'h':
            case KEY_LEFT:
                new_date.tm_mday--;
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;

            // jump to top/bottom of page
            case 'g':
                mv_valid = go_to(&state, cal, aside, mktime(&state.cal_start), &pad_pos);
                break;
            case 'G':
                mv_valid = go_to(&state, cal, aside, mktime(&state.cal_end), &pad_pos);
                break;

            // jump backward/forward by a month
            case 'K':
                if (new_date.tm_mday == 1)
                    new_date.tm_mon--;
                new_date.tm_mday = 1;
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'J':
                new_date.tm_mon++;
                new_date.tm_mday = 1;
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;

            // search for specific date
            case 's':
                wclear(header);
                curs_set(2);
                mvwprintw(header, 0, 0, "Go to date [YYYY-MM-DD]: ");
                if (wscanw(header, "%4i-%2i-%2i", &syear, &smonth, &sday) == 3) {
                    // struct tm.tm_year: years since 1900
                    new_date.tm_year = syear - 1900;
                    // struct tm.tm_mon in range [0, 11]
                    new_date.tm_mon = smonth - 1;
                    new_date.tm_mday = sday;
                    mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                }
                curs_set(0);
                //update_date(header);
                break;
            // today shortcut
            case 't':
                new_date = state.today;
                mv_valid = go_to(&state, cal, aside, state.raw_time, &pad_pos);
                break;
            // delete entry
            case 'd':
            case 'x':
                if (date_has_entry(&state, &state.curs_date)) {
                    // get file path of entry and delete entry
                    fpath(&state, &state.curs_date, &ppth, sizeof pth);
                    if (ppth == NULL) {
                        fprintf(stderr, "Error retrieving file path for entry removal");
                        break;
                    }

                    // prepare header for confirmation dialogue
                    wclear(header);
                    curs_set(2);
                    noecho();

                    // ask for confirmation
                    get_date_str(&state.curs_date, dstr, sizeof dstr);
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
                            update_date(&state, header);
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
                if (date_has_entry(&state, &state.curs_date)) {
                    atrs = winch(cal) & A_ATTRIBUTES;
                    wchgat(cal, 2, atrs | A_BOLD, 0, NULL);

                    // refresh the calendar to add highlighting
                    prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH,
                             LINES - 1, ASIDE_WIDTH + CAL_WIDTH);
                }
                break;
            // Move to the previous diary entry
            case 'N':
                new_date = find_closest_entry(&state, new_date, true);
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;
            // Move to the next diary entry
            case 'n':
                new_date = find_closest_entry(&state, new_date, false);
                mv_valid = go_to(&state, cal, aside, mktime(&new_date), &pad_pos);
                break;
        }

        if (mv_valid) {
            update_date(&state, header);

            // adjust prev width (if terminal was resized in the mean time)
            prev_width = COLS - ASIDE_WIDTH - CAL_WIDTH;
            wresize(prev, prev_height, prev_width);

            // read the diary
            display_entry(&state, &state.curs_date, prev, prev_width);
        }
    } while (ch != 'q');

    endwin();
    system("clear");
    return 0;
}
