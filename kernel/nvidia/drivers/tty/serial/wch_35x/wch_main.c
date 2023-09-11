/*
 * PCI/PCIE to serial driver for ch351/352/353/355/356/357/358/359/382/384, etc.
 *
 * Copyright (C) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
 * Web: http://wch.cn
 * Author: WCH <tech@wch.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Update Log:
 * V1.00 - initial version
 * V1.10 - fixed uart send bugs
 * V1.11 - fixed the issue of serial ports number creation
 * V1.12 - fixed modem signals support
 * V1.13 - added automatic frequency multiplication when using baud rates higher than 115200bps
                 - added mutex protection in uart transmit process
 * V1.14 - optimized the processing of serial ports in interruption
 * V1.15 - added support for non-standard baud rate
 * V1.16 - fixed uart clock frequency multiplication bugs
 * V1.17 - modified uart data received process
 * V1.18 - changed uart fifo trigger level
         - changed uart transmission to half of the fifo size
 * V1.19 - fixed ch358 uart0 setting bug
 * V1.20 - added pre-load driver
 * V1.21 - fixed modem setting when disable hardflow
 * V1.22 - added support for rs485 configuration
 * V1.23 - added supports for kernel version beyond 5.14.x
 * V1.24 - fixed ch351/2/3 uart0 setting bug, merged pre-load driver
 */

/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */


#include "wch_common.h"

extern struct wch_board wch_board_table[WCH_BOARDS_MAX];
extern struct wch_ser_port wch_ser_table[WCH_SER_TOTAL_MAX + 1];
extern unsigned char ch365_32s;

struct wch_board wch_board_table[WCH_BOARDS_MAX];
struct wch_ser_port wch_ser_table[WCH_SER_TOTAL_MAX + 1];

int wch_ser_port_total_cnt;
unsigned char ch365_32s = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
struct pci_device_id wch_pci_board_id[] = {
    {VENDOR_ID_WCH_CH351, DEVICE_ID_WCH_CH351_2S, SUB_VENDOR_ID_WCH_CH351, SUB_DEVICE_ID_WCH_CH351_2S, 0, 0,
     WCH_BOARD_CH351_2S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH352_2S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH352_2S, 0, 0,
     WCH_BOARD_CH352_2S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH352_1S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH352_1S1P, 0, 0,
     WCH_BOARD_CH352_1S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH353_4S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH353_4S, 0, 0,
     WCH_BOARD_CH353_4S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH353_2S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH353_2S1P, 0, 0,
     WCH_BOARD_CH353_2S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH353_2S1PAR, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH353_2S1PAR, 0, 0,
     WCH_BOARD_CH353_2S1PAR},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH355_4S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH355_4S, 0, 0,
     WCH_BOARD_CH355_4S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH356_4S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH356_4S1P, 0, 0,
     WCH_BOARD_CH356_4S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH356_6S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH356_6S, 0, 0,
     WCH_BOARD_CH356_6S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH356_8S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH356_8S, 0, 0,
     WCH_BOARD_CH356_8S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH357_4S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH357_4S, 0, 0,
     WCH_BOARD_CH357_4S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH358_4S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH358_4S1P, 0, 0,
     WCH_BOARD_CH358_4S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH358_8S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH358_8S, 0, 0,
     WCH_BOARD_CH358_8S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH359_16S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH359_16S, 0, 0,
     WCH_BOARD_CH359_16S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH382_2S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH382_2S, 0, 0,
     WCH_BOARD_CH382_2S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH382_2S1P, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH382_2S1P, 0, 0,
     WCH_BOARD_CH382_2S1P},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_4S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_4S, 0, 0,
     WCH_BOARD_CH384_4S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_4S1P, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_4S1P, 0, 0,
     WCH_BOARD_CH384_4S1P},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_8S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_8S, 0, 0,
     WCH_BOARD_CH384_8S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_28S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_28S, 0, 0,
     WCH_BOARD_CH384_28S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH365_32S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH365_32S, 0, 0,
     WCH_BOARD_CH365_32S},
    {0}
};
MODULE_DEVICE_TABLE(pci, wch_pci_board_id);
#else

