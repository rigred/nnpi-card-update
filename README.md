# NNPI‑Card Project (Community Fork)

> **Important Notice:**  
> This project is a community-maintained fork of Intel’s original NNPI‑Card project. Intel discontinued development and no longer provides maintenance, bug fixes, or updates. If you rely on NNPI‑Card or wish to continue its evolution, you are encouraged to contribute, maintain patches, or create your own fork.

## Overview

The NNPI‑Card Project provides the complete open source stack for Intel® Nervana™ Neural Network Processor for Inference (NNP‑I). This stack consists of:

- **Kernel Driver:**  
  A PCIe driver that initializes and manages the NNP‑I card, which supports multi-user access and simultaneous processing of inference workloads. The driver is responsible for loading the OS image into the card’s firmware during boot.

- **Firmware & Embedded OS:**  
  A Buildroot-based OS image designed specifically to run on the NNPI‑I card. Once loaded by the driver, this embedded Linux OS powers the card’s onboard CPU cores and inference compute engines.

- **ICE (Inference Compute Engine) Driver:**  
  Firmware and software components (collectively referred to as the ICE driver) that manage the specialized inference hardware, 12 dedicated Inference Compute Engines (ICEs) and associated on-die memory, to deliver high-throughput, low-latency deep learning inference.

- **User-Space Tools and Libraries:**  
  Components that enable user applications and deep learning frameworks to interface with the card, offloading inference computations to the hardware accelerator.

## Hardware & Software Features

- **Purpose-Built for Inference:**  
  The NNP‑I card is engineered for high-volume, low-latency AI inference. With power envelopes ranging from 10 W (M.2 card) to 75 W (PCIe card), the design is scalable from the network edge to data centers.

- **Innovative Hardware Architecture:**  
  - **12 Inference Compute Engines (ICEs):** Optimized for low-precision arithmetic (e.g., INT8) to maximize inference throughput.  
  - **Dual Integrated CPU Cores:** These cores, featuring Intel® AVX and VNNI support, provide the programmability needed to efficiently map and manage inference workloads.
  - **On-Die Memory Hierarchy:** A combination of large local SRAM, dedicated caches, and a coherent network-on-chip minimizes data latency and maximizes throughput.
  - **Dynamic Power Management:** Fully integrated voltage regulation (FIVR) adjusts power distribution to maintain high efficiency across different workloads and power envelopes.

- **Embedded OS for the NNPI‑I Card:**  
  Unlike traditional OS images that run on host systems, the Buildroot-based OS image in this project is loaded into the NNPI‑I card itself. The card’s firmware boots this OS, which in turn provides a runtime environment for the inference compute engines and associated user-space services.

- **Open, Flexible Software:**  
  The complete software stack, ranging from the low-level kernel driver to high-level user-space libraries, is designed for integration with popular deep learning frameworks (such as TensorFlow, PyTorch, and ONNX). The system supports direct integration or custom backend development for inference acceleration.

## Project Structure

```
/nnpi-card-update
├── LICENSE.txt               # License file (GPL)
├── README.md                 # This file
├── build_toolchain.sh        # Script to build the toolchain and OS image
├── card_driver               # NNPI kernel driver source and upstream patches
│   ├── card
│   ├── common
│   ├── linux_upstream
│   └── upstream_patches
├── ice_driver                # ICE driver source and supporting firmware
│   ├── Module.symvers
│   ├── driver
│   ├── external
│   └── other supporting files
├── nnpi_os_buildroot         # Buildroot-based OS build environment (for NNPI‑I card)
├── nnpi_os_external_sources  # External source tarballs and licenses
└── versions.txt              # Version information and changelog
```

## Build Instructions

### Prerequisites

Ensure that your host system has all required packages installed. For Ubuntu:

```bash
sudo apt-get update
sudo apt-get install sed make binutils gcc g++ bash patch gzip bzip2 perl tar cpio python3 unzip rsync libncurses-dev libelf-dev libssl-dev bison libarchive-dev
```

For CentOS, install the corresponding packages.

### Building the Toolchain & Embedded OS

The provided `build_toolchain.sh` script creates a complete Buildroot-based toolchain and builds the OS image that will run on the NNPI‑I card.

