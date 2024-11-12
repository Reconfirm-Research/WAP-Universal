#ifndef ETH_RXTX_H
#define ETH_RXTX_H

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <stdint.h>
#include <stdbool.h>

// Default configuration values
#define DEFAULT_RX_DESCRIPTORS 1024
#define DEFAULT_TX_DESCRIPTORS 1024
#define DEFAULT_MEMPOOL_CACHE_SIZE 512
#define DEFAULT_MBUF_SIZE 2048
#define DEFAULT_NUM_MBUFS 8192

// Statistics structure
struct eth_stats {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
};

// Device configuration structure
struct eth_config {
    uint16_t rx_queues;
    uint16_t tx_queues;
    uint16_t rx_descs;
    uint16_t tx_descs;
    uint32_t mbuf_cache_size;
    uint32_t mbuf_size;
    uint32_t num_mbufs;
    bool enable_rss;
};

// Ethernet device structure
struct eth_device {
    uint16_t port_id;
    struct rte_mempool *mbuf_pool;
    struct rte_ether_addr mac_addr;
    struct eth_config config;
    struct eth_stats stats;
    bool initialized;
};

// Function declarations
int eth_dev_init(struct eth_device *dev, const char *device_name, struct eth_config *config);
void eth_dev_close(struct eth_device *dev);

// Packet handling functions
uint16_t eth_rx_burst(struct eth_device *dev, uint16_t queue_id, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t eth_tx_burst(struct eth_device *dev, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

// Statistics functions
void eth_stats_get(struct eth_device *dev);
void eth_stats_reset(struct eth_device *dev);
void eth_stats_print(struct eth_device *dev);

// Helper functions
int eth_dev_configure(struct eth_device *dev);
int eth_dev_start(struct eth_device *dev);
void eth_dev_stop(struct eth_device *dev);
int eth_dev_socket_id(struct eth_device *dev);
bool eth_dev_is_valid_port(uint16_t port_id);
int eth_dev_get_port_by_name(const char *name, uint16_t *port_id);

#endif // ETH_RXTX_H