static struct wch_pci_info wch_pci_board_id[] = {
    {VENDOR_ID_WCH_CH351, DEVICE_ID_WCH_CH351_2S, SUB_VENDOR_ID_WCH_CH351, SUB_DEVICE_ID_WCH_CH351_2S,
     WCH_BOARD_CH351_2S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH352_2S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH352_2S, WCH_BOARD_CH352_2S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH352_1S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH352_1S1P,
     WCH_BOARD_CH352_1S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH353_4S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH353_4S, WCH_BOARD_CH353_4S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH353_2S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH353_2S1P,
     WCH_BOARD_CH353_2S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH353_2S1PAR, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH353_2S1PAR,
     WCH_BOARD_CH353_2S1PAR},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH355_4S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH355_4S, WCH_BOARD_CH355_4S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH356_4S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH356_4S1P,
     WCH_BOARD_CH356_4S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH356_6S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH356_6S, WCH_BOARD_CH356_6S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH356_8S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH356_8S, WCH_BOARD_CH356_8S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH357_4S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH357_4S, WCH_BOARD_CH357_4S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH358_4S1P, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH358_4S1P,
     WCH_BOARD_CH358_4S1P},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH358_8S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH358_8S, WCH_BOARD_CH358_8S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH359_16S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH359_16S,
     WCH_BOARD_CH359_16S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH382_2S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH382_2S,
     WCH_BOARD_CH382_2S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH382_2S1P, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH382_2S1P,
     WCH_BOARD_CH382_2S1P},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_4S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_4S,
     WCH_BOARD_CH384_4S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_4S1P, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_4S1P,
     WCH_BOARD_CH384_4S1P},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_8S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_8S,
     WCH_BOARD_CH384_8S},
    {VENDOR_ID_WCH_PCIE, DEVICE_ID_WCH_CH384_28S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_28S,
     WCH_BOARD_CH384_28S},
    {VENDOR_ID_WCH_PCI, DEVICE_ID_WCH_CH365_32S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH365_32S,
     WCH_BOARD_CH365_32S},
    {0}
};
#endif



#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18))
static irqreturn_t wch_interrupt(int irq, void *dev_id)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
static irqreturn_t wch_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static void wch_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
    struct wch_ser_port *sp = NULL;
    struct wch_board *sb = NULL;
    int i;
    int status = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    int handled = IRQ_NONE;
#endif

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        if (dev_id == &(wch_board_table[i])) {
            sb = dev_id;
            break;
        }
    }

    if (i == WCH_BOARDS_MAX) {
        status = 1;
    }

    if (!sb) {
        status = 1;
    }

    if (sb->board_enum <= 0) {
        status = 1;
    }

    if (status != 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        return handled;
#else
        return;
#endif
    }

    if ((sb->ser_ports > 0) && (sb->ser_isr != NULL)) {
        sp = &wch_ser_table[sb->ser_port_index];

        if (!sp) {
            status = 1;
        }

        status = sb->ser_isr(sb, sp);
    }

    if (status != 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        return handled;
#else
        return;
#endif
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    handled = IRQ_HANDLED;
    return handled;
#endif
}

static int wch_pci_board_probe(void)
{
    struct wch_board *sb;
    struct pci_dev *pdev = NULL;
    struct pci_dev *pdev_array[4] = {NULL, NULL, NULL, NULL};

    int wch_pci_board_id_cnt;
    int table_cnt;
    int board_cnt;
    int i;
    unsigned short int sub_device_id;
    int status;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    // clear and init some variable
    memset(wch_board_table, 0, WCH_BOARDS_MAX * sizeof(struct wch_board));

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        wch_board_table[i].board_enum = -1;
        wch_board_table[i].board_number = -1;
    }

    wch_pci_board_id_cnt = (sizeof(wch_pci_board_id) / sizeof(wch_pci_board_id[0])) - 1;

    // search wch serial and multi-I/O board
    pdev = NULL;
    table_cnt = 0;
    board_cnt = 0;
    status = 0;

    while (table_cnt < wch_pci_board_id_cnt) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        pdev = pci_get_device(wch_pci_board_id[table_cnt].vendor, wch_pci_board_id[table_cnt].device, pdev);
#else
        pdev = pci_find_device(wch_pci_board_id[table_cnt].vendor, wch_pci_board_id[table_cnt].device, pdev);
#endif
        if (pdev == NULL) {
            table_cnt++;
            continue;
        }

        if ((table_cnt > 0) && ((pdev == pdev_array[0]) || (pdev == pdev_array[1]) || (pdev == pdev_array[2]) ||
                                (pdev == pdev_array[3]))) {
            continue;
        }

        if (wch_pci_board_id[table_cnt].driver_data == WCH_BOARD_CH365_32S) {
            ch365_32s = 0x01;
        }

        pci_read_config_word(pdev, 0x2e, &sub_device_id);

        if (ch365_32s) {
        } else {
            if (sub_device_id == 0) {
                printk("WCH Error: WCH Board (bus:%d device:%d), in configuration space,\n", pdev->bus->number,
                       PCI_SLOT(pdev->devfn));
                printk("           subdevice id isn't vaild.\n\n");
                status = -EIO;
                return status;
            }

            if (sub_device_id != wch_pci_board_id[table_cnt].subdevice) {
                continue;
            }
        }
        if (pdev == NULL) {
            printk("WCH Error: PCI device object is an NULL pointer !\n\n");
            status = -EIO;
            return status;
        } else {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 4, 3))
            pci_disable_device(pdev);
