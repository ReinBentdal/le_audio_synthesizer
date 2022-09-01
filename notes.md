
# Notes
- Reducing frame duration below 7.5ms is not possible because the LC3 codec only supports 7.5ms and 10ms frame durations. Wondering why this is hardcoded and not arbitrary. Have used a 2ms frame duration for my personal project, which should effectively reduce the latency by the difference in frame duration.
