#include "../include/eth_rxtx.h"
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_bus_pci.h>
#include <rte_errno.h>
#include <stdio.h>
#include <string.h>

int eth_dev_init(struct eth_device *dev, const char *device_name, struct eth_config *config) {
    if (!dev || !device_name || !config) {
        return -EINVAL;
    }

    // Get port ID from device name
    if (eth_dev_get_port_by_name(device_name, &dev->port_id) < 0) {
        printf("Failed to find port for device %s\n", device_name);
        return -1;
    }

    // Copy configuration
    memcpy(&dev->config, config, sizeof(struct eth_config));

    // Create mempool for mbufs
    char pool_name[RTE_MEMPOOL_NAMESIZE];
    snprintf(pool_name, RTE_MEMPOOL_NAMESIZE, "mbuf_pool_%u", dev->port_id);

    dev->mbuf_pool = rte_pktmbuf_pool_create(pool_name,
                                            dev->config.num_mbufs,
                                            dev->config.mbuf_cache_size,
                                            0,  // priv_size
                                            dev->config.mbuf_size,
                                            rte_socket_id());

    if (dev->mbuf_pool == NULL) {
        printf("Failed to create mbuf pool: %s\n", rte_strerror(rte_errno));
        return -1;
    }

    // Configure device
    if (eth_dev_configure(dev) < 0) {
        printf("Failed to configure ethernet device\n");
        return -1;
    }

    // Get MAC address
    rte_eth_macaddr_get(dev->port_id, &dev->mac_addr);

    // Start device
    if (eth_dev_start(dev) < 0) {
        printf("Failed to start ethernet device\n");
        return -1;
    }

    // Enable promiscuous mode
    rte_eth_promiscuous_enable(dev->port_id);

    dev->initialized = true;
    return 0;
}

void eth_dev_close(struct eth_device *dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    eth_dev_stop(dev);
    rte_eth_dev_close(dev->port_id);
    
    if (dev->mbuf_pool) {
        rte_mempool_free(dev->mbuf_pool);
    }

    dev->initialized = false;
}

uint16_t eth_rx_burst(struct eth_device *dev, uint16_t queue_id, 
                      struct rte_mbuf **rx_pkts, uint16_t nb_pkts) {
    if (!dev || !dev->initialized) {
        return 0;
    }

    uint16_t nb_rx = rte_eth_rx_burst(dev->port_id, queue_id, rx_pkts, nb_pkts);
    
    if (nb_rx > 0) {
        dev->stats.rx_packets += nb_rx;
        for (uint16_t i = 0; i < nb_rx; i++) {
            dev->stats.rx_bytes += rx_pkts[i]->pkt_len;
        }
    }

    return nb_rx;
}

uint16_t eth_tx_burst(struct eth_device *dev, uint16_t queue_id,
                      struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
    if (!dev || !dev->initialized) {
        return 0;
    }

    uint16_t nb_tx = rte_eth_tx_burst(dev->port_id, queue_id, tx_pkts, nb_pkts);
    
    if (nb_tx > 0) {
        dev->stats.tx_packets += nb_tx;
        for (uint16_t i = 0; i < nb_tx; i++) {
            dev->stats.tx_bytes += tx_pkts[i]->pkt_len;
        }
    }

    if (nb_tx < nb_pkts) {
        dev->stats.tx_dropped += (nb_pkts - nb_tx);
    }

    return nb_tx;
}

