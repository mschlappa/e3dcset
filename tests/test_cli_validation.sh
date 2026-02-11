#!/bin/bash
# CLI argument validation tests
# Tests error handling and validation logic without requiring E3DC connection

E3DCSET="../e3dcset"
TAGS_FILE="../e3dcset.tags"
PASSED=0
FAILED=0

# Helper function to test for expected failure
test_should_fail() {
    local description="$1"
    shift
    local args="$@"
    
    # Run command, expect non-zero exit
    $E3DCSET $args >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "✓ $description"
        ((PASSED++))
    else
        echo "✗ $description (expected failure, got success)"
        ((FAILED++))
    fi
}

# Helper function to test specific error message
test_error_message() {
    local description="$1"
    local expected_msg="$2"
    shift 2
    local args="$@"
    
    # Run command and capture stderr
    local output=$($E3DCSET $args 2>&1)
    if echo "$output" | grep -q "$expected_msg"; then
        echo "✓ $description"
        ((PASSED++))
    else
        echo "✗ $description"
        echo "  Expected: $expected_msg"
        echo "  Got: $output"
        ((FAILED++))
    fi
}

echo "=== Argument Validation Tests ==="
echo

# Test: -r cannot be combined with -c, -d, -e, -E, -a
test_error_message "Reject -r with -c" "kann nicht zusammen" -t $TAGS_FILE -r EMS_BAT_SOC -c 2000
test_error_message "Reject -r with -d" "kann nicht zusammen" -t $TAGS_FILE -r EMS_BAT_SOC -d 500
test_error_message "Reject -r with -e" "kann nicht zusammen" -t $TAGS_FILE -r EMS_BAT_SOC -e 5000
test_error_message "Reject -r with -E" "kann nicht zusammen" -t $TAGS_FILE -r EMS_BAT_SOC -E 3000
test_error_message "Reject -r with -a" "kann nicht zusammen" -t $TAGS_FILE -r EMS_BAT_SOC -a

# Test: -H cannot be combined with other commands
test_error_message "Reject -H with -r" "kann nicht zusammen" -t $TAGS_FILE -H day -r EMS_BAT_SOC
test_error_message "Reject -H with -c" "kann nicht zusammen" -t $TAGS_FILE -H day -c 2000
test_error_message "Reject -H with -e" "kann nicht zusammen" -t $TAGS_FILE -H day -e 5000

# Test: -q only works with -r
test_error_message "Reject -q without -r" "kann nur zusammen mit" -t $TAGS_FILE -q -c 2000

# Test: -D only works with -H
test_error_message "Reject -D without -H" "kann nur zusammen mit" -t $TAGS_FILE -D 2024-11-20 -r EMS_BAT_SOC

# Test: -H requires valid type
test_error_message "Reject invalid history type" "Ungültiger History-Typ" -H invalid

# Test: -l with invalid category (negative cases will be caught by shell)
test_error_message "Reject -l with invalid category 0" "Ungültige Kategorie" -l 0
test_error_message "Reject -l with invalid category 9" "Ungültige Kategorie" -l 9

# Test: No arguments at all
test_should_fail "Reject no arguments" ""

# Test: Invalid combinations with -a (use valid values within range)
# Note: -a with -c/-d hits the power range check before the auto+manual conflict,
# because no config file is loaded (MIN/MAX_LEISTUNG = 0). We test that it still fails.
test_should_fail "Reject -a with -c" -t $TAGS_FILE -a -c 1000
test_should_fail "Reject -a with -d" -t $TAGS_FILE -a -d 1000

echo
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "Some tests failed."
    exit 1
fi
