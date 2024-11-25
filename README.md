# TAPDance

## Overview

TAPDance is a basic JTAG Instruction Register (IR) brute forcer for identifying undocumented JTAG Data Registers (DRs) based loosely on enumeration techniques described in [Blackbox JTAG Reverse Engineering by Felix Domke](https://fahrplan.events.ccc.de/congress/2009/Fahrplan/attachments/1435_JTAG.pdf).

This was developed to run on a NUCLEO-F072RB board using the [Arduino STM32 Core](https://github.com/stm32duino/Arduino_Core_STM32) and a Renesas SH72531 TAP controller as the target. However, it can be easily adapted to run on an Arduino Uno or similar at a lower speed.

Because this program is a single-threaded software implementation of JTAG the TCK clock is extremely irregular. A target that requires a consistent JTAG TCK clock simply will not work.

## Running Faster

When running on an F072RB at 48MHz, the program can only test about 1,000 registers per second. This may sound fast, but brute-forcing an IR that is 32-bits will take about 50 days at this rate.

Replacing the ``delayMicroseconds(TCK_ALTERNATION_DELAY)`` calls in the ``tckPulse()`` function with five or six ``__asm__ __volatile__ ("nop\n\t")`` instructions will substantially increase the TCK speed at the risk of less stability. Additionally, setting Tools -> Optimize to "Fastest (-O3) with LTO" in the Arduino IDE will increase how fast the code between calls to ``tckPulse()`` will execute.

Both of these optimizations can bring the speed up to around 9,000 attempts per second.
