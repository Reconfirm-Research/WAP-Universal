#include "../include/cache.h"
#include <unity.h>
#include <string.h>

// Test fixtures
static struct cache_config test_config = {
    .max_entries = 100,
    .default_ttl = 300,
    .cleanup_interval = 60
};

void setUp(void) {
    cache_init(&test_config);
}

void tearDown(void) {
    cache_destroy();
}

void test_cache_insert_and_lookup(void) {
    const char *domain = "example.com";
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    const size_t test_data_len = sizeof(test_data);
    
    // Insert data into cache
    cache_insert(domain, test_data, test_data_len, 60);
    
    // Lookup the data
    uint8_t response[512];
    size_t response_len = sizeof(response);
    bool found = cache_lookup(domain, response, &response_len);
    
    // Verify results
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_UINT(test_data_len, response_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_data, response, test_data_len);
}

void test_cache_expired_entry(void) {
    const char *domain = "expired.com";
    const uint8_t test_data[] = {0x05, 0x06, 0x07, 0x08};
    const size_t test_data_len = sizeof(test_data);
    
    // Insert with very short TTL
    cache_insert(domain, test_data, test_data_len, 1);
    
    // Wait for entry to expire
    sleep(2);
    
    // Try to lookup expired entry
    uint8_t response[512];
    size_t response_len = sizeof(response);
    bool found = cache_lookup(domain, response, &response_len);
    
    // Verify entry has expired
    TEST_ASSERT_FALSE(found);
}

void test_cache_cleanup(void) {
    const char *domain1 = "test1.com";
    const char *domain2 = "test2.com";
    const uint8_t test_data[] = {0x0A, 0x0B, 0x0C, 0x0D};
    const size_t test_data_len = sizeof(test_data);
    
    // Insert entries with different TTLs
    cache_insert(domain1, test_data, test_data_len, 1);  // Short TTL
    cache_insert(domain2, test_data, test_data_len, 300);  // Long TTL
    
    // Wait for first entry to expire
    sleep(2);
    
    // Run cleanup
    cache_cleanup();
    
    // Verify results
    uint8_t response[512];
    size_t response_len = sizeof(response);
    
    // First entry should be gone
    TEST_ASSERT_FALSE(cache_lookup(domain1, response, &response_len));
    
    // Second entry should still be there
    response_len = sizeof(response);
    TEST_ASSERT_TRUE(cache_lookup(domain2, response, &response_len));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_data, response, test_data_len);
}

void test_cache_statistics(void) {
    const char *domain = "stats.com";
    const uint8_t test_data[] = {0x0E, 0x0F};
    const size_t test_data_len = sizeof(test_data);
    uint8_t response[512];
    size_t response_len;
    
    // Initial stats should be zero
    TEST_ASSERT_EQUAL_UINT(0, cache_get_hit_count());
    TEST_ASSERT_EQUAL_UINT(0, cache_get_miss_count());
    TEST_ASSERT_EQUAL_FLOAT(0.0, cache_get_hit_ratio());
    
    // First lookup should miss
    response_len = sizeof(response);
    cache_lookup(domain, response, &response_len);
    TEST_ASSERT_EQUAL_UINT(0, cache_get_hit_count());
    TEST_ASSERT_EQUAL_UINT(1, cache_get_miss_count());
    
    // Insert and lookup again
    cache_insert(domain, test_data, test_data_len, 60);
    response_len = sizeof(response);
    cache_lookup(domain, response, &response_len);
    TEST_ASSERT_EQUAL_UINT(1, cache_get_hit_count());
    TEST_ASSERT_EQUAL_UINT(1, cache_get_miss_count());
    TEST_ASSERT_EQUAL_FLOAT(0.5, cache_get_hit_ratio());
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_cache_insert_and_lookup);
    RUN_TEST(test_cache_expired_entry);
    RUN_TEST(test_cache_cleanup);
    RUN_TEST(test_cache_statistics);
    
    return UNITY_END();
}