#endif
#endif
            status = pci_enable_device(pdev);

            if (status != 0) {
                printk("WCH Error: WCH Board Enable Fail !\n\n");
                status = -ENXIO;
                return status;
            }
        }

        board_cnt++;
        if (board_cnt > WCH_BOARDS_MAX) {
            printk("\n");
            printk("WCH Error: WCH Driver Module Support Four Boards In Maximum !\n\n");
            status = -ENOSPC;
            return status;
        }

        sb = &wch_board_table[board_cnt - 1];

        pdev_array[board_cnt - 1] = pdev;
        sb->pdev = pdev;
        sb->bus_number = pdev->bus->number;
        sb->dev_number = PCI_SLOT(pdev->devfn);

        sb->board_enum = (int)wch_pci_board_id[table_cnt].driver_data;
        sb->pb_info = wch_pci_board_conf[sb->board_enum];

        sb->board_flag = sb->pb_info.board_flag;

        sb->board_number = board_cnt - 1;
    }

    if (board_cnt == 0) {
        printk("WCH Info : No WCH Multi-I/O Board Found !\n\n");
        status = -ENXIO;
        return status;
    } else {
        for (i = 0; i < WCH_BOARDS_MAX; i++) {
            sb = &wch_board_table[i];
            if (sb->board_enum > 0) {
                printk("\n");
                if ((sb->pb_info.num_serport) > 0) {
                    printk("WCH Info : Found WCH %s Series Board (%dS),\n", sb->pb_info.board_name,
                           sb->pb_info.num_serport);
                }

                printk("           bus number:%d, device number:%d\n\n", sb->bus_number, sb->dev_number);
            }
        }
    }

    return status;
}

static int wch_get_pci_board_conf(void)
{
    struct wch_board *sb = NULL;
    struct pci_dev *pdev = NULL;
    int status = 0;
    int i;
    int j;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];

        if (sb->board_enum > 0) {
            pdev = sb->pdev;
            sb->ser_ports = sb->pb_info.num_serport;

            wch_ser_port_total_cnt = wch_ser_port_total_cnt + sb->ser_ports;

            if (wch_ser_port_total_cnt > WCH_SER_TOTAL_MAX) {
                printk("WCH Error: Too much serial port, maximum %d ports can be supported !\n\n", WCH_SER_TOTAL_MAX);
                status = -EIO;
                return status;
            }

            for (j = 0; j < WCH_PCICFG_BAR_TOTAL; j++) {
                sb->bar_addr[j] = pci_resource_start(pdev, j);
            }

            if ((sb->board_flag & BOARDFLAG_CH365_32_PORTS) == BOARDFLAG_CH365_32_PORTS) {
                sb->board_membase = ioremap(sb->bar_addr[1], 4096);
                if (!sb->board_membase) {
                    status = -EIO;
                    printk("WCH Error: ioremap failed !\n");
                    return status;
                }
            }

            sb->irq = sb->pdev->irq;
            if (sb->irq <= 0) {
                printk(
                    "WCH Error: WCH Board %s Series (bus:%d device:%d), in configuartion space, irq isn't valid !\n\n",
                    sb->pb_info.board_name, sb->bus_number, sb->dev_number);

                status = -EIO;
                return status;
            }
        }
    }

    return status;
}

