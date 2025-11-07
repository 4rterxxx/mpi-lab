#!/bin/bash

PROGRAM="task1"
RESULTS_FILE="task1_results.csv"

mpicc task1.c -o $PROGRAM -lm

echo "Processes,Time,Speedup,Efficiency" > $RESULTS_FILE

echo "=== Запуск на 1 процессе ==="
time1=$(mpiexec -n 1 ./$PROGRAM)
echo "Время: $time1 секунд"
echo "1,$time1,1.00,100.00" >> $RESULTS_FILE
echo ""

for n in {2..16}; do
    echo "=== Запуск на $n процессах ==="
    time_n=$(mpiexec -n $n ./$PROGRAM)
    
    if [ ! -z "$time_n" ] && [ "$time_n" != "0.000000" ]; then
        speedup=$(echo "scale=2; $time1 / $time_n" | bc -l)
        efficiency=$(echo "scale=2; $speedup / $n * 100" | bc -l)
        echo "$n,$time_n,$speedup,$efficiency" >> $RESULTS_FILE
        echo "Время: $time_n секунд"
        echo "Ускорение: $speedup"
        echo "Эффективность: $efficiency%"
    fi
    echo ""
done
