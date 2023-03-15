/*
 *
 * Copyright 2023 Kenichi Yasukata
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <rvif.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include <rte_kvargs.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>

#include <ethdev_vdev.h>

struct pmd_info {
	struct rte_eth_dev *d;

	struct rte_mempool *mb_pool;
	struct rte_ether_addr mac;

	uint64_t rx_queue[RVIF_MAX_QUEUE];
	uint64_t tx_queue[RVIF_MAX_QUEUE];

	struct {
		int fd;
		unsigned long len;
		void *ptr;

		unsigned long mem_off;
	} mem;

	struct {
		_Atomic uint64_t rx_cnt;
		_Atomic uint64_t tx_cnt;
		_Atomic uint64_t rx_byte;
		_Atomic uint64_t tx_byte;
	} stat;
};

static int dev_close(struct rte_eth_dev *dev)
{
	dev->data->mac_addrs = NULL;
	return 0;
}

static int dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = RTE_ETH_LINK_UP;
	return 0;
}

static int dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	return 0;
}

static int dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int dev_infos_get(struct rte_eth_dev *dev __rte_unused,
			 struct rte_eth_dev_info *di)
{
	di->max_mac_addrs = 1;
	di->max_rx_pktlen = 1518;
	di->max_tx_queues = di->max_rx_queues = RVIF_MAX_QUEUE;
	di->min_rx_bufsize = 0;
	return 0;
}

static int rx_queue_setup(struct rte_eth_dev *dev,
			  uint16_t rx_queue_id,
			  uint16_t nb_rx_desc,
			  unsigned int socket_id __rte_unused,
			  const struct rte_eth_rxconf *rx_conf __rte_unused,
			  struct rte_mempool *mb_pool)
{
	struct rvif *vif = ((struct pmd_info *) dev->data->dev_private)->mem.ptr;
	printf("%s: queue[%hu] desc %hu\n", __func__, rx_queue_id, nb_rx_desc);
	if (vif->num <= rx_queue_id)
		vif->num = rx_queue_id + 1;
	vif->queue[rx_queue_id].ring[0].head = vif->queue[rx_queue_id].ring[0].tail = 0;
	vif->queue[rx_queue_id].ring[0].num = nb_rx_desc;
	{
		unsigned short i;
		for (i = 0; i < nb_rx_desc; i++) {
			vif->queue[rx_queue_id].ring[0].slot[i].off = ((struct pmd_info *) dev->data->dev_private)->mem.mem_off;
			((struct pmd_info *) dev->data->dev_private)->mem.mem_off -= 2048;
		}
	}
	((struct pmd_info *) dev->data->dev_private)->mb_pool = mb_pool;
	dev->data->rx_queues[rx_queue_id] = &((struct pmd_info *) dev->data->dev_private)->rx_queue[rx_queue_id];
	return 0;
}

static int tx_queue_setup(struct rte_eth_dev *dev,
			  uint16_t tx_queue_id,
			  uint16_t nb_tx_desc,
			  unsigned int socket_id __rte_unused,
			  const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct rvif *vif = ((struct pmd_info *) dev->data->dev_private)->mem.ptr;
	printf("%s: queue[%hu] desc %hu\n", __func__, tx_queue_id, nb_tx_desc);
	if (vif->num <= tx_queue_id)
		vif->num = tx_queue_id + 1;
	vif->queue[tx_queue_id].ring[1].head = vif->queue[tx_queue_id].ring[1].tail = 0;
	vif->queue[tx_queue_id].ring[1].num = nb_tx_desc;
	{
		unsigned short i;
		for (i = 0; i < nb_tx_desc; i++) {
			vif->queue[tx_queue_id].ring[1].slot[i].off = ((struct pmd_info *) dev->data->dev_private)->mem.mem_off;
			((struct pmd_info *) dev->data->dev_private)->mem.mem_off -= 2048;
		}
	}
	dev->data->tx_queues[tx_queue_id] = &((struct pmd_info *) dev->data->dev_private)->tx_queue[tx_queue_id];
	return 0;
}

static void rx_queue_release(struct rte_eth_dev *dev __rte_unused,
			     uint16_t rx_queue_id __rte_unused)
{
}

static void tx_queue_release(struct rte_eth_dev *dev __rte_unused,
			     uint16_t tx_queue_id __rte_unused)
{
}

static int mtu_set(struct rte_eth_dev *dev __rte_unused,
		   uint16_t mtu)
{
	assert(mtu <= 1518);
	return 0;
}

static int link_update(struct rte_eth_dev *dev __rte_unused,
		       int wait __rte_unused)
{
	return 0;
}

static int mac_address_set(struct rte_eth_dev *dev,
			   struct rte_ether_addr *addr)
{
	memcpy(&((struct pmd_info *) dev->data->dev_private)->mac, addr, sizeof(((struct pmd_info *) dev->data->dev_private)->mac));
	return 0;
}

static int stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *s)
{
	s->ipackets = ((struct pmd_info *) dev->data->dev_private)->stat.rx_cnt;
	s->opackets = ((struct pmd_info *) dev->data->dev_private)->stat.tx_cnt;
	s->ibytes = ((struct pmd_info *) dev->data->dev_private)->stat.rx_byte;
	s->obytes = ((struct pmd_info *) dev->data->dev_private)->stat.tx_byte;
	return 0;
}

static int stats_reset(struct rte_eth_dev *dev)
{
	memset(&((struct pmd_info *) dev->data->dev_private)->stat, 0, sizeof(((struct pmd_info *) dev->data->dev_private)->stat));
	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_close = dev_close,
	.dev_start = dev_start,
	.dev_stop = dev_stop,
	.dev_configure = dev_configure,
	.dev_infos_get = dev_infos_get,
	.rx_queue_setup = rx_queue_setup,
	.tx_queue_setup = tx_queue_setup,
	.rx_queue_release = rx_queue_release,
	.tx_queue_release = tx_queue_release,
	.mtu_set = mtu_set,
	.link_update = link_update,
	.mac_addr_set = mac_address_set,
	.stats_get = stats_get,
	.stats_reset = stats_reset,
};

__attribute__((weak)) void dpdk_rvif_io_hook(void *d, unsigned short q, void *b, uint16_t n,
					     uint16_t c, char is_tx, char is_post)
{
	(void) d;
	(void) q;
	(void) b;
	(void) n;
	(void) c;
	(void) is_tx;
	(void) is_post;
}

__attribute__((weak)) int dpdk_rvif_setup_hook(void *d, char is_exit)
{
	(void) d;
	(void) is_exit;
	return 0;
}

static uint16_t rx_pkt_burst(void *q, struct rte_mbuf **b, uint16_t n)
{
	struct pmd_info *p = container_of(q, struct pmd_info, rx_queue[*((uint64_t *) q)]);
	struct rvif *vif = p->mem.ptr;
	uint16_t rx_cnt = 0;
	uint64_t rx_byte = 0;
	dpdk_rvif_io_hook(p->d, *((uint64_t *) q), b, n, 0, 0, 0);
	{
		uint16_t h, t;
		h = vif->queue[*((uint64_t *) q)].ring[0].head;
		asm volatile ("" ::: "memory");
		t = vif->queue[*((uint64_t *) q)].ring[0].tail;
		{
			uint16_t _t = t;
			while ((rx_cnt < n) && (_t != h)) {
				rx_cnt++;
				rx_byte += vif->queue[*((uint64_t *) q)].ring[0].slot[_t].len;
				if (++_t == vif->queue[*((uint64_t *) q)].ring[0].num) _t = 0;
			}
		}
		assert(!rte_pktmbuf_alloc_bulk(p->mb_pool, b, rx_cnt));
		{
			unsigned short i;
			for (i = 0; i < rx_cnt; i++, t = (t + 1 == vif->queue[*((uint64_t *) q)].ring[0].num ? 0 : t + 1)) {
				rte_memcpy(rte_pktmbuf_mtod(b[i], void *),
						(void *)((unsigned long) vif + vif->queue[*((uint64_t *) q)].ring[0].slot[t].off),
						vif->queue[*((uint64_t *) q)].ring[0].slot[t].len);
				rte_pktmbuf_data_len(b[i]) = vif->queue[*((uint64_t *) q)].ring[0].slot[t].len;
			}
		}
		asm volatile ("" ::: "memory");
		vif->queue[*((uint64_t *) q)].ring[0].tail = t;
	}
	dpdk_rvif_io_hook(p->d, *((uint64_t *) q), b, n, rx_cnt, 0, 1);
	p->stat.rx_cnt += rx_cnt;
	p->stat.rx_byte += rx_byte;
	return rx_cnt;
}

static uint16_t tx_pkt_burst(void *q, struct rte_mbuf **b, uint16_t n)
{
	struct pmd_info *p = container_of(q, struct pmd_info, tx_queue[*((uint64_t *) q)]);
	struct rvif *vif = p->mem.ptr;
	uint16_t tx_cnt = 0;
	uint64_t tx_byte = 0;
	dpdk_rvif_io_hook(p->d, *((uint64_t *) q), b, n, tx_cnt, 1, 0);
	{
		uint16_t h, t;
		h = vif->queue[*((uint64_t *) q)].ring[1].head;
		asm volatile ("" ::: "memory");
		t = vif->queue[*((uint64_t *) q)].ring[1].tail;
		while ((tx_cnt < n) && ((t + 1 == vif->queue[*((uint64_t *) q)].ring[1].num ? 0 : t + 1) != h)) {
			tx_byte += vif->queue[*((uint64_t *) q)].ring[1].slot[t].len = rte_pktmbuf_data_len(b[tx_cnt]);
			rte_memcpy((void *)((unsigned long) vif + vif->queue[*((uint64_t *) q)].ring[1].slot[t].off),
					rte_pktmbuf_mtod(b[tx_cnt], void *),
					vif->queue[*((uint64_t *) q)].ring[1].slot[t].len);
			tx_cnt++;
			if (++t == vif->queue[*((uint64_t *) q)].ring[1].num) t = 0;
		}
		asm volatile ("" ::: "memory");
		vif->queue[*((uint64_t *) q)].ring[1].tail = t;
	}
	dpdk_rvif_io_hook(p->d, *((uint64_t *) q), b, n, tx_cnt, 1, 1);
	rte_pktmbuf_free_bulk(b, n);
	p->stat.tx_cnt += tx_cnt;
	p->stat.tx_byte += tx_byte;
	return tx_cnt;
}

#define ETH_RVIF_MAC_ARG "mac"
#define ETH_RVIF_MEM_ARG "mem"

static int process_arg(const char *key, const char *value, void *extra)
{
	if ((strlen(ETH_RVIF_MAC_ARG) == strlen(key)) && !strcmp(ETH_RVIF_MAC_ARG, key)) {
		assert(sscanf(value, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
					&((struct pmd_info *) extra)->mac.addr_bytes[0],
					&((struct pmd_info *) extra)->mac.addr_bytes[1],
					&((struct pmd_info *) extra)->mac.addr_bytes[2],
					&((struct pmd_info *) extra)->mac.addr_bytes[3],
					&((struct pmd_info *) extra)->mac.addr_bytes[4],
					&((struct pmd_info *) extra)->mac.addr_bytes[5]) == 6);
	} else if ((strlen(ETH_RVIF_MEM_ARG) == strlen(key)) && !strcmp(ETH_RVIF_MEM_ARG, key)) {
		{
			struct stat st;
			assert(!stat(value, &st));
			((struct pmd_info *) extra)->mem.len = st.st_size;
		}
		assert((((struct pmd_info *) extra)->mem.fd = open(value, O_RDWR)) != -1); // we do not close this fd
		assert((((struct pmd_info *) extra)->mem.ptr = mmap(NULL,
						((struct pmd_info *) extra)->mem.len,
						PROT_READ | PROT_WRITE,
						MAP_SHARED | MAP_POPULATE,
						((struct pmd_info *) extra)->mem.fd, 0)) != MAP_FAILED);
		((struct pmd_info *) extra)->mem.mem_off = ((struct pmd_info *) extra)->mem.len - 2048;
	}
	return 0;
}

static int dev_probe(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *d;

	if (dev->device.numa_node == SOCKET_ID_ANY)
		dev->device.numa_node = rte_socket_id();

	assert((d = rte_eth_vdev_allocate(dev, sizeof(struct pmd_info))) != NULL);

	memset(d->data->dev_private, 0, sizeof(struct pmd_info));

	((struct pmd_info *) d->data->dev_private)->d = d;

	{
		int i;
		for (i = 0; i < RVIF_MAX_QUEUE; i++) {
			((struct pmd_info *) d->data->dev_private)->rx_queue[i] = i;
			((struct pmd_info *) d->data->dev_private)->tx_queue[i] = i;
		}
	}

	{
		const char *params = rte_vdev_device_args(dev);
		if (params) {
			const char *valid_arguments[] = {
				ETH_RVIF_MAC_ARG,
				ETH_RVIF_MEM_ARG,
				NULL,
			};
			struct rte_kvargs *kvlist;
			assert((kvlist = rte_kvargs_parse(params, valid_arguments)) != NULL);
			assert(rte_kvargs_count(kvlist, ETH_RVIF_MAC_ARG) == 1);
			assert(rte_kvargs_count(kvlist, ETH_RVIF_MEM_ARG) == 1);
			assert(!rte_kvargs_process(kvlist, NULL, process_arg, d->data->dev_private));
			rte_kvargs_free(kvlist);
		}
	}

	d->dev_ops = &ops;
	d->device = &dev->device;
	d->rx_pkt_burst = rx_pkt_burst;
	d->tx_pkt_burst = tx_pkt_burst;
	d->data->nb_rx_queues = RVIF_MAX_QUEUE;
	d->data->nb_tx_queues = RVIF_MAX_QUEUE;
	d->data->mac_addrs = &((struct pmd_info *) d->data->dev_private)->mac;
	d->data->promiscuous = 1;
	d->data->all_multicast = 1;
	d->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	rte_eth_dev_probing_finish(d);

	assert(!dpdk_rvif_setup_hook(d, 0));

	return 0;
}

static int dev_remove(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *d;

	if ((d = rte_eth_dev_allocated(rte_vdev_device_name(dev))) == NULL)
		return 0;

	assert(!dpdk_rvif_setup_hook(d, 1));

	assert(!munmap(((struct pmd_info *) d->data->dev_private)->mem.ptr, ((struct pmd_info *) d->data->dev_private)->mem.len));
	close(((struct pmd_info *) d->data->dev_private)->mem.fd);

	rte_eth_dev_release_port(d);

	return 0;
}

struct rte_vdev_driver pmd_rvif_drv = {
	.probe = dev_probe,
	.remove = dev_remove,
};

RTE_PMD_REGISTER_VDEV(net_rvif, pmd_rvif_drv);
RTE_PMD_REGISTER_ALIAS(net_rvif, eth_rvif);
RTE_PMD_REGISTER_PARAM_STRING(net_rvif,
		ETH_RVIF_MAC_ARG "=<string> "
		ETH_RVIF_MEM_ARG "=<string> "
		);

RTE_INIT(rvif_init)
{
	/* do some init work */
}