static int wch_assign_resource(void)
{
    struct wch_board *sb = NULL;
    struct wch_ser_port *sp = NULL;
    int status = 0;
    int i;
    int j;
    int ser_n;
    int ser_port_index = 0;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    memset(wch_ser_table, 0, (WCH_SER_TOTAL_MAX + 1) * sizeof(struct wch_ser_port));

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];

        if (sb->board_enum > 0) {
            if (sb->ser_ports > 0) {
                sb->vector_mask = 0;

                // assign serial port resource
                ser_n = sb->ser_port_index = ser_port_index;

                sp = &wch_ser_table[ser_n];

                if (sp == NULL) {
                    status = -ENXIO;
                    printk("WCH Error: Serial port table address error !\n");
                    return status;
                }

                for (j = 0; j < sb->ser_ports; j++, ser_n++, sp++) {
                    sp->port.chip_flag = sb->pb_info.port[j].chip_flag;
                    sp->port.iobase = sb->bar_addr[sb->pb_info.port[j].bar1] + sb->pb_info.port[j].offset1;

                    /* use scr reg to test io space */
                    outb(0x55, sp->port.iobase + UART_SCR);
                    if (inb(sp->port.iobase + UART_SCR) != 0x55) {
                        status = -ENXIO;
                        if (j == 0)
                            printk("WCH Error: pci/pcie address error !\n");
                        else
                            printk("WCH Error: ch432/ch438 communication error !\n");
                        return status;
                    }

                    if ((sb->board_flag & BOARDFLAG_REMAP) == BOARDFLAG_REMAP) {
                        sp->port.vector = 0;
                    } else if ((sb->board_flag & BOARDFLAG_CH384_8_PORTS) == BOARDFLAG_CH384_8_PORTS) {
                        sp->port.chip_iobase = sb->bar_addr[sb->pb_info.port[j].bar1];
                        sp->port.vector = sb->bar_addr[sb->pb_info.intr_vector_bar] + sb->pb_info.intr_vector_offset;
                    } else if ((sb->board_flag & BOARDFLAG_CH384_28_PORTS) == BOARDFLAG_CH384_28_PORTS) {
                        sp->port.chip_iobase = sb->bar_addr[sb->pb_info.port[j].bar1];
                        if (j >= 0 && j < 0x04) {
                            sp->port.vector =
                                sb->bar_addr[sb->pb_info.intr_vector_bar] + sb->pb_info.intr_vector_offset;
                        } else if (j >= 0x04 && j < 0x0C) {
                            sp->port.vector =
                                sb->bar_addr[sb->pb_info.intr_vector_bar] + sb->pb_info.intr_vector_offset_1;
                        } else if (j >= 0x0C && j < 0x14) {
                            sp->port.vector =
                                sb->bar_addr[sb->pb_info.intr_vector_bar] + sb->pb_info.intr_vector_offset_2;
                        } else if (j >= 0x14 && j < 0x1C) {
                            sp->port.vector =
                                sb->bar_addr[sb->pb_info.intr_vector_bar] + sb->pb_info.intr_vector_offset_3;
                        } else {
                        }
                    } else if ((sb->board_flag & BOARDFLAG_CH365_32_PORTS) == BOARDFLAG_CH365_32_PORTS) {
                        sp->port.chip_iobase = sb->bar_addr[sb->pb_info.port[j].bar1];
                        sp->port.board_membase = sb->board_membase;
                        if (j >= 0 && j < 0x08) {
                            if (j >= 0 && j < 0x04) {
                                sp->port.port_membase = sb->board_membase + 0x100 + j * 0x10;
                            }
                            if (j >= 4 && j < 0x08) {
                                sp->port.port_membase = sb->board_membase + 0x100 + 0x08 + (j - 4) * 0x10;
                            }
                        }
                        if (j >= 0x08 && j < 0x10) {
                            if (j >= 0x08 && j < 0x0C) {
                                sp->port.port_membase = sb->board_membase + 0x100 + 0x80 + (j - 0x08) * 0x10;
                            }
                            if (j >= 0x0C && j < 0x10) {
                                sp->port.port_membase = sb->board_membase + 0x100 + 0x80 + 0x08 + (j - 0x0C) * 0x10;
                            }
                        }
                        if (j >= 0x10 && j < 0x18) {
                            if (j >= 0x10 && j < 0x14) {
                                sp->port.port_membase = sb->board_membase + 0x100 + 0x100 + (j - 0x10) * 0x10;
                            }
                            if (j >= 0x14 && j < 0x18) {
                                sp->port.port_membase = sb->board_membase + 0x100 + 0x100 + 0x08 + (j - 0x14) * 0x10;
                            }
                        }
                        if (j >= 0x18 && j < 0x20) {
                            if (j >= 0x18 && j < 0x1C) {
                                sp->port.port_membase = sb->board_membase + 0x100 + 0x180 + (j - 0x18) * 0x10;
                            }
                            if (j >= 0x1C && j < 0x20) {
                                sp->port.port_membase = sb->board_membase + 0x100 + 0x180 + 0x08 + (j - 0x1C) * 0x10;
                            }
                        }
                    } else {
                        sp->port.vector = sb->bar_addr[sb->pb_info.intr_vector_bar] + sb->pb_info.intr_vector_offset;
                    }
                }
                sb->vector_mask = 0xffffffff;
                ser_port_index = ser_port_index + sb->ser_ports;
            }
        }
    }

    return status;
}

