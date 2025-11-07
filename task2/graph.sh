#!/bin/bash
mpicc -o task2 task2.c -lm

echo "p,time_rows,time_cols,time_blocks" > raw_times.csv
for p in 1 4 9 16; do
    mpiexec -n $p ./task2
done

base_times=$(head -2 raw_times.csv | tail -1)
base_rows=$(echo $base_times | cut -d',' -f2)
base_cols=$(echo $base_times | cut -d',' -f3) 
base_blocks=$(echo $base_times | cut -d',' -f4)

echo "p,time_rows,speedup_rows,eff_rows,time_cols,speedup_cols,eff_cols,time_blocks,speedup_blocks,eff_blocks" > results.csv

while IFS=',' read p t_rows t_cols t_blocks; do
    if [ "$p" != "p" ]; then
        speedup_rows=$(echo "scale=4; $base_rows / $t_rows" | bc)
        eff_rows=$(echo "scale=2; $speedup_rows / $p * 100" | bc)
        
        speedup_cols=$(echo "scale=4; $base_cols / $t_cols" | bc) 
        eff_cols=$(echo "scale=2; $speedup_cols / $p * 100" | bc)
        
        speedup_blocks=$(echo "scale=4; $base_blocks / $t_blocks" | bc)
        eff_blocks=$(echo "scale=2; $speedup_blocks / $p * 100" | bc)
        
        echo "$p,$t_rows,$speedup_rows,$eff_rows,$t_cols,$speedup_cols,$eff_cols,$t_blocks,$speedup_blocks,$eff_blocks" >> results.csv
    fi
done < raw_times.csv