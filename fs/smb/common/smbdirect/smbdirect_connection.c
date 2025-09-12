// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   Copyright (C) 2017, Microsoft Corporation.
 *   Copyright (c) 2025, Stefan Metzmacher
 */

#include "smbdirect_internal.h"

static void smbdirect_connection_destroy_mem_pools(struct smbdirect_socket *sc);

__maybe_unused /* this is temporary while this file is included in others */
static int smbdirect_connection_create_mem_pools(struct smbdirect_socket *sc)
{
	const struct smbdirect_socket_parameters *sp = &sc->parameters;
	char name[80];
	size_t i;

	/*
	 * We use sizeof(struct smbdirect_negotiate_resp) for the
	 * payload size as it is larger as
	 * sizeof(struct smbdirect_data_transfer).
	 *
	 * This will fit client and server usage for now.
	 */
	snprintf(name, sizeof(name), "smbdirect_send_io_cache_%p", sc);
	struct kmem_cache_args send_io_args = {
		.align		= __alignof__(struct smbdirect_send_io),
	};
	sc->send_io.mem.cache = kmem_cache_create(name,
						  sizeof(struct smbdirect_send_io) +
						  sizeof(struct smbdirect_negotiate_resp),
						  &send_io_args,
						  SLAB_HWCACHE_ALIGN);
	if (!sc->send_io.mem.cache)
		goto err;

	sc->send_io.mem.pool = mempool_create_slab_pool(sp->send_credit_target,
							sc->send_io.mem.cache);
	if (!sc->send_io.mem.pool)
		goto err;

	/*
	 * A payload size of sp->max_recv_size should fit
	 * any message.
	 *
	 * For smbdirect_data_transfer messages the whole
	 * buffer might be exposed to userspace
	 * (currently on the client side...)
	 * The documentation says data_offset = 0 would be
	 * strange but valid.
	 */
	snprintf(name, sizeof(name), "smbdirect_recv_io_cache_%p", sc);
	struct kmem_cache_args recv_io_args = {
		.align		= __alignof__(struct smbdirect_recv_io),
		.useroffset	= sizeof(struct smbdirect_recv_io),
		.usersize	= sp->max_recv_size,
	};
	sc->recv_io.mem.cache = kmem_cache_create(name,
						  sizeof(struct smbdirect_recv_io) +
						  sp->max_recv_size,
						  &recv_io_args,
						  SLAB_HWCACHE_ALIGN);
	if (!sc->recv_io.mem.cache)
		goto err;

	sc->recv_io.mem.pool = mempool_create_slab_pool(sp->recv_credit_max,
							sc->recv_io.mem.cache);
	if (!sc->recv_io.mem.pool)
		goto err;

	for (i = 0; i < sp->recv_credit_max; i++) {
		struct smbdirect_recv_io *recv_io;

		recv_io = mempool_alloc(sc->recv_io.mem.pool,
					sc->recv_io.mem.gfp_mask);
		if (!recv_io)
			goto err;
		recv_io->socket = sc;
		recv_io->sge.length = 0;
		list_add_tail(&recv_io->list, &sc->recv_io.free.list);
	}

	return 0;
err:
	smbdirect_connection_destroy_mem_pools(sc);
	return -ENOMEM;
}

static void smbdirect_connection_destroy_mem_pools(struct smbdirect_socket *sc)
{
	struct smbdirect_recv_io *recv_io, *next_io;

	list_for_each_entry_safe(recv_io, next_io, &sc->recv_io.free.list, list) {
		list_del(&recv_io->list);
		mempool_free(recv_io, sc->recv_io.mem.pool);
	}

	/*
	 * Note mempool_destroy() and kmem_cache_destroy()
	 * work fine with a NULL pointer
	 */

	mempool_destroy(sc->recv_io.mem.pool);
	sc->recv_io.mem.pool = NULL;

	kmem_cache_destroy(sc->recv_io.mem.cache);
	sc->recv_io.mem.cache = NULL;

	mempool_destroy(sc->send_io.mem.pool);
	sc->send_io.mem.pool = NULL;

	kmem_cache_destroy(sc->send_io.mem.cache);
	sc->send_io.mem.cache = NULL;
}

__maybe_unused /* this is temporary while this file is included in others */
static struct smbdirect_send_io *smbdirect_connection_alloc_send_io(struct smbdirect_socket *sc)
{
	struct smbdirect_send_io *msg;

