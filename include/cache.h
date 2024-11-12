#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Cache entry structure
struct cache_entry {
    char domain[256];           // Domain name
    uint8_t response[512];      // DNS response data
    size_t response_len;        // Length of response
    time_t timestamp;           // Time when entry was added
    uint32_t ttl;              // Time-to-live in seconds
    bool valid;                // Entry validity flag
};

// Cache configuration structure
struct cache_config {
    size_t max_entries;         // Maximum number of entries in cache
    uint32_t default_ttl;       // Default TTL for entries without explicit TTL
    uint32_t cleanup_interval;  // Interval for cleanup of expired entries
};

// Function declarations
void cache_init(struct cache_config *config);
bool cache_lookup(const char *domain, uint8_t *response, size_t *response_len);
void cache_insert(const char *domain, const uint8_t *response, size_t response_len, uint32_t ttl);
void cache_cleanup(void);
void cache_destroy(void);

// Statistics functions
size_t cache_get_hit_count(void);
size_t cache_get_miss_count(void);
double cache_get_hit_ratio(void);

#endif // CACHE_H
