# Simulated time next to the step number

By default the scene shows only the step number (`Step 3500`). When the model also writes the simulated
time of its steps, the Viewer shows it under the step number:

```
Step 3500
Time: 4 h 52 min 5.0 s
```

## The time file

The model writes an optional text file `<output_file_name>-time.txt` into the same directory as the other
output files (next to `<output_file_name>-red.txt`), e.g. `OutputSciddicaT/sciddicaTout-time.txt`. One line
per saved step:

```
# step  simulated time [s]
0       0
500     2500
1000    5000.5
```

- 1st column: step number (the same numbers as in `<output_file_name><step>.bin` / `*_index.txt`),
- 2nd column: simulated time elapsed at that step, **in seconds** (fractions allowed; `.` and `,` are both
  accepted as the decimal separator),
- columns are separated by whitespace, further columns are ignored,
- empty lines and `#` comments are ignored.

The step does not have to be a multiple of anything and `delta_t` may change from step to step - the file
simply lists the time of every saved step, nothing is computed or interpolated by the Viewer.

Behaviour:

| Situation | What is shown |
|---|---|
| no time file | `Step N` only (as before) |
| time file, the step is listed | `Step N` and `Time: ...` |
| time file, the step is not listed | `Step N` and `Time: n/a` |
| time file that can not be parsed | a warning with the line number, `Step N` only |

## Pretty time format

**Settings → Pretty Time Format** (on by default, remembered between runs) chooses the way the time is written:

| Value | Pretty format on | Pretty format off |
|---|---|---|
| 5.5 s | `5.5 s` | `5.5 s` |
| 65.5 s | `1 min 5.5 s` | `65.5 s` |
| 3725.5 s | `1 h 2 min 5.5 s` | `3725.5 s` |
| 90061 s | `1 d 1 h 1 min 1 s` | `90061 s` |

Seconds roll over to minutes, minutes to hours and hours to days. Tenths of a second are shown until the first
day is reached, from then on whole seconds are shown. The leading zero units are omitted, the following ones
are always shown so the text does not change its shape while the simulation is played.

## For developers

- `data/StepTimeMap.h/.cpp` - reading of the file (Qt-free),
- `widgets/TimeFormatting.h` - the formatting (Qt-free, locale independent),
- `SceneWidget::setPrettyTimeFormat()` / `timeLineForCurrentStep()` - glue with the scene,
- tests: `tests/StepTimeMapTests.cpp`, `tests/TimeFormattingTests.cpp`.
