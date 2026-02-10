CXX=g++
CXXFLAGS=-O3 -MMD -MP
CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c99
ROOT_VALUE=e3dcset

# Source files
SOURCES=e3dcset.cpp config.cpp rscp_handler.cpp output.cpp history.cpp \
        RscpProtocol.cpp AES.cpp SocketConnection.cpp

# Object files and dependency files
OBJECTS=$(SOURCES:.cpp=.o)
DEPS=$(SOURCES:.cpp=.d)

# Test files
TEST_DIR=tests
TEST_SOURCES=$(wildcard $(TEST_DIR)/test_*.c)
TEST_BINARIES=$(TEST_SOURCES:.c=)

all: $(ROOT_VALUE)

# Link object files
$(ROOT_VALUE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include generated dependency files
-include $(DEPS)

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
	-rm -f $(ROOT_VALUE) $(VECTOR) $(TEST_BINARIES) $(OBJECTS) $(DEPS)
