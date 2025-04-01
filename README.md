# NNPI‑Card Project (Community Fork)

> **Important Notice:**  
> This project is a community-maintained fork of Intel’s original NNPI‑Card project. Intel has discontinued development and no longer provides maintenance, bug fixes, or updates. Community contributions, patches, and forks are strongly encouraged.

## Overview

The NNPI‑Card Project provides an open-source software stack for the Intel® Nervana™ Neural Network Processor for Inference (NNP‑I), specifically targeting high-volume and low-latency AI inference workloads.

The stack includes:

- **Kernel Driver:**  
  PCIe driver to initialize and manage the NNP‑I card, enabling multi-user access and workload sharing. It loads the embedded OS onto the card at boot.

- **Embedded OS (Buildroot-based):**  
  Custom Linux OS specifically designed for the NNP-I hardware, booted directly on the card.

- **Inference Compute Engine (ICE) Firmware:**  
  Specialized firmware to manage and optimize the performance of the 12 dedicated inference compute engines.

- **User-Space Tools and Libraries:**  
  Components enabling integration with popular deep learning frameworks (e.g., TensorFlow, PyTorch, ONNX).

## Hardware Overview

The NNP‑I 1100 series cards are purpose-built accelerators optimized for deep learning inference, delivering up to 50 TOPS (Int8 precision) within a compact and power-efficient form factor (M.2, ~12 W). The card includes 12 specialized inference compute engines (ICEs) and two integrated CPU cores with Intel AVX and VNNI instructions for optimal workload management.

For detailed specifications, refer to [Hardware.md](Hardware.md).

## Software Features

- **Dynamic Workload Management:**  
  Efficiently manages multiple inference tasks concurrently.

- **Comprehensive Memory Hierarchy:**  
  Includes tightly coupled SRAM, cache hierarchy, and high-bandwidth LPDDR4X memory.

- **Flexible Integration:**  
  Provides straightforward integration paths for major deep learning frameworks.

## Project Structure

```
/nnpi-card-update
├── LICENSE.txt               # GPL license
├── README.md                 # This document
├── Hardware.md               # Detailed hardware specifications
├── build_toolchain.sh        # Toolchain and OS build script
├── card_driver               # Kernel driver and upstream patches
│   ├── card
│   ├── common
│   ├── linux_upstream
│   └── upstream_patches
├── ice_driver                # ICE firmware sources
│   ├── Module.symvers
│   ├── driver
│   ├── external
│   └── supporting files
├── nnpi_os_buildroot         # Buildroot environment
├── nnpi_os_external_sources  # Source packages and licenses
└── versions.txt              # Changelog and version details
```

## Building the Project

### Prerequisites

Ubuntu packages required:

```bash
sudo apt-get update
sudo apt-get install sed make binutils gcc g++ bash patch gzip bzip2 perl tar cpio python3 unzip rsync libncurses-dev libelf-dev libssl-dev bison libarchive-dev
```

Ensure corresponding packages for CentOS or other distributions are installed.

### Build Steps

1. **Fetch Dependencies:**

```bash
git submodule update --init --recursive
```

2. **Build the Toolchain and Embedded OS:**

```bash
./build_toolchain.sh [output_dir]
```

- Default `output_dir`: `nnpi_os_buildroot/build`

Example:

```bash
./build_toolchain.sh nnpi_os_buildroot/build
```

### Deploying to the Card

- OS image located at `nnpi_os_buildroot/build/images/rootfs.cpio`.
- Kernel driver (`card_driver`) loads the OS onto the NNP-I card automatically during boot.

### Building Kernel Driver & Firmware

1. Build the kernel driver from `card_driver`.
2. Build ICE firmware from `ice_driver`.
3. Deploy and validate with the provided tools.

## Contributing

- **Bug Reports & Features:** Open repository issues.
- **Code Contributions:** Submit pull requests; adhere to established coding guidelines.
- **Documentation Improvements:** Updates and clarifications welcomed.

## License

Distributed under the GNU General Public License (GPL). See [LICENSE.txt](LICENSE.txt).

## Acknowledgements

- **Intel® Nervana™ NNPI-I:** Original design by Intel; community-maintained fork.
- **Linux Kernel Community:** Insights and design details from [LKML](https://lore.kernel.org/lkml/20210512071046.34941-1-guy.zadicario@intel.com/).
- **Buildroot Community:** Foundation for embedded OS support.

### Further Information

- [Intel® Nervana™ NNPI Brief](https://intel.ai/nervana-nnp/nnpi)
- [HC31 Intel SPH Presentation (2019)](https://old.hotchips.org/hc31/HC31_2.6_Intel_SPH_2019_v3.pdf)
- [NNP-I Interactive Experience Mobile (v0.5)](https://community.intel.com/cipcp26785/attachments/cipcp26785/developer-cloud/1633/2/16433-1_NNP-I_Interactive_Experience_Mobile_v0.5.pdf)
- [NNP-I Announcement Brief (v5.1)](https://community.intel.com/cipcp26785/attachments/cipcp26785/developer-cloud/1633/1/16433-1_NNP-announce_NNP-I_brief_v5.1.pdf)
- [Intel Spring Hill Microarchitecture](https://en.wikichip.org/wiki/intel/microarchitectures/spring_hill)
- [Nervana NNP-I 1100 Series Details](https://en.wikichip.org/wiki/nervana/nnp/nnp-i_1100)


