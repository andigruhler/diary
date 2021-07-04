<p align="center">
<img src="./img/diary-worm.png" width="250px" />
</p>

# Diary

This is a text-based diary, inspired by [khal](https://github.com/pimutils/khal). Journal entries are stored in text files.

## Demo
![Diary Demo](https://raw.githubusercontent.com/in0rdr/diary/master/img/demo.gif)

## Usage
1. Set the EDITOR environment variable to your favourite text editor:
    ```
    export EDITOR=vim
    ```

2. Run the diary, with the folder for the text files as first argument:
    ```
    diary ~/.diary
    ```

   Instead of this, you can also set the environment variable `DIARY_DIR`
   to the desired directory. If both an argument and the environment
   variable are given, the argument takes precedence (see [Variable Precedence Rules](#precedence_rules)).

   The text files in this folder will be named 'yyyy-mm-dd'.

3. Use the keypad or VIM-like shortcuts to move between dates:

    ```
    e, Enter  edit current entry
    d, x      delete current entry
    s         sync current entry with CalDAV server

    t         jump to today
    f         jump to or find specific day

    j, down   go forward by 1 week
    k, up     go backward by 1 week
    h, left   go left by 1 day
    l, right  go right by 1 day
    J         go forward by 1 month
    K         go backward by 1 month

    N         go to the previous journal entry
    n         go to the next journal entry
    g         go to start of journal
    G         go to end of journal

    q         quit the program
    ```

![diary cheet sheet](https://raw.githubusercontent.com/in0rdr/diary/master/img/diary-cheat-sheet.png)

## Install

Pre-built packages can be installed from the [Open Build Service (OBS)](https://software.opensuse.org//download.html?project=home%3Ain0rdr&package=diary)

### Arch Linux OBS Repository Bug

The Arch Linux repository on the OBS is broken, see [#69](https://github.com/in0rdr/diary/issues/69) and [#9953](https://github.com/openSUSE/open-build-service/issues/9953).

To use the repository on Arch Linux, use `downloadcontent.opensuse.org` instead of the [suggested](https://software.opensuse.org/download.html?project=home%3Ain0rdr&package=diary) name `download.opensuse.org` in the `/etc/pacman.conf`:
```
[home_in0rdr_Arch]
Server = https://downloadcontent.opensuse.org/repositories/home:/in0rdr/Arch/$arch
```

## Build
[![Build Status](https://travis-ci.org/in0rdr/diary.svg?branch=master)](https://travis-ci.org/in0rdr/diary)

1. Define [OAuth2 application credentials](https://developers.google.com/identity/protocols/oauth2) if CalDAV sync should be effective:
    ```
    export GOOGLE_OAUTH_CLIENT_ID=""
    export GOOGLE_OAUTH_CLIENT_SECRET=""
    ```

    Alternatively, leave these two variables unset and [define the clientid/secret in the configuration file](#Google-Calendar-OAuth2) at run-time.

2. Compile (requires ncurses and libcurl):
    ```
    make
    ```
Note: for *BSD users run gmake.

3. Install the binary (optional):
    ```
    sudo make install
    ```

   By default this will copy the binary to /usr/local/bin. To use a different
   path prefix, type `sudo make PREFIX=/usr install` to use /usr/bin for example.
   You can uninstall diary with `sudo make uninstall`.

## Configuration File

The [`diary.cfg`](./config/diary.cfg) configuration file can optionally be used to persist diary configuration. To install the sample from this repository:
```bash
mkdir -p ${XDG_CONFIG_HOME:-~/.config}/diary
cp config/diary.cfg ${XDG_CONFIG_HOME:-~/.config}/diary/
```

The file `${XDG_CONFIG_HOME:-~/.config}/diary/diary.cfg` should adhere to a basic `key = value` format. Lines can be commented with the special characters `#` or `;`. The following configuration keys are currently supported:

| Command Line Option | Config Key | Example Config Value | Default Config Value | Description |
| --- | --- | --- | --- | --- |
| `--dir`, `-d`, or first non-option argument | `dir` | ~/diary | n/a | Diary directory. Path that holds the journal text files. If unset, defaults to environment variable `$DIARY_DIR`.|
| `--editor` or `-e` | `editor` | vim | (empty) | Editor to open journal files with. If unset, defaults to environment variable `$EDITOR`. If no editor is provided, the diary is opened read-only. |
| `--fmt` or `-f` | `fmt` | %d_%b_%y | %Y-%m-%d | Date format and file name for the files inside the `dir`. For the format specifiers, see [`man strftime`](https://man7.org/linux/man-pages/man3/strftime.3.html). Be careful: If you change this, you might no longer find your existing diary entries, because the diary assumes to find the journal files under another file name. Hence, a change in FMT shows an empty diary, at first. Rename all files in the DIARY_DIR to migrate to a new FMT. |
| `--range` or `-r` | `range` | 10 | 1 | Number of years to show before/after todays date |
| `--weekday` or `-w` | `weekday` | 0 | 1 | First weekday, `7` = Sunday, `1` = Monday, ..., `6` = Saturday. Use `7` (or `0`) to display week beginning at Sunday ("S-M-T-W-T-F-S"), or `1` for "M-T-W-T-F-S-S". If `glibc` is installed, the first day of the week is derived from the current locale setting (`$LANG`, see `man locale`). Without `glibc`, the first weekday defaults to 1 (Monday), unless specified otherwise with this option. |
| n/a | `google_calendar` | diary | (empty) | Displayname of Google Calendar for [CalDAV sync](#CalDAV-sync) |
| n/a | `google_clientid` | 123-123.apps.googleusercontent.com | [built-in](#Build) / (empty) | Google Calendar for [Google Calendar OAuth2](#Google-Calendar-OAuth2) clientid |
| n/a | `google_secretid` | 321 | [built-in](#Build) / (empty) |  Google Calendar for [Google Calendar OAuth2](#Google-Calendar-OAuth2) secretid |
| n/a | `google_tokenfile` | ~/.diary-token | ~/.diary-token | Displayname of Google Calendar for [Google Calendar OAuth2](#Google-Calendar-OAuth2) API token file|

## Precedence Rules
<a name="precedence_rules"></a>

The default variables, for instance, for the configuration variables `editor`, `dir` and `weekday`, are populated with values in the following order, where earlier entries are overwritten by subsequent ones if they exist:
1. No default for `DIARY_DIR`. Defaults for `range`, `weekday`, `fmt` and `editor` are provided in [diary.h](diary.h)
* If `EDITOR` is unset and no editor is provided in the config file or via the `-e` option, the diary works read-only. Journal files cannot be opened.
* If `DIARY_DIR` is not provided, the diary won't open.
2. **Config file** (empty default for `editor`, no default for `dir`)
3. **Environment** variables `$DIARY_DIR`, `$EDITOR` and `$LANG` for locale (`weekday`)
4. **Option arguments** `-d` / `-e` / `-w`
5. First non-option argument is interpreted as `DIARY_DIR`

### Precedence Example: Locale and First Day of Week

If glibc is installed, the first weekday defaults to the locale defined in the current shell environment (`$LANG`, see `man locale`), unless specified otherwise via the `--weekday` / `-w` command line options. For example:

```bash
# start with weekday=3(Wed), overrule any other configuration value
$ diary -w3

# start with glibc derived weekday=1, regardless of 'weekday' in config file
$ LANG=de_CH diary

# if glibc is installed, start with glibc derived base date (weekday=0)
$ LANG= diary

# disable environment variable, default to value from config file
$ unset LANG

# start with 'weekday' default from config file, if available
$ diary

# remove config file
$ rm ${XDG_CONFIG_HOME:-~/.config}/diary/diary.cfg

# start with 'weekday' default value from source code (1=Mon)
$ diary
```

## CalDAV Sync
The journal files can be synced via CalDAV. Currently, only the Google Calendar is supported as remote provider. Please open an [issue](https://github.com/in0rdr/diary/issues) to implement support for additional remote calendar servers.


The calender for synchronization can be defined with the [configuration](#Configuration-File) key `google_calendar`:
```
# Google calendar name for CalDAV sync
google_calendar = example
```

This key is empty by default and the only configuration key required for setting up synchronization.

### Google Calendar OAuth2

The Google Calendar CalDAV API is protected with OAuth2.

The credentials and the consent screen can be redefined at [compile time](#Build) or with the following keys in the [configuration file](#Configuration-File):

```
# Google OAuth2 clientid and secretid
google_clientid = 123-123.apps.googleusercontent.com
google_secretid = 321
```

The token used to authenticate with the Google API is stored in the file specified by `google_tokenfile` (defaults to `~/.diary-token`):
```
# Google OAuth2 tokenfile
google_tokenfile = ~/.diary-token
```

The application requires two [OAuth2 scopes](https://developers.google.com/calendar/auth) for CalDAV requests:

1. `https://www.googleapis.com/auth/calendar`: read/write access to Calendars - required to discover the unique hyperlink/URI for the calendar specified by the [configuration key](#Configuration-File) `google_calendar`
2. `https://www.googleapis.com/auth/calendar.events.owned`: read/write access to Events owned by the user - allows diary to create/read/update/delete events in `google_calendar`
