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

# Test files
TEST_DIR=tests
TEST_SOURCES=$(wildcard $(TEST_DIR)/test_*.c)
TEST_BINARIES=$(TEST_SOURCES:.c=)

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
