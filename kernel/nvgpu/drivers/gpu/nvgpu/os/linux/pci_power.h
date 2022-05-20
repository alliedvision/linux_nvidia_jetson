/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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

#ifndef NVGPU_PCI_POWER_H
#define NVGPU_PCI_POWER_H

#include <linux/version.h>
#include <linux/pci.h>

#define NVGPU_POWER_OFF		0
#define NVGPU_POWER_ON		1

int nvgpu_pci_set_powerstate(char *dev_name, int powerstate);

int nvgpu_pci_add_pci_power(struct pci_dev *pdev);
int nvgpu_pci_clear_pci_power(const char *dev_name);

void *tegra_pcie_detach_controller(struct pci_dev *pdev);
int tegra_pcie_attach_controller(void *cookie);

int __init nvgpu_pci_power_init(struct pci_driver *nvgpu_pci_driver);
void __exit nvgpu_pci_power_exit(struct pci_driver *nvgpu_pci_driver);
void __exit nvgpu_pci_power_cleanup(void);

#endif
