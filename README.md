# whack

A high-performance DNS packet analyzer using DPDK (Data Plane Development Kit) for fast packet processing.

## Features

- High-Performance Packet Processing with DPDK
- Support for Multiple DNS Record Types
- Advanced Caching System with TTL Support
- Multi-Core Design for Scalability
- Real-Time Analytics and Logging

## Prerequisites

### System Requirements
- Linux operating system
- Hugepages configured
- DPDK-compatible network interface card (NIC)
- Root privileges

### Required Packages
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libnuma-dev \
    python3-pyelftools \
    libpcap-dev \
    libelf-dev \
    zlib1g-dev \
    libbsd-dev \
    meson \
    ninja-build
```

### Configure Hugepages
```bash
# Set up 1024 hugepages of 2MB each
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# Mount hugepages
sudo mkdir -p /dev/hugepages
sudo mount -t hugetlbfs nodev /dev/hugepages
```

## Building

```bash
# Build with DPDK support
sudo ./build.sh

# The script will:
# 1. Install required dependencies
# 2. Install and configure DPDK
# 3. Build the project
```

## Usage

```bash
sudo ./build/whack -i <interface> -d <domains_file> -r <resolvers_file> [options]

Options:
  -i, --interface    Network interface to use
  -d, --domains      File containing domains to resolve
  -r, --resolvers    File containing DNS resolvers
  -l, --rate-limit   Query rate limit (default: 5000)
  -o, --output       Output file for results
  -c, --cache-size   Cache size (default: 10000)
  -n, --numa-node    NUMA node to use (default: auto)
  -p, --cpu-core     CPU core to use (default: auto)
  -h, --help         Show this help message
```

## Configuration Files

### domains.txt
Contains the list of domains to resolve, one per line:
```
example.com
google.com
github.com
```

### resolvers.txt
Contains the list of DNS resolvers to use, one per line:
```
8.8.8.8
1.1.1.1
9.9.9.9
```

## Performance Tuning

1. **Hugepages**
   ```bash
   # Check hugepages status
   grep Huge /proc/meminfo
   ```

2. **NUMA Configuration**
   ```bash
   # Check NUMA topology
   numactl --hardware
   
   # Run with specific NUMA node
   sudo ./build/whack -i <interface> -n <numa_node> ...
   ```

3. **CPU Affinity**
   ```bash
   # Check CPU topology
   lscpu
   
   # Run with specific CPU core
   sudo ./build/whack -i <interface> -p <cpu_core> ...
   ```

4. **Network Interface**
   ```bash
   # Bind interface to DPDK
   sudo dpdk-devbind.py --bind=vfio-pci <interface_pci_address>
   
   # Check interface status
   sudo dpdk-devbind.py --status
   ```

## Architecture

The project is structured into several key components:

1. **Packet Processing (eth_rxtx)**
   - DPDK-based network interface handling
   - Zero-copy packet processing
   - Efficient batch processing

2. **DNS Query Handling (dns_query)**
   - DNS packet parsing and construction
   - Multiple record type support
   - Query validation

3. **Caching System (cache)**
   - High-performance memory cache
   - TTL-based entry management
   - Thread-safe operations

4. **Main Program**
   - DPDK EAL initialization
   - Configuration management
   - Statistics collection

## Troubleshooting

1. **DPDK Issues**
   ```bash
   # Check DPDK status
   sudo dpdk-devbind.py --status
   
   # Verify hugepages
   grep Huge /proc/meminfo
   ```

2. **Interface Issues**
   ```bash
   # List network interfaces
   ip link show
   
   # Check interface details
   ethtool -i <interface>
   ```

3. **Permission Issues**
   ```bash
   # Ensure proper permissions
   sudo chmod +r /dev/hugepages
   sudo usermod -aG dpdk $USER
   ```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Uses DPDK for high-performance packet processing
- Inspired by modern DNS resolver architectures