1. **Run the Toolchain Build Script:**

   ```bash
   ./build_toolchain.sh [output_dir]
   ```

   - If `output_dir` is not specified, it defaults to `nnpi_os_buildroot/build`.
   - The script performs the following:
     - Creates the output directory.
     - Removes any previous toolchain build.
     - Appends necessary configuration options (e.g., `BR2_COMPILER_PARANOID_UNSAFE_PATH=n`) to the defconfig.
     - Runs `make SPH_x86_64_efi_nnpi_defconfig` followed by `make sdk` to build the toolchain and OS image.
     - Unpacks and renames the SDK image into a directory named `toolchain`.

   **Example:**

   ```bash
   ./build_toolchain.sh nnpi_os_buildroot/build
   ```

   Expected output:
   ```
   toolchain output directory: nnpi_os_buildroot/build/build
   Toolchain is ready at nnpi_os_buildroot/build/toolchain
   ```

2. **Build Issues**

    **Note:** Before building, if you encounter errors such as "No rule to make target '/.../.br2-external.mk'", run:

    ```bash
    git submodule update --init --recursive
    ``` 

    This ensures all required external sources are fetched.

### Deploying the OS Image to the NNPI‑I Card

When the OS image build completes, the resulting image (typically a CPIO archive) is not intended to run on your host system, it is loaded into the NNPI‑I card. The kernel driver (in the `card_driver` directory) is responsible for loading this OS image into the card at boot time. 

- The built OS image is found in `nnpi_os_buildroot/build/images/rootfs.cpio`.
- This image is loaded into the NNPI‑I card by the driver during initialization, so that the card’s embedded CPU cores run this OS.

### Building the Full System

Once you have built the toolchain and OS image, you can integrate them with the NNPI‑I kernel driver and firmware:

1. **Build the Kernel Driver:**

   Follow the instructions in the `card_driver` directory to build the NNPI‑I kernel driver for your target kernel.

2. **Build the Firmware:**

   The firmware (in the `card_driver` and possibly `ice_driver` directories) is responsible for initializing the card and preparing it to receive the OS image.

3. **Deploy & Test:**

   - Ensure that the host system loads the NNPI‑I driver.
   - The driver will transfer the OS image (from `rootfs.cpio`) to the NNPI‑I card during boot.
   - Once loaded, the card will run its own embedded Linux OS, which is your NNPI‑OS.

## Contributing

Since this is a community-maintained fork:
- **Bug Reports & Feature Requests:** Please open issues on the repository.
- **Code Contributions:** Submit patches via pull requests. Follow the coding style and guidelines established in the project.
- **Documentation:** Help improve this README and related documentation.

## License

This project is distributed under the GNU General Public License (GPL). See [LICENSE.txt](LICENSE.txt) for details.

## Acknowledgements

- **Intel® Nervana™ NNPI‑I:**  
  The original hardware and software were developed by Intel. This fork is maintained by the community to ensure continued support and evolution.
- **Linux Kernel Mailing List Contributions:**  
  Many insights and design details were provided on the [LKML](https://lore.kernel.org/lkml/20210512071046.34941-1-guy.zadicario@intel.com/).
- **Buildroot & Open Source Community:**  
  Thanks to Buildroot developers and contributors for providing a robust build environment.

For further details on NNPI‑I hardware and software, please refer to:
- [Intel® Nervana™ NNPI Brief](https://intel.ai/nervana-nnp/nnpi)
- [HC31 Intel SPH Presentation (2019)](https://old.hotchips.org/hc31/HC31_2.6_Intel_SPH_2019_v3.pdf)
- [NNP-I Interactive Experience (Mobile, v0.5)](https://community.intel.com/cipcp26785/attachments/cipcp26785/developer-cloud/1633/2/16433-1_NNP-I_Interactive_Experience_Mobile_v0.5.pdf)
- [NNP-I Announcement Brief (v5.1)](https://community.intel.com/cipcp26785/attachments/cipcp26785/developer-cloud/1633/1/16433-1_NNP-announce_NNP-I_brief_v5.1.pdf)
- [Intel Spring Hill Microarchitecture](https://en.wikichip.org/wiki/intel/microarchitectures/spring_hill)
- [Nervana NNP-I 1100 Details](https://en.wikichip.org/wiki/nervana/nnp/nnp-i_1100)

