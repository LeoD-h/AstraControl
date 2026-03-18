CC = gcc
CFLAGS = -Wall -Wextra -O2
CPPFLAGS = -Iinclude

BUILD_DIR = build
SRC_DIR = src
TEST_DIR = tests

LIB_OBJS = $(BUILD_DIR)/session.o $(BUILD_DIR)/data.o
LIB_NAME = $(BUILD_DIR)/libinet.a

TEST_STREAM = $(BUILD_DIR)/test_stream
TEST_DGRAM = $(BUILD_DIR)/test_dgram
USELIBINET_SRV = $(BUILD_DIR)/useLibInet_srv
USELIBINET_CLT = $(BUILD_DIR)/useLibInet_clt

.PHONY: all clean tests dirs

all: dirs $(LIB_NAME) $(TEST_STREAM) $(TEST_DGRAM)

examples: dirs $(LIB_NAME) $(USELIBINET_SRV) $(USELIBINET_CLT)

dirs:
	mkdir -p $(BUILD_DIR)

$(LIB_NAME): $(LIB_OBJS)
	ar rcs $@ $^

$(BUILD_DIR)/session.o: $(SRC_DIR)/session.c include/session.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/data.o: $(SRC_DIR)/data.c include/data.h include/session.h | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_STREAM): $(TEST_DIR)/test_stream.c $(LIB_NAME)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIB_NAME)

$(TEST_DGRAM): $(TEST_DIR)/test_dgram.c $(LIB_NAME)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIB_NAME)

$(USELIBINET_SRV): $(SRC_DIR)/useLibInet.c $(LIB_NAME)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSERVEUR -o $@ $< $(LIB_NAME)

$(USELIBINET_CLT): $(SRC_DIR)/useLibInet.c $(LIB_NAME)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCLIENT -o $@ $< $(LIB_NAME)

tests: $(TEST_STREAM) $(TEST_DGRAM)
	./$(TEST_STREAM)
	./$(TEST_DGRAM)

clean:
	rm -rf $(BUILD_DIR)
