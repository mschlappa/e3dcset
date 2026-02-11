#include "catch.hpp"
#include "config.h"

TEST_CASE("History interval constants are correct", "[constants]") {
    SECTION("Day interval is 15 minutes") {
        REQUIRE(HISTORY_INTERVAL_DAY == 900);
        REQUIRE(HISTORY_INTERVAL_DAY == 15 * 60); // 15 minutes in seconds
    }
    
    SECTION("Week interval is 1 hour") {
        REQUIRE(HISTORY_INTERVAL_WEEK == 3600);
        REQUIRE(HISTORY_INTERVAL_WEEK == 60 * 60); // 1 hour in seconds
    }
    
    SECTION("Month interval is 1 day") {
        REQUIRE(HISTORY_INTERVAL_MONTH == 86400);
        REQUIRE(HISTORY_INTERVAL_MONTH == 24 * 60 * 60); // 1 day in seconds
    }
    
    SECTION("Year interval is 1 week") {
        REQUIRE(HISTORY_INTERVAL_YEAR == 604800);
        REQUIRE(HISTORY_INTERVAL_YEAR == 7 * 24 * 60 * 60); // 1 week in seconds
    }
}

TEST_CASE("History span constants are correct", "[constants]") {
    SECTION("Day span is 24 hours") {
        REQUIRE(HISTORY_SPAN_DAY == 86400);
        REQUIRE(HISTORY_SPAN_DAY == 24 * 60 * 60); // 24 hours in seconds
    }
    
    SECTION("Week span is 7 days") {
        REQUIRE(HISTORY_SPAN_WEEK == 604800);
        REQUIRE(HISTORY_SPAN_WEEK == 7 * 24 * 60 * 60); // 7 days in seconds
    }
    
    SECTION("Month span is 31 days") {
        REQUIRE(HISTORY_SPAN_MONTH == 2678400);
        REQUIRE(HISTORY_SPAN_MONTH == 31 * 24 * 60 * 60); // 31 days in seconds
    }
    
    SECTION("Year span is 365 days") {
        REQUIRE(HISTORY_SPAN_YEAR == 31536000);
        REQUIRE(HISTORY_SPAN_YEAR == 365 * 24 * 60 * 60); // 365 days in seconds
    }
}

TEST_CASE("History intervals align with their spans", "[constants]") {
    SECTION("Day: 15min intervals fit into 24h") {
        int intervals = HISTORY_SPAN_DAY / HISTORY_INTERVAL_DAY;
        REQUIRE(intervals == 96); // 24h * 60min / 15min
        REQUIRE((HISTORY_SPAN_DAY % HISTORY_INTERVAL_DAY) == 0); // No remainder
    }
    
    SECTION("Week: 1h intervals fit into 7 days") {
        int intervals = HISTORY_SPAN_WEEK / HISTORY_INTERVAL_WEEK;
        REQUIRE(intervals == 168); // 7 * 24 hours
        REQUIRE((HISTORY_SPAN_WEEK % HISTORY_INTERVAL_WEEK) == 0);
    }
    
    SECTION("Month: 1 day intervals fit into 31 days") {
        int intervals = HISTORY_SPAN_MONTH / HISTORY_INTERVAL_MONTH;
        REQUIRE(intervals == 31);
        REQUIRE((HISTORY_SPAN_MONTH % HISTORY_INTERVAL_MONTH) == 0);
    }
    
    SECTION("Year: 1 week intervals fit into 365 days") {
        int intervals = HISTORY_SPAN_YEAR / HISTORY_INTERVAL_YEAR;
        REQUIRE(intervals == 52); // 52 weeks + 1 day (but we use 52)
        // Note: 365 days % 7 days = 1 day remainder (expected)
        REQUIRE((HISTORY_SPAN_YEAR % HISTORY_INTERVAL_YEAR) == 86400);
    }
}

TEST_CASE("Interval/span ratios are reasonable for E3DC queries", "[constants]") {
    SECTION("Day query: ~100 data points") {
        int points = HISTORY_SPAN_DAY / HISTORY_INTERVAL_DAY;
        REQUIRE(points >= 90);
        REQUIRE(points <= 100);
    }
    
    SECTION("Week query: ~150-200 data points") {
        int points = HISTORY_SPAN_WEEK / HISTORY_INTERVAL_WEEK;
        REQUIRE(points >= 150);
        REQUIRE(points <= 200);
    }
    
    SECTION("Month query: 30-35 data points") {
        int points = HISTORY_SPAN_MONTH / HISTORY_INTERVAL_MONTH;
        REQUIRE(points >= 30);
        REQUIRE(points <= 35);
    }
    
    SECTION("Year query: 50-55 data points") {
        int points = HISTORY_SPAN_YEAR / HISTORY_INTERVAL_YEAR;
        REQUIRE(points >= 50);
        REQUIRE(points <= 55);
    }
}

