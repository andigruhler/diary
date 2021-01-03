# CLI Diary

This is a text based diary, inspired by [khal](https://github.com/pimutils/khal). Diary entries are stored in raw text. You may say C & ncurses are old, I say paper is older..

![Diary Demo](https://raw.githubusercontent.com/in0rdr/diary/master/demo.gif)

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
   variable are given, the argument takes precedence.

   The text files in this folder will be named 'yyyy-mm-dd'.

3. Use the keypad or VIM-like shortcuts to move between dates:

    ```
    e, Enter  Edit the current entry
    d, x      Delete/remove current entry
    t         Jump to today
    s         Jump to specific day

    j, down   go forward by 1 week
    k, up     go backward by 1 week
    h, left   go left by 1 day
    l, right  go right by 1 day

    N         go to the previous diary entry
    n         go to the next diary entry

    g         go to the first journal entry
    G         go to the last journal entry

    J         Go forward by 1 month
    K         Go backward by 1 month

    q         quit the program
    ```

![diary cheet sheet](https://raw.githubusercontent.com/in0rdr/diary/master/diary-cheat-sheet.png)

## Install

Pre-built packages can be installed from the [Open Build Service (OBS)](https://software.opensuse.org//download.html?project=home%3Ain0rdr&package=diary)

## Build
[![Build Status](https://travis-ci.org/in0rdr/diary.svg?branch=master)](https://travis-ci.org/in0rdr/diary)


1. Compile (requires ncurses):
    ```
    make
    ```
Note: for *BSD users run gmake.

2. Install the binary (optional):
    ```
    sudo make install
    ```

   By default this will copy the binary to /usr/local/bin. To use a different
   path prefix, type `sudo make PREFIX=/usr install` to use /usr/bin for example.
   You can uninstall diary with `sudo make uninstall`.

## Configuration File

(version `master`)

The [`diary.cfg`](./diary.cfg) configuration file can optionally be used to persist diary configuration. To install the sample from this repository:
```bash
mkdir -p ~/.config/diary/
cp diary.cfg ~/.config/diary/
```

The file `~/.config/diary/diary.cfg` should adhere to a basic `key = value` format. Lines can be commented with the special characters `#` or `;`. The following configuration keys are currently supported:

| Command Line Option | Config Key | Example Config Value | Default Config Value | Description |
| --- | --- | --- | --- | --- |
| `--dir`, `-d`, or first non-option argument | `diary_dir` | ~/diary | n/a | Diary directory. Path that holds the journal text files. |
| n/a | `year_range` | 10 | 1 | Number of years to show before/after todays date |
| n/a | `first_weekday` | 0 | 1 | First weekday, `0` = Sunday, `1` = Monday, ..., `6` = Saturday. Use `0` to display week beginning at Sunday ("S-M-T-W-T-F-S"), or `1` for "M-T-W-T-F-S-S" (default) |
| n/a | `date_fmt` | %d_%b_%y | %Y-%m-%d | Date format and file name for the files inside the `diary_dir`. For the format specifiers, see [`man strftime`](https://man7.org/linux/man-pages/man3/strftime.3.html). |