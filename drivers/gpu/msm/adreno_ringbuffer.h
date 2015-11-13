/* Copyright (c) 2002,2007-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ADRENO_RINGBUFFER_H
#define __ADRENO_RINGBUFFER_H

#include "kgsl_iommu.h"
#include "adreno_dispatch.h"

#define KGSL_RB_SIZE (32 * 1024)

#define KGSL_RB_DWORDS (KGSL_RB_SIZE >> 2)

struct kgsl_device;
struct kgsl_device_private;

union adreno_ttbr0 {
	unsigned int ttbr0_lo;
	unsigned int ttbr0_hi;
};

struct adreno_submit_time {
	uint64_t ticks;
	u64 ktime;
	struct timespec utime;
};

struct adreno_ringbuffer_pagetable_info {
	int current_global_ptname;
	int current_rb_ptname;
	int incoming_ptname;
	int switch_pt_enable;
	union adreno_ttbr0 ttbr0[KGSL_IOMMU_MAX_UNITS];
};

struct adreno_ringbuffer {
	struct kgsl_device *device;
	uint32_t flags;
	struct kgsl_memdesc buffer_desc;
	unsigned int sizedwords;
	unsigned int wptr;
	unsigned int rptr;
	unsigned int last_wptr;
	int id;
	unsigned int fault_detect_ts;
	unsigned int timestamp;
	struct kgsl_event_group events;
	struct kgsl_event_group mmu_events;
	struct adreno_context *drawctxt_active;
	struct kgsl_memdesc pagetable_desc;
	struct kgsl_memdesc pt_update_desc;
	struct adreno_dispatcher_cmdqueue dispatch_q;
	wait_queue_head_t ts_expire_waitq;
};

struct adreno_ringbuffer_mmu_disable_clk_param {
	struct adreno_ringbuffer *rb;
	int unit;
	unsigned int ts;
};

#define GSL_RB_MEMPTRS_SCRATCH_MASK 0x1

#define GSL_RB_PROTECTED_MODE_CONTROL		0x200001F2

#define ADRENO_CURRENT_RINGBUFFER(a)	((a)->cur_rb)

#define KGSL_MEMSTORE_RB_OFFSET(rb, field)	\
	KGSL_MEMSTORE_OFFSET((rb->id + KGSL_MEMSTORE_MAX), field)

int adreno_ringbuffer_issueibcmds(struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_cmdbatch *cmdbatch,
				uint32_t *timestamp);

int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_cmdbatch *cmdbatch,
		struct adreno_submit_time *time);

int adreno_ringbuffer_init(struct kgsl_device *device);

int adreno_ringbuffer_warm_start(struct adreno_device *adreno_dev);

int adreno_ringbuffer_cold_start(struct adreno_device *adreno_dev);

void adreno_ringbuffer_stop(struct adreno_device *adreno_dev);

void adreno_ringbuffer_close(struct adreno_device *adreno_dev);

int adreno_ringbuffer_issuecmds(struct adreno_ringbuffer *rb,
					unsigned int flags,
					unsigned int *cmdaddr,
					int sizedwords);

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time);

void kgsl_cp_intrcallback(struct kgsl_device *device);

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
						unsigned int numcmds);

int adreno_ringbuffer_read_pfp_ucode(struct kgsl_device *device);

int adreno_ringbuffer_read_pm4_ucode(struct kgsl_device *device);

void adreno_ringbuffer_mmu_disable_clk_on_ts(struct kgsl_device *device,
			struct adreno_ringbuffer *rb, unsigned int ts,
			int unit);

int adreno_ringbuffer_waittimestamp(struct adreno_ringbuffer *rb,
					unsigned int timestamp,
					unsigned int msecs);

int adreno_rb_readtimestamp(struct kgsl_device *device,
	void *priv, enum kgsl_timestamp_type type,
	unsigned int *timestamp);

static inline int adreno_ringbuffer_count(struct adreno_ringbuffer *rb,
	unsigned int rptr)
{
	if (rb->wptr >= rptr)
		return rb->wptr - rptr;
	return rb->wptr + KGSL_RB_DWORDS - rptr;
}

static inline unsigned int adreno_ringbuffer_inc_wrapped(unsigned int val,
							unsigned int size)
{
	return (val + sizeof(unsigned int)) % size;
}

static inline unsigned int adreno_ringbuffer_dec_wrapped(unsigned int val,
							unsigned int size)
{
	return (val + size - sizeof(unsigned int)) % size;
}

static inline int adreno_ringbuffer_check_timestamp(
			struct adreno_ringbuffer *rb,
			unsigned int timestamp, int type)
{
	unsigned int ts;
	adreno_rb_readtimestamp(rb->device, rb, type, &ts);
	return (timestamp_cmp(ts, timestamp) >= 0);
}

#endif  