static int wch_ser_port_table_init(void)
{
    struct wch_board *sb = NULL;
    struct wch_ser_port *sp = NULL;
    int status = 0;
    int i;
    int j;
    int n;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];

        if (sb == NULL) {
            status = -ENXIO;
            printk("WCH Error: Board table pointer error !\n");
            return status;
        }

        if ((sb->board_enum > 0) && (sb->ser_ports > 0)) {
            n = sb->ser_port_index;
            sp = &wch_ser_table[n];

            if (sp == NULL) {
                status = -ENXIO;
                printk("WCH Error: Serial port table pointer error !\n");
                return status;
            }

            for (j = 0; j < sb->ser_ports; j++, n++, sp++) {
                sp->port.board_enum = sb->board_enum;
                sp->port.bus_number = sb->bus_number;
                sp->port.dev_number = sb->dev_number;
                sp->port.baud_base = CRYSTAL_FREQ * 2 / 16;
                sp->port.pb_info = sb->pb_info;
                if (sp->port.chip_flag == WCH_BOARD_CH384_8S) {
                    if (n == sb->ser_port_index)
                        sp->port.bext1stport = true;
                    else
                        sp->port.bext1stport = false;
                } else if (sp->port.chip_flag == WCH_BOARD_CH384_28S) {
                    if ((n == sb->ser_port_index + 4) || (n == sb->ser_port_index + 12) ||
                        (n == sb->ser_port_index + 20))
                        sp->port.bext1stport = true;
                    else
                        sp->port.bext1stport = false;
                } else if (sp->port.chip_flag == WCH_BOARD_CH355_4S || sp->port.chip_flag == WCH_BOARD_CH356_4S1P ||
                           sp->port.chip_flag == WCH_BOARD_CH356_6S || sp->port.chip_flag == WCH_BOARD_CH356_8S ||
                           sp->port.chip_flag == WCH_BOARD_CH358_4S1P || sp->port.chip_flag == WCH_BOARD_CH358_8S) {
                    if (n == sb->ser_port_index)
                        sp->port.bext1stport = true;
                    else
                        sp->port.bext1stport = false;
                } else if (sp->port.chip_flag == WCH_BOARD_CH359_16S) {
                    if ((n == sb->ser_port_index) || (n == sb->ser_port_index + 8))
                        sp->port.bext1stport = true;
                    else
                        sp->port.bext1stport = false;
                }

                if (sp->port.chip_flag == WCH_BOARD_CH351_2S || sp->port.chip_flag == WCH_BOARD_CH352_1S1P ||
                    sp->port.chip_flag == WCH_BOARD_CH352_2S || sp->port.chip_flag == WCH_BOARD_CH353_2S1P ||
                    sp->port.chip_flag == WCH_BOARD_CH353_2S1PAR || sp->port.chip_flag == WCH_BOARD_CH353_4S) {
                    if (n == sb->ser_port_index)
                        sp->port.bspe1stport = true;
                    else
                        sp->port.bspe1stport = false;
                }

                sp->port.irq = sb->irq;
                sp->port.line = n;
                sp->port.uartclk = CRYSTAL_FREQ * 2;
                if (ch365_32s) {
                    sp->port.iotype = WCH_UPIO_MEM;
                } else {
                    sp->port.iotype = WCH_UPIO_PORT;
                }

                sp->port.ldisc_stop_rx = 0;
                spin_lock_init(&sp->port.lock);

                if (sp->port.chip_flag == WCH_BOARD_CH351_2S) {
                    sp->port.type = PORT_SER_16550A;
                    sp->port.fifosize = CH351_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH351_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH352_2S || sp->port.chip_flag == WCH_BOARD_CH352_1S1P) {
                    sp->port.type = PORT_SER_16550A;
                    sp->port.fifosize = CH352_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH352_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH353_4S || sp->port.chip_flag == WCH_BOARD_CH353_2S1P ||
                           sp->port.chip_flag == WCH_BOARD_CH353_2S1PAR) {
                    sp->port.type = PORT_SER_16550A;
                    sp->port.fifosize = CH353_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH353_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH355_4S) {
                    sp->port.type = PORT_SER_16550A;
                    sp->port.fifosize = CH355_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH355_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH356_4S1P || sp->port.chip_flag == WCH_BOARD_CH356_6S ||
                           sp->port.chip_flag == WCH_BOARD_CH356_8S) {
                    sp->port.type = PORT_SER_16550A;
                    sp->port.fifosize = CH356_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH356_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH357_4S) {
                    sp->port.type = PORT_SER_16750;
                    sp->port.fifosize = CH357_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH357_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH358_4S1P || sp->port.chip_flag == WCH_BOARD_CH358_8S) {
                    sp->port.type = PORT_SER_16750;
                    sp->port.fifosize = CH358_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH358_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH359_16S) {
                    sp->port.type = PORT_SER_16750;
                    sp->port.fifosize = CH359_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH359_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH382_2S || sp->port.chip_flag == WCH_BOARD_CH382_2S1P) {
                    sp->port.type = PORT_SER_16750;
                    sp->port.fifosize = CH382_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH382_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH384_4S || sp->port.chip_flag == WCH_BOARD_CH384_4S1P) {
                    sp->port.type = PORT_SER_16750;
                    sp->port.fifosize = CH384_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH384_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH384_8S) {
                    sp->port.type = PORT_SER_16750;
                    sp->port.fifosize = CH358_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH358_TRIGGER_LEVEL_SET;
                } else if (sp->port.chip_flag == WCH_BOARD_CH384_28S) {
                    sp->port.type = PORT_SER_16750;
                    if (j >= 0 && j < 0x04) {
                        sp->port.fifosize = CH384_FIFOSIZE_SET;
                        sp->port.rx_trigger = CH384_TRIGGER_LEVEL_SET;
                    } else {
                        sp->port.fifosize = CH358_FIFOSIZE_SET;
                        sp->port.rx_trigger = CH358_TRIGGER_LEVEL_SET;
                    }
                } else if (sp->port.chip_flag == WCH_BOARD_CH365_32S) {
                    sp->port.type = PORT_SER_16750;
                    sp->port.fifosize = CH438_FIFOSIZE_SET;
                    sp->port.rx_trigger = CH438_TRIGGER_LEVEL_SET;
                } else {
                    sp->port.type = PORT_SER_16450;
                    sp->port.fifosize = DEFAULT_FIFOSIZE;
                    sp->port.rx_trigger = DEFAULT_TRIGGER_LEVEL;
                }

                if ((sb->pb_info.board_flag & BOARDFLAG_REMAP) == BOARDFLAG_REMAP) {
                    sp->port.vector_mask = 0;
                    sp->port.port_flag = PORTFLAG_REMAP;
                } else {
                    sp->port.vector_mask = sb->vector_mask;
                    sp->port.port_flag = PORTFLAG_NONE;
                }

                sp->port.setserial_flag = WCH_SER_BAUD_NOTSETSER;
            }

            sb->ser_isr = wch_ser_interrupt;
        } else {
            sb->ser_isr = NULL;
        }
    }

    return status;
}

