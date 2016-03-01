# Project 1 -  Multithreaded Process Scheduling

## Submission Tags:

1. Project 1 Milestone 1: `P1M1`
1. Project 1 Milestone 2: `P1M2`
1. Project 1 Final: `P1M3`

## Scheduling Times

* 1FCFS
  * ./analysis PCBs.bin FCFS  0.02s user 0.01s system 0% cpu 3:47.21 total
* 1RR
  * ./analysis PCBs.bin RR  0.01s user 0.01s system 0% cpu 3:47.22 total
* 1FCFS 1RR
  * ./analysis PCBs.bin FCFS RR  0.01s user 0.00s system 0% cpu 1:55.09 total
* 2FCFS 2RR
  * ./analysis PCBs.bin FCFS RR FCFS RR  0.01s user 0.00s system 0% cpu 1:00.06 total
* 3FCFS 2RR
  * ./analysis PCBs.bin FCFS FCFS RR FCFS RR  0.01s user 0.00s system 0% cpu 49.056 total
* 2FCFS 4RR
  * ./analysis PCBs.bin FCFS RR FCFS RR RR RR  0.00s user 0.00s system 0% cpu 40.035 total