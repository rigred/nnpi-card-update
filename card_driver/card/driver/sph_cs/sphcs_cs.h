/********************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/

#ifndef _SPHCS_H
#define _SPHCS_H

#include "nnp_types.h"
#include "sphcs_pcie.h"
#include <linux/device.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/scatterlist.h>
#include <linux/hashtable.h>
#include "sphcs_dma_sched.h"
#include "dma_page_pool.h"
#include "msg_scheduler.h"
#include "periodic_timer.h"
#include "sphcs_sw_counters.h"
#include "sphcs_hwtrace.h"


struct inf_data;
struct sphcs_cmd_chan;

#define MAX_NUM_CHANNELS (1 << NNP_IPC_CHANNEL_BITS)

struct sphcs {
	void          *hw_handle;
	struct device *hw_device;
	const struct   sphcs_pcie_hw_ops *hw_ops;

	pool_handle             dma_page_pool;
	pool_handle             net_dma_page_pool;
	struct sphcs_dma_sched *dmaSched;

	struct work_struct      host_disconnect_work;

	struct inf_data   *inf_data;

	struct workqueue_struct *wq;
	u32                      host_connected;
	u32                      host_doorbell_val;

	struct msg_scheduler       *respq_sched;
	struct msg_scheduler_queue *public_respq;

	struct periodic_timer       periodic_timer;
	struct notifier_block mce_notifier;
	struct delayed_work init_delayed_reset;

	u8 channel_created[MAX_NUM_CHANNELS];

	union nnp_inbound_mem     *inbound_mem;
	size_t inbound_mem_size;
	dma_addr_t inbound_mem_dma_addr;

	size_t     host_sys_info_num_page;
	dma_addr_t host_sys_info_dma_addr;
	bool       host_sys_info_dma_addr_valid;

	spinlock_t lock_bh;
	DECLARE_HASHTABLE(cmd_chan_hash, 6);

	struct kobject             *kobj;
	struct dentry              *debugfs_dir;
	struct sphcs_hwtrace_data	hw_tracing;
};

extern struct sphcs_pcie_callbacks g_sphcs_pcie_callbacks;

extern struct sphcs *g_the_sphcs;   /* a  global pointer to the sphcs object - currently a singleton */

typedef int (*sphcs_command_handler)(struct sphcs *sphcs, u64 *msg, u32 size);
typedef int (*sphcs_chan_command_handler)(struct sphcs *sphcs, struct sphcs_cmd_chan *chan, u64 *msg, u32 size);

void sphcs_send_event_report(struct sphcs			*sphcs,
			      uint16_t				event_code,
			      uint16_t				event_val,
			      struct msg_scheduler_queue	*respq,
			      int				context_id,
			      int				obj_id);

void sphcs_send_event_report_ext(struct sphcs			*sphcs,
				 uint16_t			event_code,
				 uint16_t			event_val,
				 struct msg_scheduler_queue	*respq,
				 int				context_id,
				 int				obj_id_1,
				 int				obj_id_2);

struct msg_scheduler_queue *sphcs_create_response_queue(struct sphcs *sphcs,
							       u32 weight);

int sphcs_destroy_response_queue(struct sphcs               *sphcs,
				 struct msg_scheduler_queue *respq);

static inline int sphcs_msg_scheduler_queue_add_msg(struct msg_scheduler_queue *queue, u64 *msg, int size)
{
	if (NNP_SW_GROUP_IS_ENABLE(g_nnp_sw_counters, SPHCS_SW_COUNTERS_GROUP_IPC))
		NNP_SW_COUNTER_INC(g_nnp_sw_counters, SPHCS_SW_COUNTERS_IPC_COMMANDS_SCHEDULED_COUNT);

	return msg_scheduler_queue_add_msg(queue, msg, size);
}

typedef void (*sphcs_alloc_resource_callback)(struct sphcs *sphcs,
					     void         *ctx,
					     int           dmabuf_fd,
					     int           status);

int sphcs_alloc_resource(struct sphcs                 *sphcs,
			 uint64_t                      size,
			 uint32_t                      page_size,
			 sphcs_alloc_resource_callback cb,
			 void                          *ctx);

int sphcs_free_resource(struct sphcs  *sphcs,
			int            dmabuf_fd);

typedef void (*hostres_pagetable_cb)(void                  *cb_ctx,
				     int                    status,
				     struct sg_table       *host_sgt,
				     uint64_t               total_size);

int sphcs_retrieve_hostres_pagetable(uint64_t             hostDmaAddr,
				     hostres_pagetable_cb completion_cb,
				     void                *cb_ctx);

struct sphcs_cmd_chan *sphcs_find_channel(struct sphcs *sphcs, uint16_t protocol_id);
#endif
