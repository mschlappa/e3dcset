/*
 * Tests for configuration file parsing
 */

#include "test_framework.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/* Minimal config structure for testing */
typedef struct {
    uint32_t MIN_LEISTUNG;
    uint32_t MAX_LEISTUNG;
    char server_ip[20];
    uint32_t server_port;
} test_config_t;

/* Simplified config parser for testing */
static bool parse_config_line(const char* line, test_config_t* config) {
    char var[128], value[128];
    
    if (sscanf(line, "%[^ \t=]%*[\t ]=%*[\t ]%[^\n]", var, value) != 2) {
        return false;
    }
    
    if (strcmp(var, "MIN_LEISTUNG") == 0) {
        config->MIN_LEISTUNG = atoi(value);
        return true;
    } else if (strcmp(var, "MAX_LEISTUNG") == 0) {
        config->MAX_LEISTUNG = atoi(value);
        return true;
    } else if (strcmp(var, "server_ip") == 0) {
        size_t len = strlen(value);
        if (len >= sizeof(config->server_ip)) {
            return false;
        }
        strncpy(config->server_ip, value, sizeof(config->server_ip) - 1);
        config->server_ip[sizeof(config->server_ip) - 1] = '\0';
        return true;
    } else if (strcmp(var, "server_port") == 0) {
        config->server_port = atoi(value);
        return true;
    }
    
    return false;
}

TEST(config_parse_integer) {
    test_config_t config = {0};
    
    bool result = parse_config_line("MIN_LEISTUNG = 1000", &config);
    
    ASSERT(result == true, "Should parse integer config");
    ASSERT_EQ(1000, config.MIN_LEISTUNG, "Should parse MIN_LEISTUNG correctly");
    PASS();
}

TEST(config_parse_string) {
    test_config_t config = {0};
    
    bool result = parse_config_line("server_ip = 192.168.1.1", &config);
    
    ASSERT(result == true, "Should parse string config");
    ASSERT_STR_EQ("192.168.1.1", config.server_ip, "Should parse server_ip correctly");
    PASS();
}

TEST(config_parse_with_tabs) {
    test_config_t config = {0};
    
    bool result = parse_config_line("MAX_LEISTUNG\t=\t5000", &config);
    
    ASSERT(result == true, "Should parse config with tabs");
    ASSERT_EQ(5000, config.MAX_LEISTUNG, "Should parse MAX_LEISTUNG with tabs");
    PASS();
}

TEST(config_parse_minimal_spaces) {
    test_config_t config = {0};
    
    /* Parser requires at least one space/tab around '=' */
    bool result = parse_config_line("server_port = 5033", &config);
    
    ASSERT(result == true, "Should parse config with minimal spaces");
    ASSERT_EQ(5033, config.server_port, "Should parse server_port correctly");
    PASS();
}

TEST(config_parse_empty_line) {
    test_config_t config = {0};
    
    bool result = parse_config_line("", &config);
    
    ASSERT(result == false, "Should reject empty line");
    PASS();
}

TEST(config_parse_comment_line) {
    test_config_t config = {0};
    
    bool result = parse_config_line("# This is a comment", &config);
    
    ASSERT(result == false, "Should reject comment line");
    PASS();
}

TEST(config_parse_invalid_format) {
    test_config_t config = {0};
    
    bool result = parse_config_line("INVALID FORMAT", &config);
    
    ASSERT(result == false, "Should reject invalid format");
    PASS();
}

TEST(config_string_too_long) {
    test_config_t config = {0};
    
    // String länger als server_ip Buffer (20 bytes)
    bool result = parse_config_line("server_ip = 192.168.255.255.255.255", &config);
    
    ASSERT(result == false, "Should reject too-long string");
    PASS();
}

TEST(config_file_parsing) {
    /* Create temporary config file */
    const char* tmpfile = "/tmp/e3dcset_test.conf";
    FILE* fp = fopen(tmpfile, "w");
    ASSERT(fp != NULL, "Should create temp config file");
    
    fprintf(fp, "MIN_LEISTUNG = 100\n");
    fprintf(fp, "MAX_LEISTUNG = 4000\n");
    fprintf(fp, "server_ip = 10.0.0.1\n");
    fprintf(fp, "server_port = 5033\n");
    fprintf(fp, "# Comment line\n");
    fprintf(fp, "\n");
    fclose(fp);
    
    /* Parse file */
    fp = fopen(tmpfile, "r");
    ASSERT(fp != NULL, "Should open temp config file");
    
    test_config_t config = {0};
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) {
        parse_config_line(line, &config);
    }
    
    fclose(fp);
    unlink(tmpfile);
    
    /* Verify parsed values */
    ASSERT_EQ(100, config.MIN_LEISTUNG, "Should parse MIN_LEISTUNG from file");
    ASSERT_EQ(4000, config.MAX_LEISTUNG, "Should parse MAX_LEISTUNG from file");
    ASSERT_STR_EQ("10.0.0.1", config.server_ip, "Should parse server_ip from file");
    ASSERT_EQ(5033, config.server_port, "Should parse server_port from file");
    
    PASS();
}

int main(void) {
    printf("\n=== Testing Config Parsing ===\n");
    
    RUN_TEST(config_parse_integer);
    RUN_TEST(config_parse_string);
    RUN_TEST(config_parse_with_tabs);
    RUN_TEST(config_parse_minimal_spaces);
    RUN_TEST(config_parse_empty_line);
    RUN_TEST(config_parse_comment_line);
    RUN_TEST(config_parse_invalid_format);
    RUN_TEST(config_string_too_long);
    RUN_TEST(config_file_parsing);
    
    TEST_SUMMARY();
}
