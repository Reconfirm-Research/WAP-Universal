#ifndef DNS_QUERY_H
#define DNS_QUERY_H

#include <stdint.h>
#include <stdlib.h>

// DNS Query Types
enum DnsQType {
    A = 1,          // IPv4 address record
    NS = 2,         // Nameserver record
    CNAME = 5,      // Canonical name record
    SOA = 6,        // Start of authority record
    PTR = 12,       // Pointer record
    MX = 15,        // Mail exchange record
    TXT = 16,       // Text record
    AAAA = 28,      // IPv6 address record
    OPT = 41        // EDNS record
};

// DNS Header structure
struct dns_header {
    uint16_t id;            // Identification number
    uint16_t flags;         // DNS flags
    uint16_t qdcount;       // Number of questions
    uint16_t ancount;       // Number of answers
    uint16_t nscount;       // Number of authority records
    uint16_t arcount;       // Number of additional records
};

// DNS Query structure
struct dns_query {
    struct dns_header header;
    enum DnsQType qtype;
    char name[256];         // Domain name
    uint16_t qclass;        // Query class (usually IN=1)
};

// Function declarations
int construct_query(struct dns_query *query, uint8_t *buffer, size_t *buffer_len);
int parse_response(const uint8_t *response, size_t response_len, struct dns_query *query);
void init_query(struct dns_query *query, const char *domain_name, enum DnsQType type);

#endif // DNS_QUERY_H
