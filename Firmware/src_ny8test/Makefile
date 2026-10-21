all : flash

TARGET:=main

TARGET_MCU?=CH32V003
ADDITIONAL_C_FILES+=../src_pgmtest/power.c ny8_prog.c
EXTRA_CFLAGS+=-I../src_pgmtest
include ../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
