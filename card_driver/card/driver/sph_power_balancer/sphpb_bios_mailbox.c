/********************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/
#include <linux/pci.h>
#include <linux/delay.h>
#include "sph_log.h"
#include "sphpb_bios_mailbox.h"


#define MCHBAR_EN BIT_ULL(0)
#define MCHBAR_MASK GENMASK_ULL(38, 16)
#define MCHBAR_SIZE BIT_ULL(16)
#define MCHBAR_LO_OFF 0x48
#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define MCHBAR_HI_OFF (MCHBAR_LO_OFF + 0x4)
#endif

#define DID_ICLI_SKU8 0x4581
#define DID_ICLI_SKU10 0x4585
#define DID_ICLI_SKU11 0x4589
#define DID_ICLI_SKU12 0x458d

static bool poll_bios_mailbox_ready(struct sphpb_pb *sphpb)
{
	u32 *mbx_interface;
	union BIOS_MAILBOX_INTERFACE iface;
	int retries = 10; /* 1ms */

	mbx_interface = sphpb->bios_mailbox_base + BIOS_MAILBOX_INTERFACE_OFFSET;

	do {
		usleep_range(100, 500);
		iface.InterfaceValue = ioread32(mbx_interface);
	} while (iface.RunBusy  && --retries > 0);

	return iface.RunBusy == 0;
}

static int write_bios_mailbox(uint8_t command, uint8_t param1, uint16_t param2,
			      uint32_t i_data, uint32_t *o_data)
{
	u32 *mbx_interface, *mbx_data;
	uint32_t verify_data, verify_data_1;
	union BIOS_MAILBOX_INTERFACE iface = {.Command = command,
					      .Param1 = param1,
					      .Param2 = param2,
					      .Reserved = 0,
					      .RunBusy = 1};
	union BIOS_MAILBOX_INTERFACE verify_iface0, verify_iface1;
	int ret = 0;

	mutex_lock(&g_the_sphpb->bios_mutex_lock);

	if (unlikely(g_the_sphpb->bios_mailbox_base == NULL)) {
		sph_log_err(POWER_BALANCER_LOG, "Mailbox is not supported - ERR (%d) !!\n", -EINVAL);
		ret = -EINVAL;
		goto err;
	}

	if (unlikely(g_the_sphpb->bios_mailbox_locked != 0)) {
		ret = -EBUSY;
		goto err;
	}

	mbx_interface = g_the_sphpb->bios_mailbox_base + BIOS_MAILBOX_INTERFACE_OFFSET;
	mbx_data = g_the_sphpb->bios_mailbox_base + BIOS_MAILBOX_DATA_OFFSET;

	if (unlikely(!poll_bios_mailbox_ready(g_the_sphpb))) {
		ret = -EBUSY;
		sph_log_err(POWER_BALANCER_LOG, "Mailbox is not ready for usage - ERR (%d) !!\n", ret);
		goto err;
	}


	iowrite32(i_data, mbx_data);
	iowrite32(iface.InterfaceValue, mbx_interface);

	if (unlikely(!poll_bios_mailbox_ready(g_the_sphpb))) {
		ret = -EBUSY;
		sph_log_err(POWER_BALANCER_LOG, "Mailbox post write is not ready for usage - ERR (%d) !!\n", ret);
		goto err;
	}

	verify_iface0.InterfaceValue = ioread32(mbx_interface);
	verify_data = ioread32(mbx_data);

	ndelay(1000);

	verify_iface1.InterfaceValue = ioread32(mbx_interface);
	verify_data_1 = ioread32(mbx_data);

	if (unlikely(verify_iface0.InterfaceValue != verify_iface1.InterfaceValue ||
	    verify_data != verify_data_1)) {
		sph_log_err(POWER_BALANCER_LOG, "Inconsistent mailbox data after write !!\n");
		ret = -EIO;
		goto err;
	}

	if (unlikely(verify_iface0.Command != 0)) {
		sph_log_err(POWER_BALANCER_LOG, "Failed to write through mailbox status=%hhu\n", verify_iface0.Command);
		ret = -EIO;
		goto err;
	}

	if (o_data)
		*o_data = verify_data;

err:
	mutex_unlock(&g_the_sphpb->bios_mutex_lock);

	return ret;
}

int sphpb_map_bios_mailbox(struct sphpb_pb *sphpb)
{
	u32 pci_dword;
	void __iomem *io_addr = NULL;
	resource_size_t mchbar_addr;
	struct pci_dev *dev0 = NULL;
	u32 icli_dids[] = {DID_ICLI_SKU8, DID_ICLI_SKU10, DID_ICLI_SKU11, DID_ICLI_SKU12};
	u32 i;

	sphpb->bios_mailbox_base = NULL;

	/* get device object of device 0 */
	for (i = 0; (dev0 == NULL) && (i < ARRAY_SIZE(icli_dids)); ++i)
		dev0 = pci_get_device(PCI_VENDOR_ID_INTEL, icli_dids[i], NULL);
	if (unlikely(dev0 == NULL)) {
		sph_log_err(POWER_BALANCER_LOG, "DID isn't supported\n");
		return -ENODEV;
	}

	/* Map MCHBAR */
	pci_read_config_dword(dev0, MCHBAR_LO_OFF, &pci_dword);
	mchbar_addr = (resource_size_t)pci_dword;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	pci_read_config_dword(dev0, MCHBAR_HI_OFF, &pci_dword);
	mchbar_addr |= (((resource_size_t)pci_dword) << 32);
#endif


	if (unlikely((mchbar_addr & MCHBAR_EN) == 0)) {
		sph_log_info(POWER_BALANCER_LOG, "MCHBAR is disabled\n");
		return -EIO;
	}

	mchbar_addr = mchbar_addr & MCHBAR_MASK;

	io_addr = ioremap(mchbar_addr + BIOS_MAILBOX_START, BIOS_MAILBOX_LENGTH);
	if (unlikely(io_addr == NULL)) {
		sph_log_err(POWER_BALANCER_LOG, "unable to map bios mailbox bar 0hunk %llx\n", mchbar_addr + BIOS_MAILBOX_START);
		return -EIO;
	}

	mutex_init(&sphpb->bios_mutex_lock);

	sphpb->bios_mailbox_base = io_addr;

	return 0;
}

