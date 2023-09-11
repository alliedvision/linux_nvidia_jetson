/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/mutex.h>
#include <linux/slab.h>

#include "pva.h"
#include "nvpva_buffer.h"
#include "nvpva_client.h"
#include "pva_iommu_context_dev.h"

/* Maximum contexts KMD creates per engine */
#define NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG (MAX_PVA_CLIENTS)

/* Search if the pid already have a context
 * The function does below things;
 * 1. loop through each clients in the client array and validates pid.
 * 2. Also tracks the first free client in the array
 */
static struct nvpva_client_context *
client_context_search_locked(struct platform_device *pdev,
			     struct pva *dev,
			     pid_t pid)
{
	struct nvpva_client_context *c_node = NULL;
	uint32_t i;
	bool shared_cntxt_dev;

	for (i = 0U; i < NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG; i++) {
		c_node = &dev->clients[i];
		if ((c_node->ref_count != 0U) && (c_node->pid == pid))
			return c_node;
	}

	for (i = 0U; i < NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG; i++) {
		c_node = &dev->clients[i];
		if (c_node->ref_count == 0U)
			break;
	}

	if (i >= NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG)
		return NULL;

	shared_cntxt_dev =  i > (NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG - 3);

	c_node->pid = pid;
	c_node->pva = dev;
	c_node->curr_sema_value = 0;
	mutex_init(&c_node->sema_val_lock);
	if (dev->version == PVA_HW_GEN2) {
		c_node->cntxt_dev =
			nvpva_iommu_context_dev_allocate(NULL,
							 0,
							 shared_cntxt_dev);

		if (c_node->cntxt_dev == NULL)
			return NULL;

		c_node->sid_index = nvpva_get_id_idx(dev, c_node->cntxt_dev) - 1;
	} else {
		c_node->cntxt_dev = pdev;
		c_node->sid_index = 0;
	}

	c_node->elf_ctx.cntxt_dev = c_node->cntxt_dev;
	c_node->buffers = nvpva_buffer_init(dev->pdev, dev->aux_pdev, c_node->cntxt_dev);
	if (IS_ERR(c_node->buffers)) {
		dev_err(&dev->pdev->dev,
			"failed to init nvhost buffer for client:%lu",
			PTR_ERR(c_node->buffers));
		if (dev->version == PVA_HW_GEN2)
			nvpva_iommu_context_dev_release(c_node->cntxt_dev);
		c_node = NULL;
	}

	return c_node;
}

/* Allocate a client context from the client array
 * The function does below things;
 * 1. Search for an existing client context, if not found then a free client
 * 2. Allocate a buffer pool if needed
 */
struct nvpva_client_context
*nvpva_client_context_alloc(struct platform_device *pdev,
			    struct pva *dev,
			    pid_t pid)
{
	struct nvpva_client_context *client = NULL;

	mutex_lock(&dev->clients_lock);
	client = client_context_search_locked(pdev, dev, pid);
	if (client != NULL)
		client->ref_count += 1;

	mutex_unlock(&dev->clients_lock);

	return client;
}

void nvpva_client_context_get(struct nvpva_client_context *client)
{
	struct pva *dev = client->pva;

	mutex_lock(&dev->clients_lock);
	client->ref_count += 1;
	mutex_unlock(&dev->clients_lock);
}

/* Free a client context from the client array */
static void
nvpva_client_context_free_locked(struct nvpva_client_context *client)
{
	nvpva_buffer_release(client->buffers);
	nvpva_iommu_context_dev_release(client->cntxt_dev);
	mutex_destroy(&client->sema_val_lock);
	client->buffers = NULL;
	client->pva = NULL;
	client->pid = 0;
	pva_unload_all_apps(&client->elf_ctx);
}

/* Release the client context
 * The function does below things;
 * 1. Reduce the active Q count
 * 2. Initiate freeing if the count is 0
 */
void nvpva_client_context_put(struct nvpva_client_context *client)
{
	struct pva *pva = client->pva;

	mutex_lock(&pva->clients_lock);
	client->ref_count--;
	if (client->ref_count == 0U)
		nvpva_client_context_free_locked(client);

	mutex_unlock(&pva->clients_lock);
}

/* De-initialize the client array for the device
 * The function does below things;
 * 1. Free all the remaining buffer pools if any
 * 2. Release the memory
 */
void nvpva_client_context_deinit(struct pva *dev)
{
	struct nvpva_client_context *client;
	uint32_t max_clients;
	uint32_t i;

	max_clients = NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG;
	if (dev->clients != NULL) {
		mutex_lock(&dev->clients_lock);
		for (i = 0U; i < max_clients; i++) {
			client = &dev->clients[i];
			pva_vpu_deinit(&client->elf_ctx);
		}
		mutex_unlock(&dev->clients_lock);
		mutex_destroy(&dev->clients_lock);
		kfree(dev->clients);
		dev->clients = NULL;
	}
}

/* Initialize a set of clients for the device
 * The function does below things;
 * 1. Allocate memory for maximum number of clients
 * 2. Assign stream ID for each client contexts
 */
int nvpva_client_context_init(struct pva *pva)
{
	struct nvpva_client_context *clients = NULL;
	uint32_t max_clients;
	uint32_t j = 0U;
	int err = 0;

	max_clients = NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG;
	clients = kcalloc(max_clients, sizeof(struct nvpva_client_context),
			  GFP_KERNEL);
	if (clients == NULL) {
		err = -ENOMEM;
		goto done;
	}
	mutex_init(&pva->clients_lock);
	for (j = 0U; j < NVPVA_CLIENT_MAX_CONTEXTS_PER_ENG; j++) {
		err = pva_vpu_init(pva, &clients[j].elf_ctx);
		if (err < 0) {
			dev_err(&pva->pdev->dev,
				"No memory for allocating VPU parsing");
			goto vpu_init_fail;
		}
	}

	pva->clients = clients;
	return err;

vpu_init_fail:
	while (j--)
		pva_vpu_deinit(&clients[j].elf_ctx);

	kfree(clients);
	mutex_destroy(&pva->clients_lock);
done:
	return err;
}
