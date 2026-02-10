# e3dcset Test Suite

This directory contains unit tests for e3dcset using a minimal C test framework with no external dependencies.

## Running Tests

```bash
make test
```

This will:
1. Compile all test binaries
2. Run each test suite
3. Report results

## Test Structure

- `test_framework.h` - Minimal test framework (no external deps)
- `test_safe_string.c` - Tests for safe_string_copy() function
- `test_config.c` - Tests for configuration file parsing
- `test_validation.c` - Tests for input validation functions

## Adding New Tests

1. Create a new `test_*.c` file
2. Include `test_framework.h`
3. Define tests using the `TEST(name)` macro
4. Use assertions: `ASSERT()`, `ASSERT_EQ()`, `ASSERT_STR_EQ()`
5. Call `PASS()` at the end of each test
6. Run tests with `RUN_TEST(name)` in main()
7. End main() with `TEST_SUMMARY()`

Example:

```c
#include "test_framework.h"

TEST(my_test) {
    ASSERT(1 == 1, "Basic math should work");
    PASS();
}

int main(void) {
    printf("\n=== Testing My Feature ===\n");
    RUN_TEST(my_test);
    TEST_SUMMARY();
}
```

The Makefile will automatically discover and compile new `test_*.c` files.

## Test Philosophy

- Tests should be simple and readable
- Each test should test one thing
- Tests should be independent (no shared state)
- Test functions should be copied/simplified if needed (avoid complex mocking)
- Tests run in CI on every PR and push to master
