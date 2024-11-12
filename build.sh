#!/bin/bash

# Exit on any error
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Building whack...${NC}"

# Function to check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Error: Please run as root (sudo)${NC}"
        echo "Root privileges are required to install dependencies"
        exit 1
    fi
}

# Function to install dependencies
install_dependencies() {
    echo "Installing dependencies..."
    apt-get update
    apt-get install -y \
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

    # Install specific DPDK version known to work
    echo "Installing DPDK..."
    DPDK_VERSION="20.11.9"
    DPDK_DIR="/usr/local/src/dpdk-${DPDK_VERSION}"
    
    cd /usr/local/src
    if [ -f "dpdk-${DPDK_VERSION}.tar.xz" ]; then
        rm dpdk-${DPDK_VERSION}.tar.xz
    fi
    
    wget http://fast.dpdk.org/rel/dpdk-${DPDK_VERSION}.tar.xz
    tar xf dpdk-${DPDK_VERSION}.tar.xz
    rm dpdk-${DPDK_VERSION}.tar.xz

    # Check if extraction was successful
    if [ ! -d "dpdk-${DPDK_VERSION}" ] && [ ! -d "dpdk-stable-${DPDK_VERSION}" ]; then
        echo -e "${RED}Error: DPDK extraction failed${NC}"
        exit 1
    fi

    # Handle different directory names that might be created
    if [ -d "dpdk-stable-${DPDK_VERSION}" ]; then
        mv dpdk-stable-${DPDK_VERSION} dpdk-${DPDK_VERSION}
    fi

    cd dpdk-${DPDK_VERSION}

    # Configure DPDK with minimal drivers and disable warnings
    echo "Configuring DPDK..."
    meson build \
        -Dexamples='' \
        -Dtests=false \
        -Denable_docs=false \
        -Dmaximum_drivers=false \
        -Ddisable_drivers="crypto/*,event/*,raw/*,vdpa/*,baseband/*" \
        -Dc_args="-w -Wno-error"

    cd build
    ninja
    ninja install
    ldconfig

    # Verify DPDK installation
    pkg-config --modversion libdpdk
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: DPDK installation failed${NC}"
        exit 1
    fi

    # Return to original directory
    cd -
}

# Configure hugepages
setup_hugepages() {
    echo "Configuring hugepages..."
    echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
    mkdir -p /dev/hugepages
    mount -t hugetlbfs nodev /dev/hugepages || true  # Don't fail if already mounted
}

# Check for required packages
echo "Checking dependencies..."
if ! pkg-config --exists libdpdk || ! pkg-config --exists libnuma; then
    echo -e "${YELLOW}Missing required packages. Installing dependencies...${NC}"
    check_root
    install_dependencies
fi

# Setup hugepages
if [ ! -d "/dev/hugepages" ] || [ $(cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages) -eq 0 ]; then
    echo -e "${YELLOW}Setting up hugepages...${NC}"
    check_root
    setup_hugepages
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

# Navigate to build directory
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
echo "Building project..."
make -j$(nproc)

echo -e "${GREEN}Build completed successfully!${NC}"
echo ""
echo "To run whack (requires root privileges):"
echo "sudo ./whack -i <interface> -d <domains_file> -r <resolvers_file> [options]"
echo ""
echo "Note: Make sure your network interface is supported by DPDK"
echo ""
echo "For more information, see README.md"

# Print system information
echo -e "\nSystem Information:"
echo "-------------------"
echo "Kernel version: $(uname -r)"
echo "Architecture: $(uname -m)"
if [ -f "/etc/os-release" ]; then
    . /etc/os-release
    echo "OS: $PRETTY_NAME"
fi
echo "Compiler: $(cc --version | head -n1)"
echo "CMake: $(cmake --version | head -n1)"

# Print DPDK information
echo -e "\nDPDK Information:"
echo "----------------"
echo "DPDK version: $(pkg-config --modversion libdpdk)"
echo "DPDK libraries: $(pkg-config --libs libdpdk)"
echo "DPDK CFLAGS: $(pkg-config --cflags libdpdk)"

# Check hugepages
echo -e "\nHugepage Information:"
echo "--------------------"
grep Huge /proc/meminfo
