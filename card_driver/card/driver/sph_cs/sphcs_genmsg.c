/********************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/anon_inodes.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/hashtable.h>
#include <linux/atomic.h>
#include "dma_page_pool.h"
#include "sphcs_cs.h"
#include "ipc_protocol.h"
#include "nnp_debug.h"
#include "sph_log.h"
#include "ioctl_genmsg.h"
#include "sphcs_dma_sched.h"
#include "sphcs_cmd_chan.h"

#define SPH_MAX_GENERIC_SERVICES (32UL + 256UL)

static struct cdev s_cdev;
static dev_t       s_devnum;
static struct class *s_class;
static struct device *s_dev;

#pragma pack(push, 1)
union h2c_GenericMessaging {
	struct {
		u64 opcode   :  6;   /* NNP_IPC_H2C_OP_GENERIC_MSG_PACKET */
		u64 size     : 12;
		u64 connect  :  1;
		u64 hangup   :  1;
		u64 host_pfn : NNP_IPC_DMA_PFN_BITS;

		u64 host_client_id  : 12;
		u64 card_client_id  : 12;
		u64 host_page_hndl  :  8;
		u64 reserved        : 29;
		u64 service_list_req:  1;
		u64 privileged      :  1;
	};

	u64 value[2];
};
#pragma pack(pop)

struct genmsg_global_data {
	DECLARE_HASHTABLE(channel_hash, 5);
	struct ida        channel_ida;
	spinlock_t        lock;
};
static struct genmsg_global_data s_genmsg;

struct channel_data;

struct genmsg_dma_command_data {
	void       *vptr;
	page_handle dma_page_hndl;
	dma_addr_t  dma_addr;
	dma_addr_t  host_dma_addr;
	struct channel_data *channel;
	union h2c_GenericMessaging msg;
	struct sphcs_cmd_chan *cmd_chan;
};

struct pending_packet {
	struct genmsg_dma_command_data dma_data;
	int                            is_hangup_command;
	struct list_head               node;
};

enum channel_close_state {
	_chnl_open	  = 0, /* channel is operative */
	_chnl_preclose	  = 1, /* work is being done before channel can be closed */
	_chnl_close_ready = 2, /* work is done, channel can be close if... hangup is set */
	_chnl_close_done  = 3  /* Close / free is being done or done and ownership
				* is with a different thread
				*/
};

struct channel_data {
	struct kref	  ref;
	u16               host_client_id;
	int               fd;
	bool              is_privileged;
	struct file      *file;
	struct list_head  pending_read_packets;
	spinlock_t        read_lock;
	struct pending_packet *current_read_packet;
	u32                    current_read_size;
	u32               n_read_dma_req;
	struct sphcs_cmd_chan *cmd_chan;

	u16                channel_id;
	struct hlist_node  hash_node;

	enum channel_close_state closing;
	int               hanging_up;
	bool              io_error;

	wait_queue_head_t read_waitq;

	int               write_host_page_valid;
	page_handle       write_host_page_hndl;
	dma_addr_t        write_host_page_addr;
	page_handle       write_page_hndl;
	dma_addr_t        write_page_addr;
	void             *write_page_vptr;
	wait_queue_head_t write_waitq;
	atomic_t          n_write_dma_req;
	struct mutex      write_lock;
};

struct service_data {
	int              id;
	u64              host_client_handle;
	struct list_head pending_connections;
	spinlock_t       lock;

	wait_queue_head_t waitq;
};

static struct sphcs_genmsg_service_list {
	struct ida       ida;
	struct mutex     lock;
	const char      *service_name[SPH_MAX_GENERIC_SERVICES];
	size_t           service_name_len[SPH_MAX_GENERIC_SERVICES];
	struct service_data *service_data[SPH_MAX_GENERIC_SERVICES];
	uint32_t             num_services;
} *s_service_list = NULL;

struct dma_req_user_data {
	page_handle dma_page_hndl;
	page_handle host_dma_page_hndl;
	void       *dma_vptr;
	u32         xfer_size;
	u32         param1;
	union {
		struct channel_data *channel;
		struct sphcs_cmd_chan *cmd_chan;
	};
};

static void sphcs_chan_genmsg_hangup(struct sphcs_cmd_chan *chan, void *cb_ctx);

static void free_channel(struct kref *kref)
{
	struct channel_data *channel = container_of(kref,
						   struct channel_data,
						   ref);

	NNP_ASSERT(channel->closing == _chnl_close_done && channel->hanging_up);

	mutex_destroy(&channel->write_lock);
	channel->cmd_chan->destroy_cb = NULL;
	sphcs_cmd_chan_put(channel->cmd_chan);
	NNP_SPIN_LOCK(&s_genmsg.lock);
	hash_del(&channel->hash_node);
	NNP_SPIN_UNLOCK(&s_genmsg.lock);
	ida_simple_remove(&s_genmsg.channel_ida, channel->channel_id);

	kfree(channel);
}

static int channel_put(struct channel_data *chan)
{
	return kref_put(&chan->ref, free_channel);
}

static struct channel_data *find_channel(u16 channel_id)
{
	struct channel_data *channel;

	NNP_SPIN_LOCK(&s_genmsg.lock);
	hash_for_each_possible(s_genmsg.channel_hash,
			       channel,
			       hash_node,
			       channel_id)
		if (channel->channel_id == channel_id) {
			/* Make sure we have a valid reference to channel:
			 * reference count may go down in the middle
			 */
			if (!kref_get_unless_zero(&channel->ref))
				channel = NULL;
			NNP_SPIN_UNLOCK(&s_genmsg.lock);
			return channel;
		}

	NNP_SPIN_UNLOCK(&s_genmsg.lock);
	return NULL;
}

static int chan_response_dma_completed(struct sphcs *sphcs,
				       void *ctx,
				       const void *user_data,
				       int status,
				       u32 timeUS);

/***************************************************************************
 * Connected Channel file descriptor operations
 ***************************************************************************/
static inline int is_channel_file(struct file *f);

