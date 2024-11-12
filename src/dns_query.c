#include "../include/dns_query.h"
#include <string.h>
#include <arpa/inet.h>

// Convert domain name to DNS format (e.g., "www.example.com" -> "\03www\07example\03com\0")
static int encode_domain_name(const char *domain, uint8_t *buffer, size_t buffer_len) {
    size_t label_len = 0;
    size_t pos = 0;
    const char *label_start = domain;
    
    // Process each character in the domain name
    for (const char *p = domain; ; p++) {
        if (*p == '.' || *p == '\0') {
            if (label_len == 0 || label_len > 63) {
                return -1;  // Invalid label length
            }
            if (pos + label_len + 1 > buffer_len) {
                return -1;  // Buffer overflow
            }
            
            // Write label length
            buffer[pos++] = label_len;
            
            // Write label contents
            memcpy(buffer + pos, label_start, label_len);
            pos += label_len;
            
            if (*p == '\0') {
                break;
            }
            
            // Reset for next label
            label_len = 0;
            label_start = p + 1;
        } else {
            label_len++;
        }
    }
    
    // Add terminating zero
    if (pos + 1 > buffer_len) {
        return -1;
    }
    buffer[pos++] = 0;
    
    return pos;
}

void init_query(struct dns_query *query, const char *domain_name, enum DnsQType type) {
    static uint16_t query_id = 0;
    
    // Initialize header
    memset(&query->header, 0, sizeof(struct dns_header));
    query->header.id = ++query_id;
    query->header.flags = htons(0x0100);  // Standard query with recursion desired
    query->header.qdcount = htons(1);     // One question
    
    // Set query parameters
    strncpy(query->name, domain_name, sizeof(query->name) - 1);
    query->name[sizeof(query->name) - 1] = '\0';
    query->qtype = type;
    query->qclass = htons(1);  // IN class
}

int construct_query(struct dns_query *query, uint8_t *buffer, size_t *buffer_len) {
    if (*buffer_len < 512) {  // Minimum DNS message size
        return -1;
    }
    
    // Copy header
    memcpy(buffer, &query->header, sizeof(struct dns_header));
    size_t pos = sizeof(struct dns_header);
    
    // Encode domain name
    int name_len = encode_domain_name(query->name, buffer + pos, *buffer_len - pos);
    if (name_len < 0) {
        return -1;
    }
    pos += name_len;
    
    // Add query type and class
    if (pos + 4 > *buffer_len) {
        return -1;
    }
    *(uint16_t *)(buffer + pos) = htons(query->qtype);
    pos += 2;
    *(uint16_t *)(buffer + pos) = query->qclass;
    pos += 2;
    
    *buffer_len = pos;
    return 0;
}

int parse_response(const uint8_t *response, size_t response_len, struct dns_query *query) {
    if (response_len < sizeof(struct dns_header)) {
        return -1;
    }
    
    // Copy and validate header
    memcpy(&query->header, response, sizeof(struct dns_header));
    
    // Convert header fields from network byte order
    query->header.flags = ntohs(query->header.flags);
    query->header.qdcount = ntohs(query->header.qdcount);
    query->header.ancount = ntohs(query->header.ancount);
    query->header.nscount = ntohs(query->header.nscount);
    query->header.arcount = ntohs(query->header.arcount);
    
    // Check for errors in response
    if (query->header.flags & 0x000F) {  // RCODE field
        return -1;
    }
    
    return 0;
}
