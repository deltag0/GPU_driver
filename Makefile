# Kernel module Makefile

# Target modules
obj-m += gpu_driver.o
gpu_driver-objs := driver.o execbuffer.o

# Detect the current kernel version
KERNEL_VERSION ?= $(shell uname -r)
KERNEL_DIR ?= /lib/modules/$(KERNEL_VERSION)/build

# Default compiler (can be overridden)
CC ?= gcc

# Additional compilation flags
EXTRA_CFLAGS += -Wall -Wextra -Wno-unused-parameter
EXTRA_CFLAGS += -D__FAKE_KERNEL__ -DTEST_GPU

# Verbose output control (use make V=1 for verbose compilation)
ifeq ($(V),1)
	Q =
else
	Q = @
endif

# Main build target
all: modules

modules:
	$(Q)make -C $(KERNEL_DIR) M=$(PWD) modules CC=$(CC) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" C=1

# Clean target
clean:
	$(Q)make -C $(KERNEL_DIR) M=$(PWD) clean

# Help target to show available options
help:
	@echo "Available targets:"
	@echo "  all      - Build kernel modules (default)"
	@echo "  clean    - Remove built modules and temporary files"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Useful variables:"
	@echo "  KERNEL_VERSION  - Specify kernel version (current: $(KERNEL_VERSION))"
	@echo "  CC             - Compiler to use (current: $(CC))"
	@echo ""
	@echo "Examples:"
	@echo "  make             - Build modules for current kernel"
	@echo "  make V=1         - Verbose compilation"
	@echo "  make CC=clang    - Use Clang instead of GCC"

# Phony targets
.PHONY: all modules clean help