static int sphcs_genmsg_chan_release(struct inode *inode, struct file *f)
{
	struct channel_data *channel = (struct channel_data *)f->private_data;
	struct pending_packet *pend;
	struct list_head pending_read_packets;
	union c2h_ChanGenericMessaging msg2;

	if (!is_channel_file(f))
		return -EINVAL;

	/* move pending read packets to a list on local stack */
	NNP_SPIN_LOCK(&channel->read_lock);
	channel->closing = _chnl_preclose;
	INIT_LIST_HEAD(&pending_read_packets);
	list_splice_init(&channel->pending_read_packets, &pending_read_packets);
	if (channel->current_read_packet) {
		/* add the current read packet if exist */
		list_add_tail(&channel->current_read_packet->node, &pending_read_packets);
		channel->current_read_packet = NULL;
	}
	NNP_SPIN_UNLOCK(&channel->read_lock);

	/* release all packets in the pending read list */
	while (!list_empty(&pending_read_packets)) {
		pend = list_first_entry(&pending_read_packets, struct pending_packet, node);
		list_del(&pend->node);

		if (!pend->is_hangup_command)
			dma_page_pool_set_page_free(g_the_sphcs->dma_page_pool, pend->dma_data.dma_page_hndl);
		kfree(pend);
	}

	/*
	 * Wait for all pending write dma requests to complete before sending
	 * the hangup packet
	 */
	wait_event_interruptible(channel->write_waitq,
				 atomic_read(&channel->n_write_dma_req) == 0);

	/* return write pages back to pool if we still hold it */
	mutex_lock(&channel->write_lock);
	if (channel->write_page_vptr)
		dma_page_pool_set_page_free(g_the_sphcs->dma_page_pool, channel->write_page_hndl);

	mutex_unlock(&channel->write_lock);

	/*
	 * Send hangup message to host
	 */
	msg2.value = 0LL;
	msg2.opcode = NNP_IPC_C2H_OP_CHAN_GENERIC_MSG_PACKET;
	msg2.chan_id = channel->cmd_chan->protocol_id;
	msg2.rb_id = 0;
	msg2.size = 0;
	msg2.hangup = 1;
	msg2.card_client_id = channel->channel_id;

	sphcs_msg_scheduler_queue_add_msg(channel->cmd_chan->respq, &msg2.value, 1);

	/* free the channel if we aleady got hangup message from host
	 * otherwise, it will be freed once a hangup message is arrived
	 */
	NNP_SPIN_LOCK(&channel->read_lock);
	channel->closing = _chnl_close_ready;
	if (channel->hanging_up) {
		channel->closing = _chnl_close_done;
		NNP_SPIN_UNLOCK(&channel->read_lock);
		channel_put(channel);
	} else {
		NNP_SPIN_UNLOCK(&channel->read_lock);
	}

	return 0;
}

static ssize_t sphcs_genmsg_chan_read(struct file *f,
				      char __user *buf,
				      size_t       size,
				      loff_t      *off)
{
	struct channel_data *channel = (struct channel_data *)f->private_data;
	size_t read_size;
	ssize_t ret;

	if (unlikely(!is_channel_file(f)))
		return -EINVAL;

	if (channel->io_error)
		return -EIO;

	if (!channel->current_read_packet) {

		/* wait for a pending read packets */
		ret = wait_event_interruptible(channel->read_waitq,
				!list_empty(&channel->pending_read_packets));
		if (unlikely(ret < 0))
			return ret;

		NNP_SPIN_LOCK(&channel->read_lock);
		NNP_ASSERT(!list_empty(&channel->pending_read_packets));
		channel->current_read_packet = list_first_entry(
						&channel->pending_read_packets,
						struct pending_packet, node);
		list_del(&channel->current_read_packet->node);
		NNP_SPIN_UNLOCK(&channel->read_lock);

		channel->current_read_size = 0;
	}

	read_size = channel->current_read_packet->dma_data.msg.size + 1 -
		    channel->current_read_size;

	if (read_size == 0 || !channel->current_read_packet->dma_data.vptr) {
		/* it must be a hangup packet */
		NNP_ASSERT(channel->current_read_packet->is_hangup_command);
		kfree(channel->current_read_packet);
		channel->current_read_packet = NULL;
		channel->current_read_size = 0;
		return 0;
	}

	if (size < read_size)
		read_size = size;

	ret = copy_to_user(buf,
			   channel->current_read_packet->dma_data.vptr +
			   channel->current_read_size,
			   read_size);
	ret = read_size - ret;
	channel->current_read_size += ret;

	/* free the current read packet if all was read */
	if (channel->current_read_size >=
	    channel->current_read_packet->dma_data.msg.size + 1) {
		/* return the local dma page back to the dma pool */
		dma_page_pool_set_page_free(g_the_sphcs->dma_page_pool,
			channel->current_read_packet->dma_data.dma_page_hndl);
		kfree(channel->current_read_packet);
		channel->current_read_packet = NULL;
		channel->current_read_size = 0;
	}

	if (unlikely(ret == 0))
		return -EFAULT;

	return ret;
}

static ssize_t sphcs_genmsg_chan_write(struct file       *f,
				       const char __user *buf,
				       size_t             size,
				       loff_t            *off)
{
	struct channel_data *channel = (struct channel_data *)f->private_data;
	ssize_t n_written = 0;
	size_t write_size, max_write_size;
	struct dma_req_user_data dma_req_data;
	int ret = 0;

	if (unlikely(!is_channel_file(f)))
		return -EINVAL;

	if (channel->io_error)
		return -EIO;

	/* do not send a zero size data packet */
	if (unlikely(size == 0))
		return 0;

	max_write_size = NNP_PAGE_SIZE;

	mutex_lock(&channel->write_lock);

	do {
		if (channel->hanging_up) {
			ret = -EPIPE;
			break;
		}

		// Need to have a host response page for sending data to host
		if (!channel->write_host_page_valid) {
			struct sphcs_host_rb *resp_data_rb = &channel->cmd_chan->c2h_rb[0];
			uint32_t chunk_size;
			int n;

			n = host_rb_wait_free_space(resp_data_rb,
						    NNP_PAGE_SIZE,
						    1,
						    &channel->write_host_page_addr,
						    &chunk_size);
			if (n != 1 || chunk_size != NNP_PAGE_SIZE) {
				sph_log_err(SERVICE_LOG, "Failed to get host response page for write n=%d chunk_size=%u\n", n, chunk_size);
				/* end the write loop and return */
				break;
			}
			host_rb_update_free_space(resp_data_rb, NNP_PAGE_SIZE);
			channel->write_host_page_valid = 1;
		}

		/* need to have local dma address for copying data from user */
		if (!channel->write_page_vptr) {
			ret = dma_page_pool_get_free_page(
						g_the_sphcs->dma_page_pool,
						&channel->write_page_hndl,
						&channel->write_page_vptr,
						&channel->write_page_addr);
			if (unlikely(ret < 0)) {
				sph_log_err(SERVICE_LOG, "Failed to get free dma page for write ret=%d\n", ret);
				/* end the write loop and return */
				break;
			}
		}

		write_size = min(size - n_written, max_write_size);

		ret = copy_from_user(channel->write_page_vptr, buf + n_written, write_size);
		if (unlikely(ret != 0)) {
			sph_log_err(SERVICE_LOG, "Failed to read data from user\n");
			/* end the write loop and return */
			break;
		}

		/* start DMA for transfering the copied packet to host */
		dma_req_data.dma_page_hndl = channel->write_page_hndl;
		dma_req_data.host_dma_page_hndl = channel->write_host_page_hndl;
		dma_req_data.dma_vptr = channel->write_page_vptr;
		dma_req_data.xfer_size = write_size;
		dma_req_data.channel = channel;

		/* Increment number of write dma requests */
		atomic_inc(&channel->n_write_dma_req);

		ret = sphcs_dma_sched_start_xfer_single(g_the_sphcs->dmaSched,
						&channel->cmd_chan->c2h_dma_desc,
						channel->write_page_addr,
						channel->write_host_page_addr,
						dma_req_data.xfer_size,
						chan_response_dma_completed,
						NULL,
						&dma_req_data,
						sizeof(dma_req_data));
		if (unlikely(ret < 0)) {
			sph_log_err(SERVICE_LOG, "Failed to schedule DMA transfer\n");
			/* end the write loop and return */
			break;
		}

		n_written += write_size;

		/* mark that we used the local and host write pages */
		channel->write_page_vptr = NULL;
		channel->write_host_page_valid = 0;

	} while (n_written < size);

	mutex_unlock(&channel->write_lock);

	if (n_written == 0)
		return ret;

	return n_written;
}

