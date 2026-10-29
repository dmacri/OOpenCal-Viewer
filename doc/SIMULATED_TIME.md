# Simulated time next to the step number

By default the scene shows only the step number (`Step 3500`). When the model also writes the simulated
time of its steps, the Viewer shows it under the step number:

```
Step 3500
Time: 4 h 52 min 5.0 s
```

The time line is a separate text under the step number, in the same color, written with a slightly smaller
font (about 80% of the step font).

## The time file

The model writes an optional text file `<output_file_name>-time.txt` into the same directory as the other
output files (next to `<output_file_name>-red.txt`), e.g. `OutputSciddicaT/sciddicaTout-time.txt`. The first line
is the unit of the times in square brackets, then one line per saved step:

```
[ms]
# step  time
0       0
500     2500
1000    5000.5
```

- 1st line: unit of the times, `[s]` (seconds) or `[ms]` (milliseconds), case does not matter. It comes before the
  first step; comments and empty lines may precede it. **When the line is missing the times are seconds.**
  Any other unit is reported as an error,
- 1st column: step number (the same numbers as in `<output_file_name><step>.bin` / `*_index.txt`),
- 2nd column: simulated time elapsed at that step, in the unit from the first line (fractions allowed; `.` and `,`
  are both accepted as the decimal separator),
- columns are separated by whitespace, further columns are ignored,
- empty lines and `#` comments are ignored.

The step does not have to be a multiple of anything and `delta_t` may change from step to step - the file
simply lists the time of every saved step, nothing is computed or interpolated by the Viewer.

Behaviour:

| Situation | What is shown |
|---|---|
| no time file | `Step N` only (as before) |
| time file, the step is listed | `Step N` and `Time: ...` |
| time file, the step is not listed | `Step N` and `Time: n/a` (nothing fails, it is a normal situation) |
| time file that can not be parsed (also an unknown unit) | a warning with the line number, `Step N` only |

## Pretty time format

**Settings → Pretty Time Format** (on by default, remembered between runs) chooses the way the time is written.
With the format off the value is shown as it is in the file, together with its unit. With the format on the time
is converted to days, hours, minutes and seconds:

| In the file | Pretty format on | Pretty format off |
|---|---|---|
| `[s]` 5.5 | `5.5 s` | `5.5 s` |
| `[s]` 65.5 | `1 min 5.5 s` | `65.5 s` |
| `[s]` 3725.5 | `1 h 2 min 5.5 s` | `3725.5 s` |
| `[s]` 90061 | `1 d 1 h 1 min 1 s` | `90061 s` |
| `[ms]` 2500 | `2.500 s` | `2500 ms` |
| `[ms]` 65500 | `1 min 5.500 s` | `65500 ms` |
| `[ms]` 3725500 | `1 h 2 min 5.500 s` | `3725500 ms` |
| `[ms]` 90061400 | `1 d 1 h 1 min 1 s` | `90061400 ms` |

Seconds roll over to minutes, minutes to hours and hours to days. The fraction of a second keeps the resolution of
the unit - tenths for `[s]`, thousandths for `[ms]` - until the first day is reached, from then on whole seconds are
shown. The leading zero units are omitted, the following ones are always shown so the text does not change its
shape while the simulation is played.

## For developers

- `data/StepTimeMap.h/.cpp` - reading of the file (Qt-free),
- `core/TimeUnit.h` - the units understood in the first line of the file (to add one, extend the three functions there),
- `widgets/TimeFormatting.h` - the formatting (Qt-free, locale independent),
- `SceneWidget::setPrettyTimeFormat()` / `timeLineForCurrentStep()` / `updateTimeText()` and `Visualizer::buildTimeText()` - glue with the scene,
- tests: `tests/StepTimeMapTests.cpp`, `tests/TimeFormattingTests.cpp`.
