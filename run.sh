#!/bin/bash

arduino-cli compile \
	. \
	--fqbn STMicroelectronics:stm32:Nucleo_64 \
	-u \
	-p /dev/ttyACM0 \
	--board-options "pnum=NUCLEO_F072RB,upload_method=swdMethod"