static unsigned int sphcs_genmsg_chan_poll(struct file              *f,
					   struct poll_table_struct *pt)
{
	struct channel_data *channel = (struct channel_data *)f->private_data;
	unsigned int mask = (POLLOUT | POLLWRNORM);

	if (!is_channel_file(f))
		return -EINVAL;

	if (channel) {
		/* check for ready to read */
		if (channel->current_read_packet != NULL) {
			mask |= (POLLIN | POLLRDNORM);
		} else {
			poll_wait(f, &channel->read_waitq, pt);
			NNP_SPIN_LOCK(&channel->read_lock);
			if (!list_empty(&channel->pending_read_packets))
				mask |= (POLLIN | POLLRDNORM);
			NNP_SPIN_UNLOCK(&channel->read_lock);
		}
	}

	return mask;
}

static long write_response_wait(struct file *f, void __user *arg)
{
	struct channel_data *channel = (struct channel_data *)f->private_data;
	int ret = 0;

	mutex_lock(&channel->write_lock);

	/* need local free dma page */
	if (!channel->write_page_vptr) {

		mutex_unlock(&channel->write_lock);
		ret = dma_page_pool_get_free_page(g_the_sphcs->dma_page_pool,
				&channel->write_page_hndl,
				&channel->write_page_vptr,
				&channel->write_page_addr);
		mutex_lock(&channel->write_lock);
	}

	/* need host response page */
	if (!channel->write_host_page_valid) {
		struct sphcs_host_rb *resp_data_rb = &channel->cmd_chan->c2h_rb[0];
		uint32_t chunk_size;
		int n;

		mutex_unlock(&channel->write_lock);
		n = host_rb_wait_free_space(resp_data_rb,
					    NNP_PAGE_SIZE,
					    1,
					    &channel->write_host_page_addr,
					    &chunk_size);
		if (n != 1 || chunk_size != NNP_PAGE_SIZE) {
			sph_log_err(SERVICE_LOG, "Failed to get host response page for write n=%d chunk_size=%u\n", n, chunk_size);
			ret = -1;
		} else {
			host_rb_update_free_space(resp_data_rb, NNP_PAGE_SIZE);
			ret = 0;
		}
		mutex_lock(&channel->write_lock);
		if (!ret)
			channel->write_host_page_valid = 1;
	}

	mutex_unlock(&channel->write_lock);

	return ret;
}

static long chan_is_privileged(struct file *f, void __user *arg)
{
	struct channel_data *channel = (struct channel_data *)f->private_data;
	int is_privileged = 0;
	int rc;

	if (!is_channel_file(f))
		return -EINVAL;

	if (channel->is_privileged)
		is_privileged = 1;

	rc = copy_to_user(arg, &is_privileged, sizeof(int));
	if (rc)
		return -EIO;

	return 0;
}

