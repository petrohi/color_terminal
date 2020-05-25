target extended-remote :3333

# print demangled symbols
# set print asm-demangle on

# set backtrace limit to not have infinite backtrace loops
set backtrace limit 32

# *try* to stop at the user entry point (it might be gone due to inlining)
break main

# send captured ITM to the file itm.fifo
# (the microcontroller SWO pin must be connected to the programmer SWO pin)
monitor tpiu config internal itm.txt uart off 72000000

# enable ITM port 0
monitor itm port 0 on

load

# start the process but immediately halt the processor
stepi
