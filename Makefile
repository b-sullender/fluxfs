CC = gcc
CFLAGS = -Wall -Wextra -fPIC
LDFLAGS = -L$(BUILD_DIR) -lfluxfs $(shell pkg-config fuse --cflags --libs)
AR = ar
ARFLAGS = rcs

LIB_NAME = libfluxfs
LIB_DIR = source/lib
TEST_DIR = source/testing
APP_DIR = source/fluxfs
BUILD_DIR = build

LIB_SRC = $(wildcard $(LIB_DIR)/*.c)
LIB_OBJ = $(LIB_SRC:$(LIB_DIR)/%.c=$(BUILD_DIR)/libfluxfs_%.o)

LIB_A = $(BUILD_DIR)/$(LIB_NAME).a
LIB_SO = $(BUILD_DIR)/$(LIB_NAME).so

TEST_SRC = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJ = $(TEST_SRC:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%.o)
TEST_BIN = $(BUILD_DIR)/test

APP_SRC = $(wildcard $(APP_DIR)/*.c)
APP_OBJ = $(APP_SRC:$(APP_DIR)/%.c=$(BUILD_DIR)/fluxfs_%.o)
APP_BIN = $(BUILD_DIR)/fluxfs

# Create build directory if it does not exist
$(shell mkdir -p $(BUILD_DIR))

all: static shared test app

# Compile static library
static: $(LIB_A)

$(LIB_A): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

# Compile shared library
shared: $(LIB_SO)

$(LIB_SO): $(LIB_OBJ)
	$(CC) -shared -o $@ $^

# Compile object files for library (renamed)
$(BUILD_DIR)/libfluxfs_%.o: $(LIB_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile object files for test program (renamed)
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile object files for application (renamed)
$(BUILD_DIR)/fluxfs_%.o: $(APP_DIR)/%.c
	$(CC) $(CFLAGS) $(shell pkg-config fuse --cflags) -c $< -o $@

# Compile test program linking with static library
test: $(TEST_BIN)

$(TEST_BIN): $(TEST_OBJ) $(LIB_A)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile application linking with libfluxfs and FUSE
app: $(APP_BIN)

$(APP_BIN): $(APP_OBJ) $(LIB_A)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*.a $(BUILD_DIR)/*.so $(TEST_BIN) $(APP_BIN)