int eth_dev_configure(struct eth_device *dev) {
    struct rte_eth_conf port_conf = {
        .rxmode = {
            .mq_mode = dev->config.enable_rss ? RTE_ETH_MQ_RX_RSS : RTE_ETH_MQ_RX_NONE,
        },
        .txmode = {
            .mq_mode = RTE_ETH_MQ_TX_NONE,
        },
    };

    // Configure RSS if enabled
    if (dev->config.enable_rss) {
        port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
        port_conf.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP;
    }

    // Configure the device
    int ret = rte_eth_dev_configure(dev->port_id,
                                  dev->config.rx_queues,
                                  dev->config.tx_queues,
                                  &port_conf);
    if (ret < 0) {
        printf("Failed to configure device: %s\n", rte_strerror(-ret));
        return ret;
    }

    // Set up RX queues
    for (uint16_t i = 0; i < dev->config.rx_queues; i++) {
        ret = rte_eth_rx_queue_setup(dev->port_id, i,
                                   dev->config.rx_descs,
                                   rte_eth_dev_socket_id(dev->port_id),
                                   NULL,  // Default RX conf
                                   dev->mbuf_pool);
        if (ret < 0) {
            printf("Failed to setup RX queue %u: %s\n", i, rte_strerror(-ret));
            return ret;
        }
    }

    // Set up TX queues
    for (uint16_t i = 0; i < dev->config.tx_queues; i++) {
        ret = rte_eth_tx_queue_setup(dev->port_id, i,
                                   dev->config.tx_descs,
                                   rte_eth_dev_socket_id(dev->port_id),
                                   NULL);  // Default TX conf
        if (ret < 0) {
            printf("Failed to setup TX queue %u: %s\n", i, rte_strerror(-ret));
            return ret;
        }
    }

    return 0;
}

int eth_dev_start(struct eth_device *dev) {
    int ret = rte_eth_dev_start(dev->port_id);
    if (ret < 0) {
        printf("Failed to start device: %s\n", rte_strerror(-ret));
        return ret;
    }

    // Wait for link to come up
    struct rte_eth_link link;
    printf("Waiting for link to come up...\n");
    do {
        rte_eth_link_get_nowait(dev->port_id, &link);
        rte_pause();
    } while (!link.link_status);

    printf("Link Up - speed %u Mbps - %s\n",
           link.link_speed,
           (link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX) ? 
           "full-duplex" : "half-duplex");

    return 0;
}

void eth_dev_stop(struct eth_device *dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    rte_eth_dev_stop(dev->port_id);
}

void eth_stats_get(struct eth_device *dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    struct rte_eth_stats stats;
    if (rte_eth_stats_get(dev->port_id, &stats) == 0) {
        dev->stats.rx_packets = stats.ipackets;
        dev->stats.tx_packets = stats.opackets;
        dev->stats.rx_bytes = stats.ibytes;
        dev->stats.tx_bytes = stats.obytes;
        dev->stats.rx_errors = stats.ierrors;
        dev->stats.tx_errors = stats.oerrors;
        dev->stats.rx_dropped = stats.imissed;
        dev->stats.tx_dropped = stats.oerrors;
    }
}

void eth_stats_reset(struct eth_device *dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    rte_eth_stats_reset(dev->port_id);
    memset(&dev->stats, 0, sizeof(struct eth_stats));
}

void eth_stats_print(struct eth_device *dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    eth_stats_get(dev);
    printf("\nPort %u statistics:\n", dev->port_id);
    printf("  RX packets: %" PRIu64 "\n", dev->stats.rx_packets);
    printf("  TX packets: %" PRIu64 "\n", dev->stats.tx_packets);
    printf("  RX bytes:   %" PRIu64 "\n", dev->stats.rx_bytes);
    printf("  TX bytes:   %" PRIu64 "\n", dev->stats.tx_bytes);
    printf("  RX errors:  %" PRIu64 "\n", dev->stats.rx_errors);
    printf("  TX errors:  %" PRIu64 "\n", dev->stats.tx_errors);
    printf("  RX dropped: %" PRIu64 "\n", dev->stats.rx_dropped);
    printf("  TX dropped: %" PRIu64 "\n", dev->stats.tx_dropped);
}

int eth_dev_socket_id(struct eth_device *dev) {
    if (!dev) {
        return -1;
    }
    return rte_eth_dev_socket_id(dev->port_id);
}

bool eth_dev_is_valid_port(uint16_t port_id) {
    return rte_eth_dev_is_valid_port(port_id);
}

int eth_dev_get_port_by_name(const char *name, uint16_t *port_id) {
    return rte_eth_dev_get_port_by_name(name, port_id);
}