	msg = mempool_alloc(sc->send_io.mem.pool, sc->send_io.mem.gfp_mask);
	if (!msg)
		return ERR_PTR(-ENOMEM);
	msg->socket = sc;
	INIT_LIST_HEAD(&msg->sibling_list);
	msg->num_sge = 0;

	return msg;
}

__maybe_unused /* this is temporary while this file is included in others */
static void smbdirect_connection_free_send_io(struct smbdirect_send_io *msg)
{
	struct smbdirect_socket *sc = msg->socket;
	size_t i;

	/*
	 * The list needs to be empty!
	 * The caller should take care of it.
	 */
	WARN_ON_ONCE(!list_empty(&msg->sibling_list));

	/*
	 * Note we call ib_dma_unmap_page(), even if some sges are mapped using
	 * ib_dma_map_single().
	 *
	 * The difference between _single() and _page() only matters for the
	 * ib_dma_map_*() case.
	 *
	 * For the ib_dma_unmap_*() case it does not matter as both take the
	 * dma_addr_t and dma_unmap_single_attrs() is just an alias to
	 * dma_unmap_page_attrs().
	 */
	for (i = 0; i < msg->num_sge; i++)
		ib_dma_unmap_page(sc->ib.dev,
				  msg->sge[i].addr,
				  msg->sge[i].length,
				  DMA_TO_DEVICE);

	mempool_free(msg, sc->send_io.mem.pool);
}

__maybe_unused /* this is temporary while this file is included in others */
static struct smbdirect_recv_io *smbdirect_connection_get_recv_io(struct smbdirect_socket *sc)
{
	struct smbdirect_recv_io *msg = NULL;
	unsigned long flags;

	spin_lock_irqsave(&sc->recv_io.free.lock, flags);
	if (likely(!sc->first_error))
		msg = list_first_entry_or_null(&sc->recv_io.free.list,
					       struct smbdirect_recv_io,
					       list);
	if (likely(msg)) {
		list_del(&msg->list);
		sc->statistics.get_receive_buffer++;
	}
	spin_unlock_irqrestore(&sc->recv_io.free.lock, flags);

	return msg;
}

__maybe_unused /* this is temporary while this file is included in others */
static void smbdirect_connection_put_recv_io(struct smbdirect_recv_io *msg)
{
	struct smbdirect_socket *sc = msg->socket;
	unsigned long flags;

	if (likely(msg->sge.length != 0)) {
		ib_dma_unmap_single(sc->ib.dev,
				    msg->sge.addr,
				    msg->sge.length,
				    DMA_FROM_DEVICE);
		msg->sge.length = 0;
	}

	spin_lock_irqsave(&sc->recv_io.free.lock, flags);
	list_add_tail(&msg->list, &sc->recv_io.free.list);
	sc->statistics.put_receive_buffer++;
	spin_unlock_irqrestore(&sc->recv_io.free.lock, flags);

	queue_work(sc->workqueue, &sc->recv_io.posted.refill_work);
}

__maybe_unused /* this is temporary while this file is included in others */
static void smbdirect_connection_reassembly_append_recv_io(struct smbdirect_socket *sc,
							   struct smbdirect_recv_io *msg,
							   u32 data_length)
{
	unsigned long flags;

	spin_lock_irqsave(&sc->recv_io.reassembly.lock, flags);
	list_add_tail(&msg->list, &sc->recv_io.reassembly.list);
	sc->recv_io.reassembly.queue_length++;
	/*
	 * Make sure reassembly_data_length is updated after list and
	 * reassembly_queue_length are updated. On the dequeue side
	 * reassembly_data_length is checked without a lock to determine
	 * if reassembly_queue_length and list is up to date
	 */
	virt_wmb();
	sc->recv_io.reassembly.data_length += data_length;
	spin_unlock_irqrestore(&sc->recv_io.reassembly.lock, flags);
	sc->statistics.enqueue_reassembly_queue++;
}

__maybe_unused /* this is temporary while this file is included in others */
static struct smbdirect_recv_io *
smbdirect_connection_reassembly_first_recv_io(struct smbdirect_socket *sc)
{
	struct smbdirect_recv_io *msg;

	msg = list_first_entry_or_null(&sc->recv_io.reassembly.list,
				       struct smbdirect_recv_io,
				       list);

	return msg;
}
