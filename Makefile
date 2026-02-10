CXX=g++
CXXFLAGS=-O3
CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c99
ROOT_VALUE=e3dcset

# Source files
SOURCES=e3dcset.cpp config.cpp rscp_handler.cpp output.cpp history.cpp \
        RscpProtocol.cpp AES.cpp SocketConnection.cpp

# Test files
TEST_DIR=tests
TEST_SOURCES=$(wildcard $(TEST_DIR)/test_*.c)
TEST_BINARIES=$(TEST_SOURCES:.c=)

all: $(ROOT_VALUE)

$(ROOT_VALUE): clean
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

# Build and run all tests
test: $(TEST_BINARIES)
	@echo "========================================="
	@echo "Running e3dcset Test Suite"
	@echo "========================================="
	@for test in $(TEST_BINARIES); do \
		$$test || exit 1; \
	done
	@echo "========================================="
	@echo "All tests passed!"
	@echo "========================================="

# Build individual test binaries
$(TEST_DIR)/test_%: $(TEST_DIR)/test_%.c $(TEST_DIR)/test_framework.h
	$(CC) $(CFLAGS) -I$(TEST_DIR) $< -o $@

clean:
	-rm -f $(ROOT_VALUE) $(VECTOR) $(TEST_BINARIES)
