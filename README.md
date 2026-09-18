# xbps-tui

A textual front-end (TUI) for the **XBPS package manager** of Void Linux.
Built with **ncurses** and a small set of C data structures (hash tables for
the package caches, dynamic arrays for the package lists). It shells out to
`xbps-query(1)` and `xbps-install(1)`; those do all the real work.

## Requirements

- Void Linux with the `xbps` package installed (`xbps-query` and
  `xbps-install` must be on `PATH`)
- gcc and ncurses development headers

## Building

```sh
make          # builds ./xbps-tui (with -Wall -Wextra)
make clean
```

The program refuses to start if `xbps-query` cannot be run.

## Usage

```sh
./xbps-tui
```

`xbps-tui` shows one of six kinds of package lists. Switch between them with
**^L** (Control-L):

| Kind | Lists | Actions |
|------|-------|---------|
| Installed | all installed packages | ^R remove, ^K kill |
| Installed w/o libs | installed, minus `lib*` (default) | ^R remove, ^K kill |
| Installed orphans | installed but not required by anything | ^R remove, ^K kill |
| Installable | everything in the repositories | Tab install |
| Installable w/o libs | repository packages, minus `lib*` | Tab install |
| Upgradeable | packages with newer versions available | ^U upgrade |

## Keys

| Key | Action |
|-----|--------|
| cursor Up/Down (or `j`/`k`), PgUp/PgDn, Home/End | move in list and menus (`j`/`k` work there too) |
| **^Q** | quit |
| **^H** (or Backspace) | help |
| **^L** | choose list kind |
| **^N** | filter by name (case insensitive, live) |
| **^D** | filter by description (case sensitive, live) |
| **^P** | show properties of the selected package (description, files, size, deps) |
| **^R** | remove the selected package |
| **^K** | kill the selected package (`xbps-remove -R`, removes dependencies too) |
| **^U** | upgrade the selected package (upgradeable list) |
| Tab | install the selected package (installable lists) |
| **^F** | other functions |
| Esc | cancel a menu / clear a filter |

The **^F** menu provides:

- **Synchronize** (`xbps-install -S`) — refresh the repository databases; use it once a session
- **Show size** — compute installed size for the visible packages

Removing, killing, upgrading, installing, and synchronizing run the real
`xbps` commands via `sudo` with full control of the terminal, so the normal
`sudo` password prompt (and any `xbps` prompts) are shown in the terminal —
no password input boxes are built into the TUI. After the command finishes,
the "press a key to continue" screen shows the final line of its output
(e.g. `3 downloaded, 1 installed, 0 updated, 1 configured, 0 removed, 0 on hold.`).

## Development

The code is split into small modules under `src/`:

```
hashmap   string-key hash map used for several caches
pkglist   dynamic array of packages
util      screen/string helpers
proc      running external commands and capturing pipes
xbps      all xbps data loading (package lists, sizes, properties)
ui        ncurses drawing and input (redowin, inkey, ...)
menu      pop-up menus and message boxes
main      the event loop
```

`test/smoke.py` drives the TUI through a pty with *fake*
`xbps-query`/`xbps-install` scripts (in `test/bin/`) and dumps the raw
terminal output, so the program can be exercised and diffed without a Void
install:

```sh
python3 test/smoke.py ./xbps-tui
```

## License

MIT — see [LICENSE](LICENSE).