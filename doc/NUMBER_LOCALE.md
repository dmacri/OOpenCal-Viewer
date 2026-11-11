# Number locale

Models run inside the Viewer as plugins, so the way they read and write numbers with the C library
(`printf`, `scanf`, `strtod`, `std::stod`, `std::to_string`...) follows the number locale (`LC_NUMERIC`) of the Viewer process.
The same holds for the values which the Viewer itself reads with `std::stod`, e.g. the substate lines of `Header.txt`.

This matters when the results of a simulation were written on one computer (e.g. a supercomputer) and are opened on another one:
a file with `1008.5` read in a locale with a decimal comma gives `1008`, and a file with `1008,5` read with a decimal point gives `1008` too.

## Choosing the locale

**Settings → Number Locale...** opens a list of the locales which are installed on the computer, with the decimal separator of each.
The preview below the list shows, for the selected locale, how the number `1008.5` is written and how the texts `1008.5` and
`1008,5` are read. Choose the locale in which the simulation output was written:

| The files contain | Choose |
|---|---|
| `1008.5` | **C** (the default) |
| `1008,5` | a locale with a decimal comma, e.g. `it_IT.utf8` or `pl_PL.utf8` |

The choice is remembered between runs (QSettings key `locale/numeric`; `C` when nothing was chosen) and applied at start,
before any plugin is loaded. When the stored locale is not installed any more, the Viewer prints a warning and uses `C`.

The locale is used whenever data is read. After a change, with a model open, the Viewer offers to reload the data.
The values from `Header.txt` are read when the model directory is opened, so open it again to read them in the new locale.

A locale which is not in the list is not installed. On Ubuntu / Debian:

```bash
sudo locale-gen it_IT.UTF-8 && sudo update-locale
```

## What is and is not changed

Only `LC_NUMERIC` of the process is set. The global `std::locale` of C++ streams is not touched (the streams of the Viewer would
start to write decimal commas and thousands separators), and neither is the `QLocale` used by the Qt widgets.
