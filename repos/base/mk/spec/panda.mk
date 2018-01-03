#
# Enable peripherals of the platform
#
SPECS += omap4 usb cortex_a9 panda gpio framebuffer

#
# Pull in CPU specifics
#
SPECS += arm_v7a

#
# Add device parameters to include search path
#
REP_INC_DIR += include/spec/panda

include $(BASE_DIR)/mk/spec/cortex_a9.mk
include $(BASE_DIR)/mk/spec/arm_v7a.mk