void sphpb_unmap_bios_mailbox(struct sphpb_pb *sphpb)
{
	if (unlikely(sphpb->bios_mailbox_base == NULL))
		return;

	mutex_destroy(&sphpb->bios_mutex_lock);

	iounmap(sphpb->bios_mailbox_base);

	sphpb->bios_mailbox_base = NULL;
}

int set_sagv_freq(enum BIOS_SAGV_CONFIG_POLICIES qclk,
		  enum BIOS_SAGV_CONFIG_POLICIES psf0)
{
	union {
		struct {
			u32 qclk     :  4;
			u32 psf0     :  4;
			u32 reserved : 24;
		};
		u32 value;
	} data = {
		.qclk = qclk,
		.psf0 = psf0,
		.reserved = 0
	};

	return write_bios_mailbox(MAILBOX_BIOS_CMD_SAGV_CONFIG_HANDLER,
				  BIOS_SAGV_CONFIG_SET_POLICY_SUBCOMMAND,
				  0, //Param2
				  data.value,
				  NULL);
}

union mailbox_vr_imon_config {
	struct {
		u32 imon_offset : 16; //fixed point S7.8
		u32 imon_slope  : 16; //fixed point U1.15
	};
	u32 value;
};

int get_imon_sa_calib_config(uint16_t *imon_offset, //fixed point S7.8
			     uint16_t *imon_slope_factor)  //fixed point U1.15
{
	union mailbox_vr_imon_config data;
	int ret;

	ret = write_bios_mailbox(MAILBOX_BIOS_CMD_VR_IMON_CALIBRATION,
				 BIOS_IMON_CALIBRATION_SA_READ_SUBCOMMAND,
				 0,
				 0,
				 &data.value);
	if (unlikely(ret < 0))
		return ret;

	if (imon_offset != NULL)
		*imon_offset = data.imon_offset;
	if (imon_slope_factor != NULL)
		*imon_slope_factor = data.imon_slope;

	return 0;
}

int set_imon_sa_calib_config(uint16_t imon_offset, //fixed point S7.8
			     uint16_t imon_slope_factor)  //fixed point U1.15
{
	union mailbox_vr_imon_config data = {
		.imon_offset = imon_offset,
		.imon_slope  = imon_slope_factor
	};

	return write_bios_mailbox(MAILBOX_BIOS_CMD_VR_IMON_CALIBRATION,
				  BIOS_IMON_CALIBRATION_SA_WRITE_SUBCOMMAND,
				  0,
				  data.value,
				  NULL);
}

int get_imon_vccin_calib_config(uint16_t *imon_offset, //fixed point S7.8
				uint16_t *imon_slope)  //fixed point U1.15
{
	union mailbox_vr_imon_config data;
	int ret;

	ret = write_bios_mailbox(MAILBOX_BIOS_CMD_VR_IMON_CALIBRATION,
				 BIOS_IMON_CALIBRATION_VCCIN_READ_SUBCOMMAND,
				 0,
				 0,
				 &data.value);
	if (unlikely(ret < 0))
		return ret;

	if (likely(imon_offset != NULL))
		*imon_offset = data.imon_offset;
	if (likely(imon_slope != NULL))
		*imon_slope = data.imon_slope;

	return 0;
}

int set_imon_vccin_calib_config(uint16_t imon_offset, //fixed point S7.8
				uint16_t imon_slope)  //fixed point U1.15
{
	union mailbox_vr_imon_config data = {
		.imon_offset = imon_offset,
		.imon_slope  = imon_slope
	};

	return write_bios_mailbox(MAILBOX_BIOS_CMD_VR_IMON_CALIBRATION,
				  BIOS_IMON_CALIBRATION_VCCIN_WRITE_SUBCOMMAND,
				  0,
				  data.value,
				  NULL);
}

union mailbox_offset_config {
	struct {
		s16 offset; //fixed point S7.8
		u16 unused;
	};
	u32 value;
};

int get_offset_calib_config(int16_t *offset)
{
	union mailbox_offset_config data;
	int ret;

	ret = write_bios_mailbox(MAILBOX_BIOS_CMD_VR_IMON_CALIBRATION,
				 BIOS_IMON_CALIBRATION_OFFSET_READ_SUBCOMMAND,
				 0,
				 0,
				 &data.value);
	if (unlikely(ret < 0))
		return ret;

	if (likely(offset != NULL))
		*offset = data.offset;

	return 0;
}

int set_offset_calib_config(int16_t offset)
{
	union mailbox_offset_config data = {
		.offset = offset,
		.unused = 0
	};

	return write_bios_mailbox(MAILBOX_BIOS_CMD_VR_IMON_CALIBRATION,
				  BIOS_IMON_CALIBRATION_OFFSET_WRITE_SUBCOMMAND,
				  0,
				  data.value,
				  NULL);
}
