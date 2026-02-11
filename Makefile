CXX=g++
CXXFLAGS=-O3 -MMD -MP
CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c99
ROOT_VALUE=e3dcset

# Directory structure
SRCDIR=src
INCDIR=include

# Add include directory to compiler flags
CXXFLAGS+=-I$(INCDIR)

# Source files
SOURCES=$(SRCDIR)/e3dcset.cpp $(SRCDIR)/config.cpp $(SRCDIR)/rscp_handler.cpp \
        $(SRCDIR)/output.cpp $(SRCDIR)/history.cpp $(SRCDIR)/RscpProtocol.cpp \
        $(SRCDIR)/AES.cpp $(SRCDIR)/SocketConnection.cpp

# Object files and dependency files
OBJECTS=$(SOURCES:.cpp=.o)
DEPS=$(SOURCES:.cpp=.d)

# C test files
TEST_DIR=tests
TEST_SOURCES=$(wildcard $(TEST_DIR)/test_*.c)
TEST_BINARIES=$(TEST_SOURCES:.c=)

# C++ test files (Catch2)
CXX_TEST_SOURCES=$(TEST_DIR)/test_main.cpp $(TEST_DIR)/test_constants.cpp $(TEST_DIR)/test_helpers.cpp
CXX_TEST_BINARY=$(TEST_DIR)/run_tests

all: $(ROOT_VALUE)

# Link object files
$(ROOT_VALUE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

# Compile source files to object files
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include generated dependency files
-include $(DEPS)

# Build and run all tests
test: $(TEST_BINARIES) $(CXX_TEST_BINARY)
	@echo "========================================="
	@echo "Running e3dcset Test Suite"
	@echo "========================================="
	@for test in $(TEST_BINARIES); do \
		$$test || exit 1; \
	done
	@echo "--- C++ Unit Tests (Catch2) ---"
	./$(CXX_TEST_BINARY)
	@echo "--- CLI Validation Tests ---"
	@cd $(TEST_DIR) && ./test_cli_validation.sh
	@echo "========================================="
	@echo "All tests passed!"
	@echo "========================================="

# Build individual C test binaries
$(TEST_DIR)/test_%: $(TEST_DIR)/test_%.c $(TEST_DIR)/test_framework.h
	$(CC) $(CFLAGS) -I$(TEST_DIR) $< -o $@

# Build C++ test binary (Catch2)
$(CXX_TEST_BINARY): $(CXX_TEST_SOURCES)
	$(CXX) -Wall -std=c++11 -I$(TEST_DIR) -I$(INCDIR) -o $@ $(CXX_TEST_SOURCES)

clean:
	-rm -f $(ROOT_VALUE) $(VECTOR) $(TEST_BINARIES) $(CXX_TEST_BINARY) $(OBJECTS) $(DEPS)

.PHONY: all test clean
