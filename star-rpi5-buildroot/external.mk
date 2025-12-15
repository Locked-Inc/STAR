# STAR Robot OS - Buildroot External Makefile
# No custom packages defined
include $(sort $(wildcard $(BR2_EXTERNAL_STAR_ROBOT_PATH)/package/*/*.mk))