// Helper function to check if a tag needs BAT_REQ_DATA container wrapping
static bool isBatContainerRequired(uint32_t tag) {
    return (tag & RSCP_TAG_BAT_NAMESPACE_MASK) == RSCP_TAG_BAT_NAMESPACE &&
           (tag & RSCP_TAG_BAT_CONTAINER_MASK) != RSCP_TAG_BAT_CONTAINER_PREFIX;
}

TEST_CASE("BAT namespace detection", "[bat-container]") {
    SECTION("BAT_REQ tags need container wrapping") {
        REQUIRE(isBatContainerRequired(0x03000001) == true);  // BAT_REQ_RSOC
        REQUIRE(isBatContainerRequired(0x0300000F) == true);  // BAT_REQ_ASOC
        REQUIRE(isBatContainerRequired(0x03000008) == true);  // BAT_REQ_CHARGE_CYCLES
        REQUIRE(isBatContainerRequired(0x03000002) == true);  // BAT_REQ_MODULE_VOLTAGE
        REQUIRE(isBatContainerRequired(0x03000003) == true);  // BAT_REQ_CURRENT
    }

    SECTION("BAT container tags must NOT be wrapped") {
        REQUIRE(isBatContainerRequired(0x03040000) == false);  // BAT_REQ_DATA
        REQUIRE(isBatContainerRequired(0x03040001) == false);  // BAT_INDEX
    }

    SECTION("EMS tags must NOT be wrapped") {
        REQUIRE(isBatContainerRequired(0x01000001) == false);  // EMS tag
        REQUIRE(isBatContainerRequired(0x01000008) == false);  // EMS_REQ_POWER_PV
        REQUIRE(isBatContainerRequired(0x01000009) == false);  // EMS_REQ_POWER_BAT
    }

    SECTION("Other namespaces must NOT be wrapped") {
        REQUIRE(isBatContainerRequired(0x02000001) == false);  // PVI namespace
        REQUIRE(isBatContainerRequired(0x06000001) == false);  // DB namespace
        REQUIRE(isBatContainerRequired(0x00000001) == false);  // RSCP namespace
        REQUIRE(isBatContainerRequired(0x0A000001) == false);  // INFO namespace
    }
}

// Helper function to check if a tag needs PVI_REQ_DATA container wrapping
static bool isPviContainerRequired(uint32_t tag) {
    return (tag & RSCP_TAG_PVI_NAMESPACE_MASK) == RSCP_TAG_PVI_NAMESPACE &&
           (tag & RSCP_TAG_PVI_CONTAINER_MASK) != RSCP_TAG_PVI_CONTAINER_PREFIX &&
           !(tag & RSCP_TAG_RESPONSE_BIT);
}

TEST_CASE("PVI namespace detection", "[pvi-container]") {
    SECTION("PVI_REQ tags need container wrapping") {
        REQUIRE(isPviContainerRequired(0x02000001) == true);   // PVI_REQ_ON_GRID
        REQUIRE(isPviContainerRequired(0x02000002) == true);   // PVI_REQ_STATE
        REQUIRE(isPviContainerRequired(0x02000003) == true);   // PVI_REQ_LAST_ERROR
        REQUIRE(isPviContainerRequired(0x02000009) == true);   // PVI_REQ_TYPE
        REQUIRE(isPviContainerRequired(0x02000060) == true);   // PVI_REQ_COS_PHI
        REQUIRE(isPviContainerRequired(0x02000085) == true);   // PVI_REQ_SYSTEM_MODE
    }

    SECTION("PVI_REQ_DEVICE_STATE needs container wrapping") {
        REQUIRE(isPviContainerRequired(0x02060000) == true);   // PVI_REQ_DEVICE_STATE
    }

    SECTION("PVI container tags must NOT be wrapped") {
        REQUIRE(isPviContainerRequired(0x02040000) == false);  // PVI_REQ_DATA
        REQUIRE(isPviContainerRequired(0x02040001) == false);  // PVI_INDEX
        REQUIRE(isPviContainerRequired(0x02040005) == false);  // PVI_VALUE
    }

    SECTION("PVI response tags must NOT be wrapped") {
        REQUIRE(isPviContainerRequired(0x02800001) == false);  // PVI_ON_GRID
        REQUIRE(isPviContainerRequired(0x02800002) == false);  // PVI_STATE
        REQUIRE(isPviContainerRequired(0x02840000) == false);  // PVI_DATA
        REQUIRE(isPviContainerRequired(0x02860000) == false);  // PVI_DEVICE_STATE
        REQUIRE(isPviContainerRequired(0x02860001) == false);  // PVI_DEVICE_CONNECTED
    }

    SECTION("Other namespaces must NOT be wrapped as PVI") {
        REQUIRE(isPviContainerRequired(0x03000001) == false);  // BAT namespace
        REQUIRE(isPviContainerRequired(0x01000001) == false);  // EMS namespace
        REQUIRE(isPviContainerRequired(0x06000001) == false);  // DB namespace
        REQUIRE(isPviContainerRequired(0x00000001) == false);  // RSCP namespace
    }
}
