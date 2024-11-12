#!/bin/bash

# Exit on any error and print commands
set -e
set -x

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

# Function to clean up on error
cleanup() {
    local exit_code=$?
    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}An error occurred. Cleaning up...${NC}"
        cd /usr/local/src
        rm -rf dpdk-20.11.9* 2>/dev/null || true
    fi
    exit $exit_code
}

# Set up error handling
trap cleanup EXIT

# Function to install dependencies
install_dependencies() {
    echo "Installing dependencies..."
    apt-get update || {
        echo -e "${RED}Failed to update package list${NC}"
        exit 1
    }
    
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
        ninja-build || {
        echo -e "${RED}Failed to install required packages${NC}"
        exit 1
    }

    # Install specific DPDK version known to work
    echo "Installing DPDK..."
    DPDK_VERSION="20.11.9"
    DPDK_DIR="/usr/local/src/dpdk-${DPDK_VERSION}"
    
    # Change to /usr/local/src and handle errors
    cd /usr/local/src || {
        echo -e "${RED}Failed to change to /usr/local/src directory${NC}"
        exit 1
    }
    
    # Clean up any existing files
    rm -f dpdk-${DPDK_VERSION}.tar.xz
    rm -rf dpdk-${DPDK_VERSION} dpdk-stable-${DPDK_VERSION}
    
    # Download DPDK
    wget http://fast.dpdk.org/rel/dpdk-${DPDK_VERSION}.tar.xz || {
        echo -e "${RED}Failed to download DPDK${NC}"
        exit 1
    }
    
    # Extract DPDK
    tar xf dpdk-${DPDK_VERSION}.tar.xz || {
        echo -e "${RED}Failed to extract DPDK${NC}"
        rm -f dpdk-${DPDK_VERSION}.tar.xz
        exit 1
    }
    rm -f dpdk-${DPDK_VERSION}.tar.xz

    # Handle different directory names that might be created
    if [ -d "dpdk-stable-${DPDK_VERSION}" ]; then
        mv dpdk-stable-${DPDK_VERSION} dpdk-${DPDK_VERSION} || {
            echo -e "${RED}Failed to rename DPDK directory${NC}"
            exit 1
        }
    fi

    # Change to DPDK directory
    cd dpdk-${DPDK_VERSION} || {
        echo -e "${RED}Failed to change to DPDK directory${NC}"
        exit 1
    }

    # Configure DPDK with meson
    echo "Configuring DPDK..."
    meson setup build \
        --buildtype=release \
        --prefix=/usr \
        --libdir=lib/x86_64-linux-gnu \
        -Ddefault_library=shared \
        -Denable_docs=false \
        -Denable_kmods=false \
        -Dtests=false \
        -Dexamples='' \
        -Dcrypto=false \
        -Ddisable_drivers='crypto/*,regex/*,compress/*,vdpa/*,raw/*,baseband/*,event/*,net/mlx*,net/nfp,net/sfc,net/avp,net/bnx2x' \
        -Dmbuf_refcnt_atomic=false \
        -Dmax_ethports=1 \
        -Dmax_numa_nodes=1 || {
        echo -e "${RED}Failed to configure DPDK with meson${NC}"
        exit 1
    }

    # Build and install DPDK
    cd build || {
        echo -e "${RED}Failed to change to build directory${NC}"
        exit 1
    }
    
    ninja || {
        echo -e "${RED}Failed to build DPDK${NC}"
        exit 1
    }
    
    ninja install || {
        echo -e "${RED}Failed to install DPDK${NC}"
        exit 1
    }
    
    ldconfig || {
        echo -e "${RED}Failed to run ldconfig${NC}"
        exit 1
    }

    # Verify DPDK installation
    if ! pkg-config --modversion libdpdk; then
        echo -e "${RED}DPDK installation verification failed${NC}"
        exit 1
    fi

    # Return to original directory
    cd - || {
        echo -e "${RED}Failed to return to original directory${NC}"
        exit 1
    }
}

# Configure hugepages
setup_hugepages() {
    echo "Configuring hugepages..."
    if ! echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages; then
        echo -e "${RED}Failed to set hugepages${NC}"
        exit 1
    fi
    
    mkdir -p /dev/hugepages || {
        echo -e "${RED}Failed to create hugepages directory${NC}"
        exit 1
    }
    
    if ! mount -t hugetlbfs nodev /dev/hugepages 2>/dev/null; then
        if ! grep -qs '/dev/hugepages' /proc/mounts; then
            echo -e "${RED}Failed to mount hugepages${NC}"
            exit 1
        fi
    fi
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
    mkdir build || {
        echo -e "${RED}Failed to create build directory${NC}"
        exit 1
    }
fi

# Navigate to build directory
cd build || {
    echo -e "${RED}Failed to change to build directory${NC}"
    exit 1
}

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release || {
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
}

# Build the project
echo "Building project..."
make -j$(nproc) || {
    echo -e "${RED}Build failed${NC}"
    exit 1
}

echo -e "${GREEN}Build completed successfully!${NC}"
echo ""
echo "To run whack (requires root privileges):"
echo "sudo ./whack -i <interface> -d <domains_file> -r <resolvers.txt> [options]"
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
if ! dpdk_version=$(pkg-config --modversion libdpdk); then
    echo -e "${RED}Failed to get DPDK version${NC}"
else
    echo "DPDK version: $dpdk_version"
    echo "DPDK libraries: $(pkg-config --libs libdpdk)"
    echo "DPDK CFLAGS: $(pkg-config --cflags libdpdk)"
fi

# Check hugepages
echo -e "\nHugepage Information:"
echo "--------------------"
grep Huge /proc/meminfo || echo -e "${RED}Failed to get hugepage information${NC}"