static long sphcs_genmsg_chan_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!is_channel_file(f))
		return -EINVAL;

	switch (cmd) {
	case IOCTL_GENMSG_WRITE_RESPONSE_WAIT:
		ret = write_response_wait(f, (void __user *)arg);
		break;
	case IOCTL_GENMSG_IS_PRIVILEGED:
		ret = chan_is_privileged(f, (void __user *)arg);
		break;
	default:
		sph_log_err(SERVICE_LOG, "Unsupported genmsg chan IOCTL 0x%x\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations sphcs_genmsg_chan_fops = {
	.owner = THIS_MODULE,
	.release = sphcs_genmsg_chan_release,
	.read = sphcs_genmsg_chan_read,
	.write = sphcs_genmsg_chan_write,
	.unlocked_ioctl = sphcs_genmsg_chan_ioctl,
	.compat_ioctl = sphcs_genmsg_chan_ioctl,
	.poll = sphcs_genmsg_chan_poll
};

static inline int is_channel_file(struct file *f)
{
	return f->f_op == &sphcs_genmsg_chan_fops;
}
static inline int is_channel_ptr(struct channel_data *channel)
{
	return (channel != NULL) && is_channel_file(channel->file);
}

/***************************************************************************
 * Global service list handling routines
 ***************************************************************************/
static int init_service_list(void)
{
	s_service_list = kzalloc(sizeof(*s_service_list), GFP_KERNEL);
	if (!s_service_list)
		return -ENOMEM;

	ida_init(&s_service_list->ida);
	mutex_init(&s_service_list->lock);

	s_service_list->num_services = 0;

	return 0;
}

static void release_service_list(void)
{
	if (s_service_list) {
		unsigned int i;

		for (i = 0; i < SPH_MAX_GENERIC_SERVICES; i++) {
			kfree(s_service_list->service_name[i]);
		}

		ida_destroy(&s_service_list->ida);
		mutex_destroy(&s_service_list->lock);
		kfree(s_service_list);
	}
}

static int add_service(const char *service_name, size_t service_name_len, struct service_data *service)
{
	int service_id;
	unsigned int i;

	service_id = ida_simple_get(&s_service_list->ida, 0, SPH_MAX_GENERIC_SERVICES, GFP_KERNEL);
	if (unlikely(service_id < 0)) {
		sph_log_err(SERVICE_LOG, "Failed to generate service id\n");
		return service_id;
	}

	mutex_lock(&s_service_list->lock);
	//if service with same name already exists, return error
	for (i = 0; i < SPH_MAX_GENERIC_SERVICES; i++) {
		if (s_service_list->service_name[i] != NULL &&
		    !memcmp(s_service_list->service_name[i], service_name, service_name_len + 1)) {
			NNP_ASSERT(s_service_list->service_data[i] != NULL);
			mutex_unlock(&s_service_list->lock);
			ida_simple_remove(&s_service_list->ida, service_id);
			return -EEXIST;
		}
	}
	s_service_list->service_name[service_id] = service_name;
	s_service_list->service_name_len[service_id] = service_name_len;
	s_service_list->service_data[service_id] = service;
	s_service_list->num_services++;
	mutex_unlock(&s_service_list->lock);

	return service_id;
}

static void delete_service(int service_id)
{
	struct pending_packet *p, *n;
	union c2h_ChanGenericMessaging msg;
	struct service_data *service_data;
	int rc;

	mutex_lock(&s_service_list->lock);
	if (s_service_list->service_name[service_id]) {
		service_data = s_service_list->service_data[service_id];

		/* Handle all the pending connections requests, if the service released.
		 * Needed for cases such as counterd crash before accepting connections.
		 */
		if (!list_empty(&service_data->pending_connections)) {
			msg.value = 0;
			msg.opcode = NNP_IPC_C2H_OP_CHAN_GENERIC_MSG_PACKET;
			msg.rb_id = 0;
			msg.connect = 1;
			msg.card_client_id = NNP_IPC_GENMSG_BAD_CLIENT_ID;

			list_for_each_entry_safe(p, n, &service_data->pending_connections, node) {
				/* Send connect reply message back to host */
				msg.chan_id = p->dma_data.cmd_chan->protocol_id;
				sphcs_msg_scheduler_queue_add_msg(p->dma_data.cmd_chan->respq, &msg.value, 1);

				sphcs_cmd_chan_put(p->dma_data.cmd_chan);

				rc = dma_page_pool_set_page_free(g_the_sphcs->dma_page_pool, p->dma_data.dma_page_hndl);
				if (rc)
					sph_log_err(SERVICE_LOG, "Delete service: failed to return dma page back to pool. rc=%d", rc);

				list_del(&p->node);
				kfree(p);
			}
		}
		service_data = NULL;
		kfree(s_service_list->service_name[service_id]);
		s_service_list->service_name[service_id] = NULL;
		s_service_list->service_name_len[service_id] = 0;

		s_service_list->num_services--;
	}
	mutex_unlock(&s_service_list->lock);

	ida_simple_remove(&s_service_list->ida, service_id);
}

static struct service_data *find_service(const char *service_name, size_t name_len)
{
	struct service_data *ret = NULL;
	unsigned int i;

	mutex_lock(&s_service_list->lock);
	for (i = 0; i < SPH_MAX_GENERIC_SERVICES; i++) {
		if (s_service_list->service_name[i] != NULL &&
		    s_service_list->service_name_len[i] == name_len &&
		    !memcmp(s_service_list->service_name[i], service_name, name_len)) {
			NNP_ASSERT(s_service_list->service_data[i] != NULL);
			ret = s_service_list->service_data[i];
			break;
		}
	}
	mutex_unlock(&s_service_list->lock);

	return ret;
}

static int build_service_list_packet(void *buf, unsigned int bufsize, u32 *out_service_count)
{
	int ret = 0;

	mutex_lock(&s_service_list->lock);

	{
		char *name_ptr = (char *)buf;
		unsigned int   i, n = 0;
		unsigned int   needed_size = 0;

		/* First, calculate how much space we need in the buffer */
		for (i = 0; i < SPH_MAX_GENERIC_SERVICES; i++) {
			if (s_service_list->service_name[i])
				needed_size += (s_service_list->service_name_len[i] + 1);
		}

		if (bufsize >= needed_size) {
			for (i = 0; i < SPH_MAX_GENERIC_SERVICES; i++) {
				if (s_service_list->service_name[i]) {
					memcpy(name_ptr, s_service_list->service_name[i], (s_service_list->service_name_len[i] + 1));
					name_ptr += (s_service_list->service_name_len[i] + 1);
					n++;
				}
			}

			*out_service_count = n;

			/* return the number of bytes filled in the buffer */
			ret = (name_ptr - (char *)buf);
		} else {
			/* buffer is too small - return the negative value of the size needed */
			ret = -needed_size;
		}
	}

	mutex_unlock(&s_service_list->lock);

	return ret;
}

static int send_service_list_dma_completed_chan(struct sphcs *sphcs, void *ctx, const void *user_data, int status, u32 timeUS)
{
	const struct dma_req_user_data *dma_req_user_data = (const struct dma_req_user_data *)user_data;
	union c2h_ChanServiceListMsg msg2;
	struct sphcs_cmd_chan *cmd_chan = dma_req_user_data->cmd_chan;

	msg2.value = 0LL;
	msg2.opcode = NNP_IPC_C2H_OP_CHAN_SERVICE_LIST;
	msg2.chan_id = cmd_chan->protocol_id;
	msg2.rb_id = 0;

	if (status == SPHCS_DMA_STATUS_FAILED) {
		/* dma failed */
		msg2.failure = 1;
	} else {
		/* if it is not an error - it must be done */
		NNP_ASSERT(status == SPHCS_DMA_STATUS_DONE);

		msg2.num_services = dma_req_user_data->param1;
		msg2.size = dma_req_user_data->xfer_size - 1;
	}

	/* send the host services response message to host */
	sphcs_msg_scheduler_queue_add_msg(cmd_chan->respq, &msg2.value, 1);

	/* return the local dma page back to the dma pool */
	dma_page_pool_set_page_free(sphcs->dma_page_pool, dma_req_user_data->dma_page_hndl);

	sphcs_cmd_chan_put(cmd_chan);

	return 0;
}

static int send_service_list_to_host(struct sphcs *sphcs, struct sphcs_cmd_chan *cmd_chan)
{
	dma_addr_t  dma_addr;
	dma_addr_t  host_dma_addr;
	int         ret;
	struct dma_req_user_data dma_req_data;
	union c2h_ChanServiceListMsg msg2;
	u32         fail_code = 0;
	struct sphcs_host_rb *resp_data_rb = &cmd_chan->c2h_rb[0];
	uint32_t chunk_size;
	int n;

	if (s_service_list->num_services < 1) {
		/* send empty service list response message to host */
		msg2.value = 0LL;
		msg2.opcode = NNP_IPC_C2H_OP_CHAN_SERVICE_LIST;
		msg2.chan_id = cmd_chan->protocol_id;
		msg2.rb_id = 0;
		msg2.num_services = 0;

		sphcs_msg_scheduler_queue_add_msg(cmd_chan->respq, &msg2.value, 1);
		sphcs_cmd_chan_put(cmd_chan);
		return 0;
	}

	/* get local DMA page for transfering the list to host */
	ret = dma_page_pool_get_free_page(sphcs->dma_page_pool, &dma_req_data.dma_page_hndl, &dma_req_data.dma_vptr, &dma_addr);
	if (ret) {
		sph_log_err(SERVICE_LOG, "Failed to get free DMA page\n");
		fail_code = 2;
		goto fail;
	}

	/* fill the local page with the service list packet */
	dma_req_data.xfer_size = build_service_list_packet(dma_req_data.dma_vptr,
							   NNP_PAGE_SIZE,
							   &dma_req_data.param1);
	if (dma_req_data.xfer_size < 0) {
		sph_log_err(SERVICE_LOG, "Service list too big\n");
		ret = -ENOSPC;
		fail_code = 3;
		goto fail;
	}

	/* get host response page to be filled with the service list packet */

	n = host_rb_wait_free_space(resp_data_rb,
				    NNP_PAGE_SIZE,
				    1,
				    &host_dma_addr,
				    &chunk_size);
	if (n != 1 || chunk_size != NNP_PAGE_SIZE) {
		sph_log_err(SERVICE_LOG, "Failed to get host response page n=%d chunk_size=%d\n", n, chunk_size);
		fail_code = 2;
		goto fail;
	}
	host_rb_update_free_space(resp_data_rb, NNP_PAGE_SIZE);

	dma_req_data.cmd_chan = cmd_chan;

	/* start the DMA transfer to host */
	sphcs_dma_sched_start_xfer_single(sphcs->dmaSched,
					  &cmd_chan->c2h_dma_desc,
					  dma_addr,
					  host_dma_addr,
					  dma_req_data.xfer_size,
					  send_service_list_dma_completed_chan,
					  NULL,
					  &dma_req_data,
					  sizeof(dma_req_data));

	return 0;

fail:
	/* send failed services response message to host */
	msg2.value = 0LL;
	msg2.opcode = NNP_IPC_C2H_OP_CHAN_SERVICE_LIST;
	msg2.chan_id = cmd_chan->protocol_id;
	msg2.rb_id = 0;
	msg2.failure = fail_code;

	sphcs_msg_scheduler_queue_add_msg(cmd_chan->respq, &msg2.value, 1);
	sphcs_cmd_chan_put(cmd_chan);

	return ret;
}

/*****************************************************************************
 * service file ops operations
 *****************************************************************************/
static inline int is_service_file(struct file *f);

static int sphcs_genmsg_open(struct inode *inode, struct file *f)
{
	if (!is_service_file(f))
		return -EINVAL;

	return 0;
}

static int sphcs_genmsg_release(struct inode *inode, struct file *f)
{
	struct service_data *service = (struct service_data *)f->private_data;

	if (!is_service_file(f))
		return -EINVAL;

	sph_log_debug(SERVICE_LOG, "Closing genmsg client\n");

	if (service) {
		delete_service(service->id);
		kfree(service);
	}

	return 0;
}

static long process_register_service(struct file *f, void __user *arg)
{
	struct ioctl_register_service req;
	int ret;
	char *service_name;
	struct service_data *service;
	size_t size;

	/* Fail of the command streamer object has not been created yet */
	if (unlikely(g_the_sphcs == NULL))
		return -ENODEV;

	ret = copy_from_user(&req, arg, sizeof(req));
	if (unlikely(ret != 0))
		return -EFAULT;

	if (unlikely(req.name_len == 0 || req.name_len > SPH_MAX_GENERIC_SERVICES))
		return -EINVAL;

	/* Avoid integer ovf using size_t */
	size = req.name_len;
	size = size + 1;
	service_name = kmalloc(size, GFP_KERNEL);
	if (unlikely(service_name == NULL))
		return -ENOMEM;

	ret = copy_from_user(service_name, arg + sizeof(req), req.name_len);
	if (unlikely(ret != 0)) {
		kfree(service_name);
		return -EIO;
	}

	service_name[req.name_len] = '\0';

	if (unlikely(strlen(service_name) != req.name_len)) {
		kfree(service_name);
		return -EINVAL;
	}

	service = kzalloc(sizeof(*service), GFP_KERNEL);
	if (unlikely(service == NULL)) {
		kfree(service_name);
		return -ENOMEM;
	}

	ret = add_service(service_name, req.name_len, service);
	if (unlikely(ret < 0)) {
		kfree(service);
		kfree(service_name);
		return ret;
	}

	service->id = ret;
	spin_lock_init(&service->lock);
	INIT_LIST_HEAD(&service->pending_connections);
	init_waitqueue_head(&service->waitq);

	f->private_data = service;

	return 0;
}

static long process_accept_client(struct file *f, void __user *arg)
{
	struct service_data *service = f->private_data;
	struct sphcs *sphcs = g_the_sphcs;
	struct pending_packet *pend;
	struct channel_data *channel;
	union c2h_ChanGenericMessaging msg2;
	int ret, rc;
	struct fd sfd;

	if (!service)
		return -EBADF;

	/* wait for a pending connection */
	ret = wait_event_interruptible(service->waitq, !list_empty(&service->pending_connections));
	if (ret)
		return ret;

	NNP_SPIN_LOCK(&service->lock);
	pend = list_first_entry(&service->pending_connections, struct pending_packet, node);
	list_del(&pend->node);
	NNP_SPIN_UNLOCK(&service->lock);

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel) {
		sph_log_err(SERVICE_LOG, "Failed to allocate space for channel\n");
		ret = -ENOMEM;
		goto err_done;
	}

	ret = ida_simple_get(&s_genmsg.channel_ida,
			     1,    /* 0==uninitialized */
			     NNP_IPC_GENMSG_BAD_CLIENT_ID-1,
			     GFP_KERNEL);
	if (ret < 0) {
		sph_log_err(SERVICE_LOG, "Failed to generate channel id\n");
		goto err_done;
	}

	channel->channel_id = ret;

	NNP_SPIN_LOCK(&s_genmsg.lock);
	hash_add(s_genmsg.channel_hash,
		 &channel->hash_node,
		 channel->channel_id);
	NNP_SPIN_UNLOCK(&s_genmsg.lock);

	channel->host_client_id = pend->dma_data.msg.host_client_id;
	channel->is_privileged = pend->dma_data.msg.privileged;
	channel->cmd_chan = pend->dma_data.cmd_chan;

	channel->fd = anon_inode_getfd("sphchan",
				       &sphcs_genmsg_chan_fops,
				       channel,
				       O_RDWR | O_CLOEXEC);
	if (channel->fd < 0) {
		sph_log_err(SERVICE_LOG, "Failed to create channel file descriptor\n");
		ret = channel->fd;
		goto free_id;
	}

	/* Reference count initialized to '1' for inode private_data
	 * reference created by anon_inode_getfd() call
	 */
	kref_init(&channel->ref);

	sfd = fdget(channel->fd);
	channel->file = sfd.file;
	fdput(sfd);

	INIT_LIST_HEAD(&channel->pending_read_packets);
	spin_lock_init(&channel->read_lock);
	init_waitqueue_head(&channel->read_waitq);
	init_waitqueue_head(&channel->write_waitq);
	mutex_init(&channel->write_lock);
	atomic_set(&channel->n_write_dma_req, 0);

	ret = copy_to_user(arg, &channel->fd, sizeof(int));
	if (ret) {
		sph_log_err(SERVICE_LOG, "Failed to copy fd back to user\n");
		goto free_all;
	}

	goto done;

free_all:
	mutex_destroy(&channel->write_lock);
free_id:
	NNP_SPIN_LOCK(&s_genmsg.lock);
	hash_del(&channel->hash_node);
	NNP_SPIN_UNLOCK(&s_genmsg.lock);
	ida_simple_remove(&s_genmsg.channel_ida, channel->channel_id);
err_done:
	kfree(channel);
	channel = NULL;
done:
	if (channel != NULL) {
		pend->dma_data.cmd_chan->destroy_cb = sphcs_chan_genmsg_hangup;
		pend->dma_data.cmd_chan->destroy_cb_ctx = (void *)(uintptr_t)channel->channel_id;
	}
	/* send connect reply message back to host */
	msg2.value = 0;
	msg2.opcode = NNP_IPC_C2H_OP_CHAN_GENERIC_MSG_PACKET;
	msg2.chan_id = pend->dma_data.cmd_chan->protocol_id;
	msg2.rb_id = 0;
	msg2.connect = 1;
	msg2.card_client_id = channel ? channel->channel_id :
		NNP_IPC_GENMSG_BAD_CLIENT_ID;

	sphcs_msg_scheduler_queue_add_msg(pend->dma_data.cmd_chan->respq, &msg2.value, 1);
	if (!channel)
		sphcs_cmd_chan_put(pend->dma_data.cmd_chan);

	rc = dma_page_pool_set_page_free(sphcs->dma_page_pool, pend->dma_data.dma_page_hndl);
	if (rc)
		sph_log_err(SERVICE_LOG, "Failed to return dma page back to pool\n");

	kfree(pend);

	return ret;
}

