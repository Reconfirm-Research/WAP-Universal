#include "../include/dns_query.h"
#include <unity.h>
#include <string.h>
#include <arpa/inet.h>

void setUp(void) {
    // Setup code if needed
}

void tearDown(void) {
    // Cleanup code if needed
}

void test_init_query(void) {
    struct dns_query query;
    const char *domain = "example.com";
    
    // Initialize query
    init_query(&query, domain, A);
    
    // Verify query fields
    TEST_ASSERT_EQUAL_STRING(domain, query.name);
    TEST_ASSERT_EQUAL_INT(A, query.qtype);
    TEST_ASSERT_EQUAL_INT(1, ntohs(query.qclass));  // Should be IN class
    TEST_ASSERT_NOT_EQUAL(0, query.header.id);  // ID should be non-zero
    TEST_ASSERT_EQUAL_INT(0x0100, ntohs(query.header.flags));  // Standard query with recursion
    TEST_ASSERT_EQUAL_INT(1, ntohs(query.header.qdcount));  // One question
    TEST_ASSERT_EQUAL_INT(0, ntohs(query.header.ancount));  // No answers
    TEST_ASSERT_EQUAL_INT(0, ntohs(query.header.nscount));  // No authority records
    TEST_ASSERT_EQUAL_INT(0, ntohs(query.header.arcount));  // No additional records
}

void test_construct_query(void) {
    struct dns_query query;
    const char *domain = "test.com";
    uint8_t buffer[512];
    size_t buffer_len = sizeof(buffer);
    
    // Initialize and construct query
    init_query(&query, domain, A);
    int result = construct_query(&query, buffer, &buffer_len);
    
    // Verify construction success
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_TRUE(buffer_len > sizeof(struct dns_header));
    
    // Verify header fields in constructed packet
    struct dns_header *header = (struct dns_header *)buffer;
    TEST_ASSERT_EQUAL_INT(query.header.id, header->id);
    TEST_ASSERT_EQUAL_INT(query.header.flags, header->flags);
    TEST_ASSERT_EQUAL_INT(query.header.qdcount, header->qdcount);
    
    // Verify domain name encoding
    uint8_t *qname = buffer + sizeof(struct dns_header);
    TEST_ASSERT_EQUAL_INT(4, qname[0]);  // Length of "test"
    TEST_ASSERT_EQUAL_INT('t', qname[1]);
    TEST_ASSERT_EQUAL_INT('e', qname[2]);
    TEST_ASSERT_EQUAL_INT('s', qname[3]);
    TEST_ASSERT_EQUAL_INT('t', qname[4]);
    TEST_ASSERT_EQUAL_INT(3, qname[5]);  // Length of "com"
}

void test_parse_response(void) {
    // Create a mock DNS response
    uint8_t response[512] = {0};
    struct dns_header *header = (struct dns_header *)response;
    header->id = htons(1234);
    header->flags = htons(0x8180);  // Standard response
    header->qdcount = htons(1);
    header->ancount = htons(1);
    size_t response_len = sizeof(struct dns_header) + 20;  // Header + some data
    
    // Parse the response
    struct dns_query query;
    int result = parse_response(response, response_len, &query);
    
    // Verify parsing success
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1234, ntohs(query.header.id));
    TEST_ASSERT_EQUAL_INT(0x8180, ntohs(query.header.flags));
    TEST_ASSERT_EQUAL_INT(1, ntohs(query.header.qdcount));
    TEST_ASSERT_EQUAL_INT(1, ntohs(query.header.ancount));
}

void test_invalid_response(void) {
    // Test with too small response
    uint8_t small_response[10] = {0};
    struct dns_query query;
    int result = parse_response(small_response, sizeof(small_response), &query);
    
    // Verify parsing failure
    TEST_ASSERT_NOT_EQUAL(0, result);
    
    // Test with invalid response code
    uint8_t error_response[512] = {0};
    struct dns_header *header = (struct dns_header *)error_response;
    header->flags = htons(0x8183);  // Response with error code
    result = parse_response(error_response, sizeof(struct dns_header), &query);
    
    // Verify parsing failure
    TEST_ASSERT_NOT_EQUAL(0, result);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_init_query);
    RUN_TEST(test_construct_query);
    RUN_TEST(test_parse_response);
    RUN_TEST(test_invalid_response);
    
    return UNITY_END();
}
