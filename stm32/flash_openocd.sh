#!/bin/bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "program build/PCAN.elf verify reset exit"