static long sphcs_genmsg_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	long     ret = 0;

	if (!is_service_file(f))
		return -EINVAL;

	switch (cmd) {
	case IOCTL_GENMSG_REGISTER_SERVICE:
		ret = process_register_service(f, (void __user *)arg);
		break;

	case IOCTL_GENMSG_ACCEPT_CLIENT:
		ret = process_accept_client(f, (void __user *)arg);
		break;

	default:
		sph_log_err(SERVICE_LOG, "Unsupported genmsg IOCTL 0x%x\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

static unsigned int sphcs_genmsg_poll(struct file *f, struct poll_table_struct *pt)
{
	struct service_data *service = f->private_data;
	unsigned int mask = 0;

	if (!is_service_file(f))
		return 0;

	if (service) {
		poll_wait(f, &service->waitq, pt);

		NNP_SPIN_LOCK(&service->lock);
		if (!list_empty(&service->pending_connections))
			mask |= POLLIN;
		NNP_SPIN_UNLOCK(&service->lock);
	}

	return mask;
}

static const struct file_operations sphcs_genmsg_fops = {
	.owner = THIS_MODULE,
	.open = sphcs_genmsg_open,
	.release = sphcs_genmsg_release,
	.unlocked_ioctl = sphcs_genmsg_ioctl,
	.compat_ioctl = sphcs_genmsg_ioctl,
	.poll = sphcs_genmsg_poll
};

static inline int is_service_file(struct file *f)
{
	return f->f_op == &sphcs_genmsg_fops;
}

/*****************************************************************************
 * DMA completion callbacks
 *****************************************************************************/

/*
 * called when a C2H dma transfer of the generic packet is completed
 */
static int chan_response_dma_completed(struct sphcs *sphcs,
				       void *ctx,
				       const void *user_data,
				       int status,
				       u32 timeUS)
{
	const struct dma_req_user_data *dma_req_user_data = (const struct dma_req_user_data *)user_data;

	if (status == SPHCS_DMA_STATUS_FAILED) {
		/* dma failed */
		/*
		 * mark io_error on channel - next read/write will fail
		 * and app will close the channel
		 */
		dma_req_user_data->channel->io_error = true;
	} else {
		union c2h_ChanGenericMessaging msg2;

		/* if it is not an error - it must be done */
		NNP_ASSERT(status == SPHCS_DMA_STATUS_DONE);

		/* send the packet to host */
		msg2.value = 0LL;
		msg2.opcode = NNP_IPC_C2H_OP_CHAN_GENERIC_MSG_PACKET;
		msg2.chan_id = dma_req_user_data->channel->cmd_chan->protocol_id;
		msg2.rb_id = 0;
		msg2.size = dma_req_user_data->xfer_size - 1;
		msg2.card_client_id = dma_req_user_data->channel->channel_id;

		sphcs_msg_scheduler_queue_add_msg(dma_req_user_data->channel->cmd_chan->respq,
						  &msg2.value, 1);
	}

	/* return the local dma page back to the dma pool */
	dma_page_pool_set_page_free(sphcs->dma_page_pool, dma_req_user_data->dma_page_hndl);

	/* Decrement pending write dma requests - wake threads waiting for it */
	atomic_dec_if_positive(&dma_req_user_data->channel->n_write_dma_req);
	wake_up_all(&dma_req_user_data->channel->write_waitq);

	return 0;
}

static void handle_cmd_dma_failed(struct genmsg_dma_command_data *dma_data)
{
	if (dma_data->msg.connect) {
		union c2h_ChanGenericMessaging msg;

		msg.value = 0;
		msg.opcode = NNP_IPC_C2H_OP_CHAN_GENERIC_MSG_PACKET;
		msg.chan_id = dma_data->cmd_chan->protocol_id;
		msg.rb_id = 0;
		msg.connect = 1;
		msg.no_such_service = 1;
		msg.card_client_id = NNP_IPC_GENMSG_BAD_CLIENT_ID;

		sphcs_cmd_chan_update_cmd_head(dma_data->cmd_chan, 0, NNP_PAGE_SIZE);
		sphcs_msg_scheduler_queue_add_msg(dma_data->cmd_chan->respq, (u64 *)&msg.value, 1);
	} else if (dma_data->channel) {
		sphcs_cmd_chan_update_cmd_head(dma_data->cmd_chan, 0, NNP_PAGE_SIZE);
		/*
		 * mark io_error on channel - next read/write will fail
		 * and app will close the channel
		 */
		NNP_SPIN_LOCK(&dma_data->channel->read_lock);
		if (_chnl_open == dma_data->channel->closing)
			dma_data->channel->io_error = true;
		dma_data->channel->n_read_dma_req--;
		NNP_SPIN_UNLOCK(&dma_data->channel->read_lock);
		if (dma_data->channel->n_read_dma_req == 0)
			wake_up_all(&dma_data->channel->read_waitq);

	}

	/* return back dma page to pool */
	dma_page_pool_set_page_free(g_the_sphcs->dma_page_pool, dma_data->dma_page_hndl);

	sphcs_cmd_chan_put(dma_data->cmd_chan);
}

/*
 * called when a H2C dma transfer of the generic packet is completed
 */
int sphcs_genmsg_cmd_dma_complete_callback(struct sphcs *sphcs, void *ctx, const void *user_data, int status, u32 xferTimeUS)
{
	struct genmsg_dma_command_data *dma_data = (struct genmsg_dma_command_data *)user_data;

	if (status == SPHCS_DMA_STATUS_FAILED) {
		sph_log_err(SERVICE_LOG, "Dma error\n");
		/* dma failed */
		handle_cmd_dma_failed(dma_data);

		return -1;
	}

	/* if it is not an error - it must be done */
	NNP_ASSERT(status == SPHCS_DMA_STATUS_DONE);

	if (dma_data->msg.connect) {
		/* This is a connect request - route to a service */
		struct service_data *service;
		u32 *name_len = (u32 *)(dma_data->vptr);
		const char *name = (const char *)(name_len+1);

		service = find_service(name, *name_len);

		if (service != NULL) {
			struct pending_packet *pend;

			pend = kzalloc(sizeof(*pend), GFP_NOWAIT);
			if (unlikely(pend == NULL)) {
				sph_log_err(SERVICE_LOG, "Failed to allocate pending connection struct!!\n");
				handle_cmd_dma_failed(dma_data);
				return -1;
			}
			/* insert into the connection pending list */
			memcpy(&pend->dma_data, dma_data, sizeof(*dma_data));
			NNP_SPIN_LOCK(&service->lock);
			list_add_tail(&pend->node, &service->pending_connections);
			NNP_SPIN_UNLOCK(&service->lock);
			wake_up_all(&service->waitq);
			/*keep cmd_chan ref for the new connection*/
		} else {
			/* service not found -
			 * send connect failed reply message back to host
			 */
			handle_cmd_dma_failed(dma_data);
			return -1;
		}
	} else {
		/* this is a generic packet - route to a channel */
		struct pending_packet *pend;
		struct channel_data *channel;

		channel = dma_data->channel;
		NNP_ASSERT(is_channel_ptr(channel));

		pend = kzalloc(sizeof(*pend), GFP_NOWAIT);
		if (unlikely(pend == NULL)) {
			sph_log_err(SERVICE_LOG, "Failed to allocate pending packet struct!!\n");
			handle_cmd_dma_failed(dma_data);
			return -1;
		}

		INIT_LIST_HEAD(&pend->node);
		memcpy(&pend->dma_data, dma_data, sizeof(*dma_data));

		NNP_SPIN_LOCK(&channel->read_lock);
		if (channel->closing != _chnl_open) {
			NNP_SPIN_UNLOCK(&channel->read_lock);
			kfree(pend);
			sph_log_err(SERVICE_LOG, "Got generic message with closing channel handle!!!\n");
			handle_cmd_dma_failed(dma_data);
			return -1;
		}
		/* insert into the channel's pending read packets */
		list_add_tail(&pend->node, &channel->pending_read_packets);
		if (channel->n_read_dma_req)
			channel->n_read_dma_req--;
		NNP_SPIN_UNLOCK(&channel->read_lock);
		wake_up_all(&channel->read_waitq);
		/*
		 * send a reply to host to free the xmited dma page
		 * in case of connect command, it will happen on the connect response
		 * after the service client will reply to the connect request
		 */
		sphcs_cmd_chan_update_cmd_head(dma_data->cmd_chan, 0, NNP_PAGE_SIZE);
		/*
		 * cmd_chan pointer already in the channel struct, no
		 * need to keep it for the pending packet
		 */
		sphcs_cmd_chan_put(dma_data->cmd_chan);
	}

	return 0;
}

/*
 * process_genmsg_command is called to process a
 * NNP_IPC_H2C_OP_GENERIC_MSG_PACKET message receviced from host.
 */
int process_genmsg_command(struct sphcs *sphcs,
			   union h2c_GenericMessaging *req,
			   struct sphcs_cmd_chan      *cmd_chan)
{
	struct genmsg_dma_command_data dma_data;
	dma_addr_t dma_addr;
	int ret = 0;

	if (!req->hangup && !req->service_list_req && !req->host_pfn) {
		/* This is a protocol error - should not happen!!! */
		sph_log_err(SERVICE_LOG, "Got generic message packet from host connect=%d with NULL host pfn\n", req->connect);
		sphcs_cmd_chan_put(cmd_chan);
		return -1;
	}

	if (req->service_list_req) {
		/* send host updated service list */
		send_service_list_to_host(g_the_sphcs, cmd_chan);
		return 0;
	}

	if (!req->connect) {
		struct channel_data *channel;

		channel = find_channel(req->card_client_id);
		if (!channel) {
			/* This is a protocol error - should not happen!!! */
			sph_log_err(SERVICE_LOG, "Got packet with no card, card_client_id= %u, host_client_id= %u\n", req->card_client_id, req->host_client_id);
			if (!req->hangup)
				sphcs_cmd_chan_update_cmd_head(cmd_chan, 0, NNP_PAGE_SIZE);
			sphcs_cmd_chan_put(cmd_chan);
			return 0;
		}
		if (channel->cmd_chan != cmd_chan) { // reused card_client_id
			channel_put(channel);
			sphcs_cmd_chan_put(cmd_chan);
			return 0;
		}
		dma_data.channel = channel;

		/* handle hangup packet */
		if (req->hangup) {
			struct pending_packet *pend;

			/* channel is not yet closing add empty read packet */
			/* but only after all pending read dma requests are done*/
			wait_event_interruptible(channel->read_waitq,
						 channel->n_read_dma_req == 0);
			/*
			 * cmd_chan pointer already in the channel struct, no
			 * need to keep it in the pending packet
			 */
			sphcs_cmd_chan_put(cmd_chan);
			/* Do not process two hangup messages - may happen when cmd_chan is destroyed after hangup */
			if (channel->hanging_up) {
				channel_put(channel);
				return 0;
			}

			pend = kzalloc(sizeof(*pend), GFP_NOWAIT);
			/*Don't send no mem error message here, maybe pend is not needed*/
			if (pend != NULL) {
				/* insert into the channel's pending read packets */
				memcpy(pend->dma_data.msg.value,
				       req->value,
				       sizeof(req->value));
				pend->is_hangup_command = 1;
			}

			NNP_SPIN_LOCK(&channel->read_lock);

			if (channel->n_read_dma_req > 0) {
				sph_log_err(SERVICE_LOG, "Critical! Should never happen. Received message after drain while hanging_up\n");
				NNP_ASSERT(channel->n_read_dma_req == 0);
			}

			/* check again hang up with the lock */
			if (channel->hanging_up) {
				NNP_SPIN_UNLOCK(&channel->read_lock);
				channel_put(channel);
				kfree(pend);
				return 0;
			}

			channel->hanging_up = 1;
			channel->cmd_chan->destroy_cb = NULL;

			if (channel->closing == _chnl_open) {
				/* channel is not yet closing add empty read packet */
				/* but only after all pending read dma requests are done*/
				if (pend) {
					pend->dma_data.cmd_chan = cmd_chan;
					list_add_tail(&pend->node, &channel->pending_read_packets);
					NNP_SPIN_UNLOCK(&channel->read_lock);
					wake_up_all(&channel->read_waitq);
				} else {
					NNP_SPIN_UNLOCK(&channel->read_lock);
					sph_log_err(SERVICE_LOG, "Failed to allocate pending packet struct!!\n");
					ret = -1;
				}
			} else {
				if (channel->closing == _chnl_preclose) {
					NNP_SPIN_UNLOCK(&channel->read_lock);
				} else if (channel->closing == _chnl_close_ready) {
					channel->closing = _chnl_close_done;
					NNP_SPIN_UNLOCK(&channel->read_lock);
					channel_put(channel);
				} else {
					NNP_SPIN_UNLOCK(&channel->read_lock);
				}

				kfree(pend);
			}

			/* Remove ref from find_channel() */
			channel_put(channel);
			return ret;
		}
		/* Remove ref from find_channel() */
		channel_put(channel);
	} else {
		dma_data.channel = NULL;
		if (req->hangup) {
			sph_log_err(CREATE_COMMAND_LOG, "Protocol error: connect and hangup bits must not be set together.\n");
			sphcs_cmd_chan_put(cmd_chan);
			return -1;
		}
	}

	NNP_ASSERT(!req->hangup);

	ret = dma_page_pool_get_free_page(sphcs->dma_page_pool,
					  &dma_data.dma_page_hndl,
					  &dma_data.vptr,
					  &dma_addr);
	if (ret) {
		sph_log_err(SERVICE_LOG, "Failed to get free page (err: %d)\n", ret);
		sphcs_cmd_chan_update_cmd_head(cmd_chan, 0, NNP_PAGE_SIZE);
		sphcs_cmd_chan_put(cmd_chan);
		return ret;
	}

	memcpy(dma_data.msg.value,
	       req->value,
	       sizeof(req->value));
	dma_data.host_dma_addr = NNP_IPC_DMA_PFN_TO_ADDR(req->host_pfn);
	dma_data.dma_addr = dma_addr;
	dma_data.cmd_chan = cmd_chan;

	if (dma_data.channel != NULL) {
		NNP_SPIN_LOCK(&dma_data.channel->read_lock);
		dma_data.channel->n_read_dma_req++;
		if (dma_data.channel->hanging_up) {
			NNP_SPIN_UNLOCK(&dma_data.channel->read_lock);
			sph_log_err(SERVICE_LOG, "Critical! Should never happen. Received message after hanging_up\n");
			handle_cmd_dma_failed(&dma_data);
			return -1;
		}
		NNP_SPIN_UNLOCK(&dma_data.channel->read_lock);
	}

	/* start DMA xfer to bring the packet */
	ret = sphcs_dma_sched_start_xfer_single(sphcs->dmaSched,
						&cmd_chan->h2c_dma_desc,
						dma_data.host_dma_addr,
						dma_addr,
						req->size + 1,
						sphcs_genmsg_cmd_dma_complete_callback, NULL,
						&dma_data,
						sizeof(dma_data));
	if (ret) {
		sph_log_err(SERVICE_LOG, "Failed to start DMA xfer!\n");
		handle_cmd_dma_failed(&dma_data);
	}

	return 0;
}

/*
 * Interface from new "channel" based protocol
 */
struct chan_genmsg_command_entry {
	struct work_struct             work;
	struct sphcs_cmd_chan         *chan;
	union h2c_ChanGenericMessaging msg;
};

static void chan_genmsg_command_handler(struct work_struct *work)
{
	struct chan_genmsg_command_entry *op = container_of(work,
							    struct chan_genmsg_command_entry,
							    work);
	union h2c_GenericMessaging old_msg;
	struct sphcs_host_rb *cmd_data_rb = &op->chan->h2c_rb[op->msg.rb_id];
	dma_addr_t host_dma_addr;
	u32 host_chunk_size;
	int n;

	/* ignore message if ringbuffer is not set with minimal size */
	if ((!op->msg.service_list_req && !op->msg.hangup &&
	     op->chan->h2c_rb[op->msg.rb_id].size < NNP_PAGE_SIZE) ||
	    op->chan->c2h_rb[op->msg.rb_id].size < NNP_PAGE_SIZE) {
		sph_log_err(GENERAL_LOG, "ringbuf size error rb_id=%d h2c size %d c2h size %d\n",
			    op->msg.rb_id, op->chan->h2c_rb[op->msg.rb_id].size, op->chan->c2h_rb[op->msg.rb_id].size);
		sphcs_cmd_chan_put(op->chan);
		goto done;
	}

	old_msg.opcode = op->msg.opcode;
	old_msg.size = op->msg.size;
	old_msg.connect = op->msg.connect;
	old_msg.hangup = op->msg.hangup;
	old_msg.host_client_id = op->msg.chan_id;
	old_msg.card_client_id = op->msg.card_client_id;
	old_msg.service_list_req = op->msg.service_list_req;
	old_msg.privileged = op->chan->privileged;

	old_msg.host_pfn = 0;
	old_msg.host_page_hndl = 0;

	if (!op->msg.hangup && !op->msg.service_list_req) {
		/* need to advance h2c ring buffer by one page */
		host_rb_update_free_space(cmd_data_rb, NNP_PAGE_SIZE);
		n = host_rb_get_avail_space(cmd_data_rb,
					    NNP_PAGE_SIZE,
					    1,
					    &host_dma_addr,
					    &host_chunk_size);

		NNP_ASSERT(n == 1);
		NNP_ASSERT((host_dma_addr & NNP_IPC_DMA_ADDR_ALIGN_MASK) == 0);

		old_msg.host_pfn = NNP_IPC_DMA_ADDR_TO_PFN(host_dma_addr);
		old_msg.host_page_hndl = 0;

		host_rb_update_avail_space(cmd_data_rb, NNP_PAGE_SIZE);
	}

	/* call to process command */
	process_genmsg_command(g_the_sphcs, &old_msg, op->chan);

done:
	kfree(op);
}

static void sphcs_chan_genmsg_hangup(struct sphcs_cmd_chan *cmd_chan, void *cb_ctx)
{
	union h2c_GenericMessaging old_msg;

	memset(old_msg.value, 0, sizeof(old_msg.value));
	old_msg.opcode = NNP_IPC_H2C_OP_CHAN_GENERIC_MSG_PACKET;
	old_msg.hangup = 1;
	old_msg.host_client_id = cmd_chan->protocol_id;
	old_msg.card_client_id = (uint32_t)(uintptr_t)cb_ctx;

	sphcs_cmd_chan_get(cmd_chan);

	process_genmsg_command(g_the_sphcs, &old_msg, cmd_chan);
}

/*
 * called to handle a
 * NNP_IPC_H2C_OP_GENERIC_MSG_PACKET message receviced from host.
 */
void IPC_OPCODE_HANDLER(CHAN_GENERIC_MSG_PACKET)(struct sphcs                   *sphcs,
						 union h2c_ChanGenericMessaging *msg)
{
	struct chan_genmsg_command_entry *entry;
	struct sphcs_cmd_chan *chan;

	chan = sphcs_find_channel(sphcs, msg->chan_id);
	if (!chan) {
		sph_log_err(GENERAL_LOG, "Channel not found chan_id=%d\n", msg->chan_id);
		return;
	}

	/*
	 * place the command in pending list and schedule the pending work to handle it
	 */
	entry = kzalloc(sizeof(*entry), GFP_NOWAIT);
	if (!entry) {
		sph_log_err(SERVICE_LOG, "No memory for pending command entry!!!\n");
		return;
	}

	entry->chan = chan;
	entry->msg.value = msg->value;

	INIT_WORK(&entry->work, chan_genmsg_command_handler);
	queue_work(chan->wq, &entry->work);
}

/*
 * generic messaging sub-module initialization routine.
 * called on kernel module load time.
 */
int sphcs_init_genmsg_interface(void)
{
	int ret;

	ret = alloc_chrdev_region(&s_devnum, 0, 1, SPHCS_GENMSG_DEV_NAME);
	if (ret < 0) {
		sph_log_err(START_UP_LOG, "failed to allocate devnum %d\n", ret);
		return ret;
	}

	cdev_init(&s_cdev, &sphcs_genmsg_fops);
	s_cdev.owner = THIS_MODULE;

	ret = cdev_add(&s_cdev, s_devnum, 1);
	if (ret < 0) {
		sph_log_err(START_UP_LOG, "failed to add cdev %d\n", ret);
		unregister_chrdev_region(s_devnum, 1);
		return ret;
	}

	s_class = class_create(THIS_MODULE, SPHCS_GENMSG_DEV_NAME);
	if (IS_ERR(s_class)) {
		ret = PTR_ERR(s_class);
		sph_log_err(START_UP_LOG, "failed to register class %d\n", ret);
		cdev_del(&s_cdev);
		unregister_chrdev_region(s_devnum, 1);
		return ret;
	}

	s_dev = device_create(s_class, NULL, s_devnum, NULL, SPHCS_GENMSG_DEV_NAME);
	if (IS_ERR(s_dev)) {
		ret = PTR_ERR(s_dev);
		class_destroy(s_class);
		cdev_del(&s_cdev);
		unregister_chrdev_region(s_devnum, 1);
		return ret;
	}

	ret = init_service_list();
	if (ret) {
		device_destroy(s_class, s_devnum);
		class_destroy(s_class);
		cdev_del(&s_cdev);
		unregister_chrdev_region(s_devnum, 1);
		return ret;
	}

	hash_init(s_genmsg.channel_hash);
	ida_init(&s_genmsg.channel_ida);
	spin_lock_init(&s_genmsg.lock);

	sph_log_info(START_UP_LOG, "chardev inited at MAJOR=%d\n", MAJOR(s_devnum));
	return 0;
}

/*
 * generic messaging sub-module cleanup function.
 * called during kernel module unload time.
 */
void sphcs_release_genmsg_interface(void)
{
	ida_destroy(&s_genmsg.channel_ida);
	release_service_list();
	device_destroy(s_class, s_devnum);
	class_destroy(s_class);
	cdev_del(&s_cdev);
	unregister_chrdev_region(s_devnum, 1);
}
