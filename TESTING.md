# Testing

This file holds notes for testing purposes.

## Render Documentation / Man Page

### Plain Text
Generate plain text from "runoff" text:
```bash
tbl man/diary.1 | nroff -man | less
```

### `mandoc` HTML (preferred)
Install [`mandoc`](https://en.wikipedia.org/wiki/Mandoc):
```bash
sudo apt-get install mandoc
```

Generate HTML file:
```bash
mandoc -Thtml man/diary.1 > man/diary.1.html
```

### `groff` HTML
Install [`groff`](https://www.gnu.org/software/groff):
```bash
sudo apt-get install groff
```

Generate HTML file (`-t` for `tbl`):
```bash
groff -t -mandoc -Thtml man/diary.1 > man/diary.1.html
```
