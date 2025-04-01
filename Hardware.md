# Intel® Nervana™ NNP-I 1100 Series Hardware Specification

This document provides a comprehensive summary of the known technical specifications and architectural details for the Intel® Nervana™ Neural Network Processor for Inference (NNP-I) 1100 series.

---

## Overview

The Intel® Nervana™ NNP-I 1100 is a specialized neural network inference accelerator designed to deliver high throughput and efficiency for deep learning inference workloads in data center and edge environments. It is built using Intel's 10nm FinFET process technology and leverages a combination of dedicated inference compute engines (ICE), integrated CPU cores, and advanced memory architecture.

## General Specifications

- **Model:** Intel® Nervana™ NNP-I 1100 / NNP-I 1150
- **Codename:** Spring Hill
- **Process Technology:** Intel 10nm FinFET CMOS
- **Die Size:** 239 mm²
- **Transistor Count:** ~8.5 billion
- **Form Factor:** M.2 Accelerator Card
- **Thermal Design Power (TDP):** 12 W (up to ~25 W)
- **Peak Performance:** 50 TOPS at INT8 precision
- **Intended Use:** Deep learning inference acceleration for data center and edge deployments

## Compute Architecture

The NNP-I 1100 Series integrates multiple dedicated hardware engines optimized for inference:

### Inference Compute Engines (ICE)

- **Total Number of ICEs:** 12
- **ICE Configuration:** Arranged in 6 pairs per ICL x86 core (12 ICE units total)
- **Compute Grid per ICE:**
  - Structure: 4D Grid (32×32×4)
  - Operations: Supports INT8, FP16, 4-bit, 2-bit, and 1-bit precision
  - Compute Capability: 4K MAC/cycle per ICE
- **Integrated Vector Processor:**
  - Customized Tensilica Vision P6 DSP
  - 512-bit vector width, VLIW load
  - Supports FP16, 8-bit to 32-bit integer operations

### Integrated CPU Cores

- **CPU Microarchitecture:** Intel Sunny Cove (based on Ice Lake architecture)
- **Number of CPU Cores:** 2 cores
- **Supported Instruction Sets:** AVX-512, AVX-VNNI
- **Role:** Software orchestration, runtime management, workload scheduling, and flexibility for programmable logic

## Memory Architecture

The NNP-I 1100 / 1150 has a hierarchical memory architecture designed to minimize latency and maximize throughput:

- **Tightly-Coupled Memory (TCM):**
  - Total: 3 MiB (256 KiB per ICE)
  - Bandwidth: ~68 TB/s (on-chip)

- **Deep SRAM:**
  - Total: 48 MiB (4 MiB per ICE)
  - Bandwidth: ~6.8 TB/s (on-chip)

- **Last Level Cache (LLC):**
  - Total: 24 MiB
  - Configuration: 8 cache slices × 3 MiB each
  - Bandwidth: ~680 GB/s

- **External Memory Controller:**
  - Type: LPDDR4X-4266
  - Onboard Memory: 32 GiB
  - Memory Channels: Quad-channel (4×32-bit)
  - Max Bandwidth: 67.2 GB/s
  - ECC Support: Yes

## Power Management

The NNP-I 1100 features dynamic and integrated power management capabilities, including:

- **Fully Integrated Voltage Regulation (FIVR)**
- **Dynamic Voltage and Frequency Scaling (DVFS)**
- **Thermal and power-aware workload scheduling**

These capabilities enable efficient distribution and dynamic adjustment of power and performance according to workload requirements and available thermal headroom.

## System and Communication Interfaces

- **PCIe Interface:**
  - Generation: PCIe 4/8x Gen3 (4x used on M.2)
  - Role: Main communication channel with host system for loading workloads and managing inference tasks

- **On-Chip Communication:**
  - Integrated ring bus architecture connecting ICE pairs, CPU cores, and LLC slices
  - Special synchronization units for efficient inter-unit communication

## Form Factor and Packaging

- **Form Factor:** M.2 Accelerator Card
- **Package Dimensions:** Optimized for integration into servers, edge devices, and existing infrastructure
- **Cooling and Power Requirements:** Designed for passive cooling solutions, suitable for integration into standard data-center and edge-server hardware

## Deployment Scenarios

The NNP-I 1100 supports multiple deployment scenarios:

- **Latency-Optimized Configuration:**
  - Example: 2 concurrent inference applications, each mapped to individual ICE units, batch size of 1.

- **Throughput-Optimized Configuration:**
  - Example: Multiple concurrent inference applications (e.g., 6 applications), each leveraging multiple ICEs, batch size increased for maximum throughput.

## Supported Data Types

- INT8 (primary target precision)
- FP16 (native support)
- Lower precision inference: 4-bit, 2-bit, 1-bit (hardware native support)

## Software Compatibility

The NNP-I 1100 series integrates seamlessly with common deep learning frameworks and inference runtimes through:

- Linux-based kernel drivers
- User-space libraries
- Compatibility with popular frameworks including TensorFlow, PyTorch, and ONNX via nGraph

## References and Further Reading

For deeper technical information and presentations, see the following resources:

- [Intel® Nervana™ NNPI Brief](https://intel.ai/nervana-nnp/nnpi)
- [HC31 Intel Spring Hill Presentation (HotChips)](https://old.hotchips.org/hc31/HC31_2.6_Intel_SPH_2019_v3.pdf)
- [NNP-I Interactive Experience Mobile (v0.5)](https://community.intel.com/cipcp26785/attachments/cipcp26785/developer-cloud/1633/2/16433-1_NNP-I_Interactive_Experience_Mobile_v0.5.pdf)
- [NNP-I Announcement Brief (v5.1)](https://community.intel.com/cipcp26785/attachments/cipcp26785/developer-cloud/1633/1/16433-1_NNP-announce_NNP-I_brief_v5.1.pdf)
- [WikiChip: Intel Spring Hill Architecture](https://en.wikichip.org/wiki/intel/microarchitectures/spring_hill)
- [WikiChip: Nervana NNP-I 1100 Details](https://en.wikichip.org/wiki/nervana/nnp/nnp-i_1100)