#if WCH_DBG
void wch_debug(void)
{
#if WCH_DBG_BOARD
    struct wch_board *sb = NULL;
    int i;
#endif

#if WCH_DBG_SERPORT
    struct wch_ser_port *sp = NULL;
    int j;
#endif

#if WCH_DBG_BOARD
    printk("\n");
    printk("======== board info ========\n");
    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];
        if (sb->board_enum != -1) {
            printk(" name         : %s\n", sb->pb_info.board_name);
            printk(" board_enum   : %d\n", sb->board_enum);
            printk(" board_number : %d\n", sb->board_number);
            printk(" irq          : %d\n", sb->irq);
            printk(" vector_mask  : 0x%x\n", sb->vector_mask);
            printk(" bar[0]       : 0x%lx\n", sb->bar_addr[0]);
            printk(" bar[1]       : 0x%lx\n", sb->bar_addr[1]);
            printk(" bar[2]       : 0x%lx\n", sb->bar_addr[2]);
            printk(" bar[3]       : 0x%lx\n", sb->bar_addr[3]);
            printk(" bar[4]       : 0x%lx\n", sb->bar_addr[4]);
            printk(" bar[5]       : 0x%lx\n", sb->bar_addr[5]);
            printk("----------------------------\n");
        }
    }
    printk("============================\n");
    printk("\n");
#endif

#if WCH_DBG_SERPORT
    printk("\n");
    printk("======== serial info ========\n");
    for (j = 0; j < wch_ser_port_total_cnt; j++) {
        sp = &wch_ser_table[j];
        if (sp->port.iobase) {
            printk(" number       : %d\n", j);
            printk(" name         : %s\n", sp->port.pb_info.board_name);
            printk(" iobase       : 0x%lx\n", sp->port.iobase);
            printk(" chip_iobase  : 0x%x\n", sp->port.chip_iobase);
            printk(" irq          : %d\n", sp->port.irq);
            printk(" vector       : 0x%lx\n", sp->port.vector);
            printk(" vector_mask  : 0x%x\n", sp->port.vector_mask);
            printk(" chip_flag    : 0x%x\n", sp->port.chip_flag);
            printk(" port_flag    : 0x%x\n", sp->port.port_flag);
            printk("----------------------------\n");
        }
    }
    printk("============================\n");
    printk("\n");
