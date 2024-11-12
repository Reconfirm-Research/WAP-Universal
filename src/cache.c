#include "../include/cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Static cache variables
static struct cache_entry *cache = NULL;
static struct cache_config config;
static size_t hit_count = 0;
static size_t miss_count = 0;

// Hash function for domain names
static uint32_t hash_domain(const char *domain) {
    uint32_t hash = 5381;
    int c;
    
    while ((c = *domain++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash;
}

void cache_init(struct cache_config *cfg) {
    // Store configuration
    memcpy(&config, cfg, sizeof(struct cache_config));
    
    // Allocate cache entries
    cache = calloc(config.max_entries, sizeof(struct cache_entry));
    if (!cache) {
        fprintf(stderr, "Failed to allocate cache memory\n");
        return;
    }
    
    // Initialize all entries as invalid
    for (size_t i = 0; i < config.max_entries; i++) {
        cache[i].valid = false;
    }
}

bool cache_lookup(const char *domain, uint8_t *response, size_t *response_len) {
    if (!cache || !domain || !response || !response_len) {
        return false;
    }
    
    uint32_t index = hash_domain(domain) % config.max_entries;
    struct cache_entry *entry = &cache[index];
    
    if (entry->valid && strcmp(entry->domain, domain) == 0) {
        // Check if entry has expired
        time_t now = time(NULL);
        if (now - entry->timestamp > entry->ttl) {
            entry->valid = false;
            miss_count++;
            return false;
        }
        
        // Return cached response
        memcpy(response, entry->response, entry->response_len);
        *response_len = entry->response_len;
        hit_count++;
        return true;
    }
    
    miss_count++;
    return false;
}

void cache_insert(const char *domain, const uint8_t *response, size_t response_len, uint32_t ttl) {
    if (!cache || !domain || !response || response_len > sizeof(cache->response)) {
        return;
    }
    
    uint32_t index = hash_domain(domain) % config.max_entries;
    struct cache_entry *entry = &cache[index];
    
    // Update entry
    strncpy(entry->domain, domain, sizeof(entry->domain) - 1);
    entry->domain[sizeof(entry->domain) - 1] = '\0';
    memcpy(entry->response, response, response_len);
    entry->response_len = response_len;
    entry->timestamp = time(NULL);
    entry->ttl = ttl > 0 ? ttl : config.default_ttl;
    entry->valid = true;
}

void cache_cleanup(void) {
    if (!cache) {
        return;
    }
    
    time_t now = time(NULL);
    size_t cleaned = 0;
    
    for (size_t i = 0; i < config.max_entries; i++) {
        if (cache[i].valid && (now - cache[i].timestamp > cache[i].ttl)) {
            cache[i].valid = false;
            cleaned++;
        }
    }
    
    if (cleaned > 0) {
        printf("Cleaned %zu expired cache entries\n", cleaned);
    }
}

void cache_destroy(void) {
    if (cache) {
        free(cache);
        cache = NULL;
    }
}

// Statistics functions
size_t cache_get_hit_count(void) {
    return hit_count;
}

size_t cache_get_miss_count(void) {
    return miss_count;
}

double cache_get_hit_ratio(void) {
    size_t total = hit_count + miss_count;
    if (total == 0) {
        return 0.0;
    }
    return (double)hit_count / total;
}
