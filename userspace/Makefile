CC = gcc
CFLAGS = -Wall -Wextra -O2

# Add standard system include paths explicitly
CFLAGS += -I/usr/include
CFLAGS += -I/usr/include/drm

# SDL2 configuration
SDL2_CFLAGS = $(shell pkg-config --cflags sdl2)
SDL2_LIBS = $(shell pkg-config --libs sdl2)

CFLAGS += $(SDL2_CFLAGS)

# For cross-compilation or specific architectures
ifeq ($(shell uname -m),aarch64)
    CFLAGS += -I/usr/include/aarch64-linux-gnu
endif

# Ensure we link against standard libraries and SDL2
LDFLAGS = $(SDL2_LIBS) -lm -ldrm

TARGET = execute_gpu

# Default rule
all: $(TARGET) compile_commands.json

# Compile the target
$(TARGET): execute_gpu.c
	$(CC) $(CFLAGS) -o $(TARGET) execute_gpu.c $(LDFLAGS)

# Generate compile_commands.json
compile_commands.json: execute_gpu.c
	@echo '[' > compile_commands.json
	@echo '  {' >> compile_commands.json
	@echo '    "directory": "$(shell pwd)",' >> compile_commands.json
	@echo '    "command": "$(CC) $(CFLAGS) -c execute_gpu.c",' >> compile_commands.json
	@echo '    "file": "execute_gpu.c"' >> compile_commands.json
	@echo '  }' >> compile_commands.json
	@echo ']' >> compile_commands.json

# Clean up generated files
clean:
	rm -f $(TARGET) compile_commands.json

# Run the compiled program
run: $(TARGET)
	./$(TARGET)

# Generate compile_commands.json only
compdb: compile_commands.json

# Show SDL2 configuration (helpful for debugging)
sdl-info:
	@echo "SDL2 CFLAGS: $(SDL2_CFLAGS)"
	@echo "SDL2 LIBS: $(SDL2_LIBS)"

.PHONY: all clean run compdb sdl-info