#endif
}
#endif

#if WCH_DBG_SERIAL
void ch365_32s_test(void)
{
    struct wch_board *sb = NULL;
    int i;
    dbg_serial("\n");
    dbg_serial("======== board info ========\n");
    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];
        if (sb->board_enum != -1) {
            dbg_serial(" ch438_1_uart0 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05));
            dbg_serial(" ch438_1_uart1 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x08));
            dbg_serial(" ch438_1_uart2 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x10));
            dbg_serial(" ch438_1_uart3 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x18));
            dbg_serial(" ch438_1_uart4 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x20));
            dbg_serial(" ch438_1_uart5 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x28));
            dbg_serial(" ch438_1_uart6 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x30));
            dbg_serial(" ch438_1_uart7 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x38));
            dbg_serial("----------------------------\n");
            dbg_serial(" ch438_2_uart0 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x80));
            dbg_serial(" ch438_2_uart1 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x08 + 0x80));
            dbg_serial(" ch438_2_uart2 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x10 + 0x80));
            dbg_serial(" ch438_2_uart3 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x18 + 0x80));
            dbg_serial(" ch438_2_uart4 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x20 + 0x80));
            dbg_serial(" ch438_2_uart5 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x28 + 0x80));
            dbg_serial(" ch438_2_uart6 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x30 + 0x80));
            dbg_serial(" ch438_2_uart7 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x38 + 0x80));
            dbg_serial("----------------------------\n");
            dbg_serial(" ch438_3_uart0 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x100));
            dbg_serial(" ch438_3_uart1 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x08 + 0x100));
            dbg_serial(" ch438_3_uart2 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x10 + 0x100));
            dbg_serial(" ch438_3_uart3 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x18 + 0x100));
            dbg_serial(" ch438_3_uart4 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x20 + 0x100));
            dbg_serial(" ch438_3_uart5 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x28 + 0x100));
            dbg_serial(" ch438_3_uart6 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x30 + 0x100));
            dbg_serial(" ch438_3_uart7 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x38 + 0x100));
            dbg_serial("----------------------------\n");
            dbg_serial(" ch438_4_uart0 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x180));
            dbg_serial(" ch438_4_uart1 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x08 + 0x180));
            dbg_serial(" ch438_4_uart2 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x10 + 0x180));
            dbg_serial(" ch438_4_uart3 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x18 + 0x180));
            dbg_serial(" ch438_4_uart4 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x20 + 0x180));
            dbg_serial(" ch438_4_uart5 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x28 + 0x180));
            dbg_serial(" ch438_4_uart6 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x30 + 0x180));
            dbg_serial(" ch438_4_uart7 LSR = %x\n", readb(sb->board_membase + 0x100 + 0x05 + 0x38 + 0x180));
            dbg_serial("----------------------------\n");
        }
    }
    dbg_serial("============================\n");
    dbg_serial("\n");
}
#endif

int wch_register_irq(void)
{
    struct wch_board *sb = NULL;
    int status = 0;
    int i;
    unsigned long chip_iobase;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];

        if (sb == NULL) {
            status = -ENXIO;
            printk("WCH Error: Board table pointer error !\n");
            return status;
        }

        if (sb->board_enum > 0) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18))
            status = request_irq(sb->irq, wch_interrupt, IRQF_SHARED, "wch", sb);
#else
            status = request_irq(sb->irq, wch_interrupt, SA_SHIRQ, "wch", sb);
#endif

            if (status) {
                printk("WCH Error: WCH Multi-I/O %s Board(bus:%d device:%d), request\n", sb->pb_info.board_name,
                       sb->bus_number, sb->dev_number);
                printk("           IRQ %d fail, IRQ %d may be conflit with another device.\n", sb->irq, sb->irq);
                return status;
            }
        }
        if (ch365_32s) {
            outb(inb(sb->bar_addr[0] + 0xF8) & 0xFE, sb->bar_addr[0] + 0xF8);
            outb(((inb(sb->bar_addr[0] + 0xFA) & 0xFB) | 0x03),
                 sb->bar_addr[0] + 0xFA);  // set read/write plus width 240ns->120ns
        }

        if (((sb->board_flag & BOARDFLAG_CH384_8_PORTS) == BOARDFLAG_CH384_8_PORTS) ||
            ((sb->board_flag & BOARDFLAG_CH384_28_PORTS) == BOARDFLAG_CH384_28_PORTS)) {
            chip_iobase = sb->bar_addr[0];
            if (chip_iobase) {
                outb(inb(chip_iobase + 0xEB) | 0x02, chip_iobase + 0xEB);
                /* set read/write plus width 120ns->210ns */
                outb(inb(chip_iobase + 0xFA) | 0x10, chip_iobase + 0xFA);
            }
        }
    }

    return status;
}

void wch_iounmap(void)
{
    struct wch_board *sb = NULL;
    int i;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];

        if (sb->board_enum > 0) {
            iounmap(sb->board_membase);
        }
    }
}

void wch_release_irq(void)
{
    struct wch_board *sb = NULL;
    int i;
    unsigned long chip_iobase;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    for (i = 0; i < WCH_BOARDS_MAX; i++) {
        sb = &wch_board_table[i];

        if (sb->board_enum > 0) {
            free_irq(sb->irq, sb);
        }
        if (ch365_32s) {
            outb(inb(sb->bar_addr[0] + 0xF8) | 0x01, sb->bar_addr[0] + 0xF8);
        }
        if (((sb->board_flag & BOARDFLAG_CH384_8_PORTS) == BOARDFLAG_CH384_8_PORTS) ||
            ((sb->board_flag & BOARDFLAG_CH384_28_PORTS) == BOARDFLAG_CH384_28_PORTS)) {
            chip_iobase = sb->bar_addr[0];
            if (chip_iobase)
                outb(inb(chip_iobase + 0xEB) & 0xFD, chip_iobase + 0xEB);
        }
    }
}

static struct ser_driver wch_ser_reg = {
    .dev_name = "ttyWCH",
    .major = WCH_TTY_MAJOR,
    .minor = 0,
};

int wch_35x_init(void)
{
    int status = 0;

    printk("\n\n");
    printk("=====================  WCH Device Driver Module Install  =====================\n");
    printk("\n");
    printk("WCH Info : Loading WCH Multi-I/O Board Driver Module\n");
    printk("                                                       -- Date : %s\n", WCH_DRIVER_DATE);
    printk("                                                       -- Version : %s\n\n", WCH_DRIVER_VERSION);

    wch_ser_port_total_cnt = 0;

    status = wch_pci_board_probe();
    if (status != 0) {
        goto step1_fail;
    }
    printk("------------------->pci board probe success\n");
    status = wch_get_pci_board_conf();
    if (status != 0) {
        goto step1_fail;
    }
    printk("------------------->pci board conf success\n");

    status = wch_assign_resource();
    if (status != 0) {
        goto step1_fail;
    }
    printk("------------------->pci assign success\n");

    status = wch_ser_port_table_init();
    if (status != 0) {
        goto step1_fail;
    }
    printk("------------------->ser port table init success\n");

    status = wch_register_irq();
    if (status != 0) {
        goto step1_fail;
    }
    printk("------------------->pci register irq success\n");

    status = wch_ser_register_driver(&wch_ser_reg);
    if (status != 0) {
        goto step2_fail;
    }
    printk("------------------->ser register driver success\n");

    status = wch_ser_register_ports(&wch_ser_reg);
    if (status != 0) {
        goto step3_fail;
    }

#if WCH_DBG
    wch_debug();
//	ch365_32s_test();
#endif

    printk("================================================================================\n");
    return status;

step3_fail:

    wch_ser_unregister_driver(&wch_ser_reg);

step2_fail:

    wch_release_irq();

step1_fail:

    printk("WCH Error: Couldn't Loading WCH Multi-I/O Board Driver Module correctly,\n");
    printk("           please reboot system and try again. If still can't loading driver,\n");
    printk("           contact support.\n\n");
    printk("================================================================================\n");
    return status;
}

void wch_35x_exit(void)
{
    printk("\n\n");
    printk("====================  WCH Device Driver Module Uninstall  ====================\n");
    printk("\n");

    wch_ser_unregister_ports(&wch_ser_reg);
    printk("***********wch_ser_unregister_ports***************\n");
    wch_ser_unregister_driver(&wch_ser_reg);
    printk("***********wch_ser_unregister_driver_success***********\n");
    wch_iounmap();
    wch_release_irq();
    printk("WCH Info : Unload WCH Multi-I/O Board Driver Module Done.\n");
    printk("================================================================================\n");
}
