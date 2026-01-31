all : flash

TARGET:=main

TARGET_MCU?=CH32V003
ADDITIONAL_C_FILES+=power.c pdk_prog.c pdk_devices.c
include ../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
