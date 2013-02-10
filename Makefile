# This is the Makefile for SLLogiclogger
# Copyright (C) 2012 Thomas Fischl <tfischl@gmx.de> http://www.fischl.de
#
# Used Tools:
# * GCC Toolchain gcc-arm-none-eabi-4_6-2012q4-20121016.tar.bz2
# ** https://launchpad.net/gcc-arm-embedded/+download
# * StellarisWare v9453
# ** http://www.ti.com/tool/sw-ek-lm4f120xl
# * lm4flash 12-12-20
# ** https://github.com/utzig/lm4tools.git

PART=LM4F120H5QR
VARIANT=cm4f
STELLARISWARE=/opt/stellaris/stellarisware
FLASHER=/opt/stellaris/bin/lm4flash

include ${STELLARISWARE}/makedefs

VPATH=${STELLARISWARE}/utils
IPATH=${STELLARISWARE}

all: ${COMPILER}
all: ${COMPILER}/sllogiclogger.axf

clean:
	@rm -rf ${COMPILER} ${wildcard *~} sllogiclogger.ld

flash:
	${FLASHER} ${COMPILER}/sllogiclogger.bin

startup_gcc.c:
	@cp ${STELLARISWARE}/boards/ek-lm4f120xl/project0/startup_gcc.c .
sllogiclogger.ld:
	@cp ${STELLARISWARE}/boards/ek-lm4f120xl/project0/project0.ld sllogiclogger.ld

${COMPILER}:
	@mkdir -p ${COMPILER}	

${COMPILER}/sllogiclogger.axf: ${COMPILER}/sllogiclogger.o
${COMPILER}/sllogiclogger.axf: ${COMPILER}/startup_${COMPILER}.o
${COMPILER}/sllogiclogger.axf: ${COMPILER}/uartstdio.o
${COMPILER}/sllogiclogger.axf: ${STELLARISWARE}/driverlib/${COMPILER}-cm4f/libdriver-cm4f.a
${COMPILER}/sllogiclogger.axf: sllogiclogger.ld
SCATTERgcc_sllogiclogger=sllogiclogger.ld
ENTRY_sllogiclogger=ResetISR
CFLAGSgcc=-DTARGET_IS_BLIZZARD_RA1

ifneq (${MAKECMDGOALS},clean)
-include ${wildcard ${COMPILER}/*.d} __dummy__
endif
