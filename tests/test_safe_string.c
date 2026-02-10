/*
 * Tests for safe_string_copy function
 */

#include "test_framework.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Copy of safe_string_copy from e3dcset.cpp for testing */
static bool safe_string_copy(char* dest, size_t dest_size, const char* src, const char* field_name) {
    size_t src_len = strlen(src);
    
    if (src_len >= dest_size) {
        fprintf(stderr, "FEHLER: Config-Wert für '%s' zu lang (max %zu Zeichen, gelesen: %zu)\n",
                field_name, dest_size - 1, src_len);
        fprintf(stderr, "        Bitte Config-Datei überprüfen.\n");
        return false;
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';  // Null-Terminierung sicherstellen
    return true;
}

TEST(safe_string_copy_normal) {
    char buffer[32];
    memset(buffer, 'X', sizeof(buffer));
    
    bool result = safe_string_copy(buffer, sizeof(buffer), "test123", "test_field");
    
    ASSERT(result == true, "Should return true for valid input");
    ASSERT_STR_EQ("test123", buffer, "Should copy string correctly");
    PASS();
}

TEST(safe_string_copy_empty) {
    char buffer[32];
    memset(buffer, 'X', sizeof(buffer));
    
    bool result = safe_string_copy(buffer, sizeof(buffer), "", "test_field");
    
    ASSERT(result == true, "Should return true for empty string");
    ASSERT_STR_EQ("", buffer, "Should copy empty string correctly");
    PASS();
}

TEST(safe_string_copy_max_length) {
    char buffer[8];
    memset(buffer, 'X', sizeof(buffer));
    
    // String mit genau dest_size-1 Zeichen (passt noch)
    bool result = safe_string_copy(buffer, sizeof(buffer), "1234567", "test_field");
    
    ASSERT(result == true, "Should return true for max-length string");
    ASSERT_STR_EQ("1234567", buffer, "Should copy max-length string correctly");
    ASSERT(buffer[7] == '\0', "Should null-terminate at end");
    PASS();
}

TEST(safe_string_copy_too_long) {
    char buffer[8];
    memset(buffer, 'X', sizeof(buffer));
    
    // String mit dest_size Zeichen (zu lang)
    bool result = safe_string_copy(buffer, sizeof(buffer), "12345678", "test_field");
    
    ASSERT(result == false, "Should return false for too-long string");
    PASS();
}

TEST(safe_string_copy_way_too_long) {
    char buffer[8];
    memset(buffer, 'X', sizeof(buffer));
    
    bool result = safe_string_copy(buffer, sizeof(buffer), "this is a very long string", "test_field");
    
    ASSERT(result == false, "Should return false for way-too-long string");
    PASS();
}

TEST(safe_string_copy_null_termination) {
    char buffer[16];
    memset(buffer, 'X', sizeof(buffer));
    
    safe_string_copy(buffer, sizeof(buffer), "test", "test_field");
    
    ASSERT(buffer[4] == '\0', "Should null-terminate after string");
    ASSERT(strlen(buffer) == 4, "strlen should return correct length");
    PASS();
}

int main(void) {
    printf("\n=== Testing safe_string_copy ===\n");
    
    RUN_TEST(safe_string_copy_normal);
    RUN_TEST(safe_string_copy_empty);
    RUN_TEST(safe_string_copy_max_length);
    RUN_TEST(safe_string_copy_too_long);
    RUN_TEST(safe_string_copy_way_too_long);
    RUN_TEST(safe_string_copy_null_termination);
    
    TEST_SUMMARY();
}
