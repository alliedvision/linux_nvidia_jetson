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

#ifdef MODVERSIONS
#ifndef MODULE
#define MODULE
#endif
#endif

#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(ver, rel, seq) ((ver << 16) | (rel << 8) | seq)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0))  // for 2.4 kernel
#ifdef MODULE
#include <linux/config.h>
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include <asm/bitops.h>
#include <linux/autoconf.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/serial_core.h>
#include <linux/string.h>
#include <linux/timer.h>

#ifndef PCI_ANY_ID
#define PCI_ANY_ID (~0)
#endif
#endif

#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/signal.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39))
#include <linux/smp_lock.h>
#endif

#include <linux/circ_buf.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/tty_driver.h>
#include <linux/wait.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0))
#include <asm/segment.h>
#endif
#include <asm/serial.h>
#include <linux/interrupt.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sysrq.h>
#include <linux/workqueue.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <linux/kref.h>
#endif

#include <linux/ctype.h>
#include <linux/parport.h>
#include <linux/poll.h>

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17))
#include <linux/devfs_fs_kernel.h>
#endif

#include <linux/sched.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
#include <linux/sched/signal.h>
#endif

#include <linux/sched.h>

/*-------------------------------------------------------------------------------

 for wch_main.c

-------------------------------------------------------------------------------*/
/*******************************************************
WCH driver information
*******************************************************/
#define WCH_DRIVER_VERSION "1.24"
#define WCH_DRIVER_DATE    "2023.02"
#define WCH_DRIVER_AUTHOR  "WCH GROUP"
#define WCH_DRIVER_DESC    "WCH Multi-I/O Board Driver Module"

#define WCH_TTY_MAJOR 205

/*******************************************************
WCH driver debug
*******************************************************/
#define WCH_DBG         0
#define WCH_DBG_SERIAL  0
#define WCH_DBG_BOARD   1
#define WCH_DBG_SERPORT 1

#if WCH_DBG_SERIAL
static void dbg_serial(const char *fmt, ...)
{
    char buffer[256];
    int i, len;
    va_list arglist;

    va_start(arglist, fmt);
    vsprintf(&buffer[0], fmt, arglist);
    va_end(arglist);
    len = strlen(buffer);
    for (i = 0; i < len; i++) {
        outb(buffer[i], 0x3F8);
        mdelay(1);
    }
}
#endif

/*******************************************************
 * WCH board information
 *******************************************************/

// for vid pid subvid subpid
#define VENDOR_ID_WCH_PCIE             0x1C00
#define SUB_VENDOR_ID_WCH_PCIE         0x1C00
#define VENDOR_ID_WCH_PCI              0x4348
#define SUB_VENDOR_ID_WCH_PCI          0x4348
#define VENDOR_ID_WCH_CH351            0x1C00
#define SUB_VENDOR_ID_WCH_CH351        0x1C00
#define DEVICE_ID_WCH_CH351_2S         0x2273
#define SUB_DEVICE_ID_WCH_CH351_2S     0x2273
#define DEVICE_ID_WCH_CH352_1S1P       0x5053
#define SUB_DEVICE_ID_WCH_CH352_1S1P   0x5053
#define DEVICE_ID_WCH_CH352_2S         0x3253
#define SUB_DEVICE_ID_WCH_CH352_2S     0x3253
#define DEVICE_ID_WCH_CH353_4S         0x3453
#define SUB_DEVICE_ID_WCH_CH353_4S     0x3453
#define DEVICE_ID_WCH_CH353_2S1P       0x7053
#define SUB_DEVICE_ID_WCH_CH353_2S1P   0x3253
#define DEVICE_ID_WCH_CH353_2S1PAR     0x5046
#define SUB_DEVICE_ID_WCH_CH353_2S1PAR 0x5046
#define DEVICE_ID_WCH_CH355_4S         0x7173
#define SUB_DEVICE_ID_WCH_CH355_4S     0x3473
#define DEVICE_ID_WCH_CH356_4S1P       0x7073
#define SUB_DEVICE_ID_WCH_CH356_4S1P   0x3473
#define DEVICE_ID_WCH_CH356_6S         0x3873
#define SUB_DEVICE_ID_WCH_CH356_6S     0x3873
#define DEVICE_ID_WCH_CH356_8S         0x3853
#define SUB_DEVICE_ID_WCH_CH356_8S     0x3853
#define DEVICE_ID_WCH_CH357_4S         0x5334
#define SUB_DEVICE_ID_WCH_CH357_4S     0x5053
#define DEVICE_ID_WCH_CH358_4S1P       0x5334
#define SUB_DEVICE_ID_WCH_CH358_4S1P   0x5334
#define DEVICE_ID_WCH_CH358_8S         0x5338
#define SUB_DEVICE_ID_WCH_CH358_8S     0x5338
#define DEVICE_ID_WCH_CH359_16S        0x5838
#define SUB_DEVICE_ID_WCH_CH359_16S    0x5838
#define DEVICE_ID_WCH_CH382_2S         0x3253
#define SUB_DEVICE_ID_WCH_CH382_2S     0x3253
#define DEVICE_ID_WCH_CH382_2S1P       0x3250
#define SUB_DEVICE_ID_WCH_CH382_2S1P   0x3250
#define DEVICE_ID_WCH_CH384_4S         0x3470
#define SUB_DEVICE_ID_WCH_CH384_4S     0x3470
#define DEVICE_ID_WCH_CH384_4S1P       0x3450
#define SUB_DEVICE_ID_WCH_CH384_4S1P   0x3450
#define DEVICE_ID_WCH_CH384_8S         0x3853
#define SUB_DEVICE_ID_WCH_CH384_8S     0x3853
#define DEVICE_ID_WCH_CH384_28S        0x4353
#define SUB_DEVICE_ID_WCH_CH384_28S    0x4353
#define DEVICE_ID_WCH_CH365_32S        0x5049
#define SUB_DEVICE_ID_WCH_CH365_32S    0x5049

// for chip_flag
enum {
    NONE_BOARD = 0,
    WCH_BOARD_CH351_2S,
    WCH_BOARD_CH352_2S,
    WCH_BOARD_CH352_1S1P,
    WCH_BOARD_CH353_4S,
    WCH_BOARD_CH353_2S1P,
    WCH_BOARD_CH353_2S1PAR,
    WCH_BOARD_CH355_4S,
    WCH_BOARD_CH356_4S1P,
    WCH_BOARD_CH356_6S,
    WCH_BOARD_CH356_8S,
    WCH_BOARD_CH357_4S,
    WCH_BOARD_CH358_4S1P,
    WCH_BOARD_CH358_8S,
    WCH_BOARD_CH359_16S,
    WCH_BOARD_CH382_2S,
    WCH_BOARD_CH382_2S1P,
    WCH_BOARD_CH384_4S,
    WCH_BOARD_CH384_4S1P,
    WCH_BOARD_CH384_8S,
    WCH_BOARD_CH384_28S,
    WCH_BOARD_CH365_32S,
};

// for board_flag
#define BOARDFLAG_NONE           0x0000
#define BOARDFLAG_REMAP          0x0001
#define BOARDFLAG_CH365_04_PORTS 0x0002
#define BOARDFLAG_CH365_08_PORTS 0x0004
#define BOARDFLAG_CH365_32_PORTS 0x0008
#define BOARDFLAG_CH384_8_PORTS  0x0010
#define BOARDFLAG_CH384_28_PORTS 0x0020

// for port_flag
#define PORTFLAG_NONE           0x0000
#define PORTFLAG_REMAP          0x0001
#define PORTFLAG_CH365_04_PORTS 0x0002
#define PORTFLAG_CH365_08_PORTS 0x0004
#define PORTFLAG_CH365_32_PORTS 0x0008
#define PORTFLAG_CH384_8_PORTS  0x0010
#define PORTFLAG_CH384_28_PORTS 0x0020

// board info
#define WCH_BOARDS_MAX       0x08
#define WCH_PORT_ONBOARD_MAX 0x20
#define WCH_SER_TOTAL_MAX    0x100

extern int wch_ser_port_total_cnt;

/*******************************************************
 * uart information
 *******************************************************/

// external crystal freq
#define CRYSTAL_FREQ 22118400

// uart fifo info
#define CH351_FIFOSIZE_16             16
#define CH351_TRIGGER_LEVEL_16FIFO_01 1
#define CH351_TRIGGER_LEVEL_16FIFO_04 4
#define CH351_TRIGGER_LEVEL_16FIFO_08 8
#define CH351_TRIGGER_LEVEL_16FIFO_14 14

#define CH352_FIFOSIZE_16             16
#define CH352_TRIGGER_LEVEL_16FIFO_01 1
#define CH352_TRIGGER_LEVEL_16FIFO_04 4
#define CH352_TRIGGER_LEVEL_16FIFO_08 8
#define CH352_TRIGGER_LEVEL_16FIFO_14 14

#define CH353_FIFOSIZE_16             16
#define CH353_TRIGGER_LEVEL_16FIFO_01 1
#define CH353_TRIGGER_LEVEL_16FIFO_04 4
#define CH353_TRIGGER_LEVEL_16FIFO_08 8
#define CH353_TRIGGER_LEVEL_16FIFO_14 14

#define CH355_FIFOSIZE_16             16
#define CH355_TRIGGER_LEVEL_16FIFO_01 1
#define CH355_TRIGGER_LEVEL_16FIFO_04 4
#define CH355_TRIGGER_LEVEL_16FIFO_08 8
#define CH355_TRIGGER_LEVEL_16FIFO_14 14

#define CH356_FIFOSIZE_16             16
#define CH356_TRIGGER_LEVEL_16FIFO_01 1
#define CH356_TRIGGER_LEVEL_16FIFO_04 4
#define CH356_TRIGGER_LEVEL_16FIFO_08 8
#define CH356_TRIGGER_LEVEL_16FIFO_14 14

#define CH357_FIFOSIZE_128              128
#define CH357_TRIGGER_LEVEL_128FIFO_01  1
#define CH357_TRIGGER_LEVEL_128FIFO_32  32
#define CH357_TRIGGER_LEVEL_128FIFO_64  64
#define CH357_TRIGGER_LEVEL_128FIFO_112 112

#define CH358_FIFOSIZE_128              128
#define CH358_TRIGGER_LEVEL_128FIFO_01  1
#define CH358_TRIGGER_LEVEL_128FIFO_32  32
#define CH358_TRIGGER_LEVEL_128FIFO_64  64
#define CH358_TRIGGER_LEVEL_128FIFO_112 112

#define CH359_FIFOSIZE_128              128
#define CH359_TRIGGER_LEVEL_128FIFO_01  1
#define CH359_TRIGGER_LEVEL_128FIFO_32  32
#define CH359_TRIGGER_LEVEL_128FIFO_64  64
#define CH359_TRIGGER_LEVEL_128FIFO_112 112

#define CH382_FIFOSIZE_256              256
#define CH382_TRIGGER_LEVEL_256FIFO_01  1
#define CH382_TRIGGER_LEVEL_256FIFO_32  32
#define CH382_TRIGGER_LEVEL_256FIFO_128 128
#define CH382_TRIGGER_LEVEL_256FIFO_224 224

#define CH384_FIFOSIZE_256              256
#define CH384_TRIGGER_LEVEL_256FIFO_01  1
#define CH384_TRIGGER_LEVEL_256FIFO_32  32
#define CH384_TRIGGER_LEVEL_256FIFO_128 128
#define CH384_TRIGGER_LEVEL_256FIFO_224 224

#define CH432_FIFOSIZE_16             16
#define CH432_TRIGGER_LEVEL_16FIFO_01 1
#define CH432_TRIGGER_LEVEL_16FIFO_04 4
#define CH432_TRIGGER_LEVEL_16FIFO_08 8
#define CH432_TRIGGER_LEVEL_16FIFO_14 14

#define CH438_FIFOSIZE_128              128
#define CH438_TRIGGER_LEVEL_128FIFO_01  1
#define CH438_TRIGGER_LEVEL_128FIFO_16  16
#define CH438_TRIGGER_LEVEL_128FIFO_64  64
#define CH438_TRIGGER_LEVEL_128FIFO_112 112

// uart fifo setup
#define CH351_FIFOSIZE_SET      CH351_FIFOSIZE_16
#define CH351_TRIGGER_LEVEL_SET CH351_TRIGGER_LEVEL_16FIFO_08

#define CH352_FIFOSIZE_SET      CH352_FIFOSIZE_16
#define CH352_TRIGGER_LEVEL_SET CH352_TRIGGER_LEVEL_16FIFO_08

#define CH353_FIFOSIZE_SET      CH353_FIFOSIZE_16
#define CH353_TRIGGER_LEVEL_SET CH353_TRIGGER_LEVEL_16FIFO_08

#define CH355_FIFOSIZE_SET      CH355_FIFOSIZE_16
#define CH355_TRIGGER_LEVEL_SET CH355_TRIGGER_LEVEL_16FIFO_08

#define CH356_FIFOSIZE_SET      CH356_FIFOSIZE_16
#define CH356_TRIGGER_LEVEL_SET CH356_TRIGGER_LEVEL_16FIFO_08

#define CH357_FIFOSIZE_SET      CH357_FIFOSIZE_128
#define CH357_TRIGGER_LEVEL_SET CH357_TRIGGER_LEVEL_128FIFO_64

#define CH358_FIFOSIZE_SET      CH358_FIFOSIZE_128
#define CH358_TRIGGER_LEVEL_SET CH358_TRIGGER_LEVEL_128FIFO_64

#define CH359_FIFOSIZE_SET      CH359_FIFOSIZE_128
#define CH359_TRIGGER_LEVEL_SET CH359_TRIGGER_LEVEL_128FIFO_64

#define CH382_FIFOSIZE_SET      CH382_FIFOSIZE_256
#define CH382_TRIGGER_LEVEL_SET CH382_TRIGGER_LEVEL_256FIFO_128

#define CH384_FIFOSIZE_SET      CH384_FIFOSIZE_256
#define CH384_TRIGGER_LEVEL_SET CH384_TRIGGER_LEVEL_256FIFO_128

#define CH432_FIFOSIZE_SET      CH432_FIFOSIZE_16
#define CH432_TRIGGER_LEVEL_SET CH432_TRIGGER_LEVEL_16FIFO_08

#define CH438_FIFOSIZE_SET      CH438_FIFOSIZE_128
#define CH438_TRIGGER_LEVEL_SET CH438_TRIGGER_LEVEL_128FIFO_16

#define UART_TRIGGER00_FCR 0x00
#define UART_TRIGGER01_FCR 0x40
#define UART_TRIGGER10_FCR 0x80
#define UART_TRIGGER11_FCR 0xC0

#define UART_DEFAULT_FCR 0x00

#define DEFAULT_FIFOSIZE      1
#define DEFAULT_TRIGGER_LEVEL 1

// register status info
#define UART_LSR_ERR_IN_RFIFO 0x80
#define UART_MCR_AFE          0x20
#define UART_IIR_CTO          0x0C

// serial address length
#define WCH_SER_ADDRESS_LENGTH 0x08

// PCI configuration bar 0 ~ 5
#define WCH_PCICFG_BAR_TOTAL 0x06

/*******************************************************
 * miscellaneous Information
 *******************************************************/
#define INTERRUPT_COUNT 0x80
#define WAKEUP_CHARS    0x100

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19))
#define WCHTERMIOS ktermios
#else
#define WCHTERMIOS termios
#endif

// for ser_port->setserial_flag
#define WCH_SER_BAUD_SETSERIAL 0x01
#define WCH_SER_BAUD_NOTSETSER 0x00

/*******************************************************
struct define Information
*******************************************************/

// name length
#define WCH_BOARDNAME_LENGTH     0x0F
#define WCH_DRIVERVERSION_LENGTH 0x0F

struct ser_port_info {
    char board_name_info[WCH_BOARDNAME_LENGTH];
    unsigned int bus_number_info;
    unsigned int dev_number_info;
    unsigned int port_info;
    unsigned int base_info;
    unsigned int irq_info;
};

struct port {
    char type;

    int bar1;
    unsigned int offset1;
    unsigned char length1;

    int bar2;
    unsigned int offset2;
    unsigned char length2;

    unsigned int chip_flag;
};

struct pci_board {
    unsigned int vendor_id;
    unsigned int device_id;
    unsigned int sub_vendor_id;
    unsigned int sub_device_id;

    unsigned int num_serport;

    unsigned int intr_vector_bar;
    unsigned int intr_vector_offset;
    unsigned int intr_vector_offset_1;
    unsigned int intr_vector_offset_2;
    unsigned int intr_vector_offset_3;

    char board_name[WCH_BOARDNAME_LENGTH];
    unsigned int board_flag;

    struct port port[WCH_PORT_ONBOARD_MAX];
};

struct wch_board;
struct wch_ser_port;
struct wch_par_port;

struct wch_board {
    int board_enum;
    int board_number;
    unsigned int bus_number;
    unsigned int dev_number;

    unsigned int ser_ports;

    unsigned int ser_port_index;

    unsigned long bar_addr[WCH_PCICFG_BAR_TOTAL];
    unsigned int irq;
    void *board_membase;
    unsigned int board_flag;

    unsigned int vector_mask;
    struct pci_board pb_info;
    struct pci_dev *pdev;
    int (*ser_isr)(struct wch_board *, struct wch_ser_port *);
};

/*-------------------------------------------------------------------------------

 for wch_serial.c

-------------------------------------------------------------------------------*/
/*******************************************************
 * ioctl user define
 *******************************************************/
#define WCH_IOCTL               0x900
#define WCH_SER_DUMP_PORT_INFO  (WCH_IOCTL + 50)
#define WCH_SER_DUMP_PORT_PERF  (WCH_IOCTL + 51)
#define WCH_SER_DUMP_DRIVER_VER (WCH_IOCTL + 52)

/*******************************************************
 * serial define
 *******************************************************/
#define PORT_SER_UNKNOWN  0x00
#define PORT_SER_8250     0x01
#define PORT_SER_16450    0x02
#define PORT_SER_16550    0x03
#define PORT_SER_16550A   0x04
#define PORT_SER_CIRRUS   0x05
#define PORT_SER_16650    0x06
#define PORT_SER_16650V2  0x07
#define PORT_SER_16750    0x08
#define PORT_SER_MAX_UART 0x08

#define WCH_USF_CLOSING_WAIT_INF  (0)
#define WCH_USF_CLOSING_WAIT_NONE (65535)
#define WCH_UART_CONFIG_TYPE      (1 << 0)
#define WCH_UART_CONFIG_IRQ       (1 << 1)

#define WCH_UART_XMIT_SIZE 0x1000

#define ser_circ_empty(circ)         ((circ)->head == (circ)->tail)
#define ser_circ_clear(circ)         ((circ)->head = (circ)->tail = 0)
#define ser_circ_chars_pending(circ) (CIRC_CNT((circ)->head, (circ)->tail, WCH_UART_XMIT_SIZE))
#define ser_circ_chars_free(circ)    (CIRC_SPACE((circ)->head, (circ)->tail, WCH_UART_XMIT_SIZE))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0))
#define ser_tx_stopped(port)         ((port)->info->tty->flow.stopped || (port)->info->tty->hw_stopped)
#else
#define ser_tx_stopped(port)         ((port)->info->tty->stopped || (port)->info->tty->hw_stopped)
#endif

#if defined(__i386__) && (defined(CONFIG_M386) || defined(CONFIG_M486))
#define WCH_SERIAL_INLINE
#endif

#ifdef WCH_SERIAL_INLINE
#define _INLINE_ inline
#else
#define _INLINE_
#endif

#define WCH_UPIO_PORT (0)
#define WCH_UPIO_MEM  (1)

#define WCH_UPF_SAK              (1 << 2)
#define WCH_UPF_SPD_MASK         (0x1030)
#define WCH_UPF_SPD_HI           (0x0010)
#define WCH_UPF_SPD_VHI          (0x0020)
#define WCH_UPF_SPD_CUST         (0x0030)
#define WCH_UPF_SPD_SHI          (0x1000)
#define WCH_UPF_SPD_WARP         (0x1010)
#define WCH_UPF_SKIP_TEST        (1 << 6)
#define WCH_UPF_HARDPPS_CD       (1 << 11)
#define WCH_UPF_LOW_LATENCY      (1 << 13)
#define WCH_UPF_BUGGY_UART       (1 << 14)
#define WCH_UPF_MAGIC_MULTIPLIER (1 << 16)

#define WCH_UPF_CHANGE_MASK (0x17fff)
#define WCH_UPF_USR_MASK    (WCH_UPF_SPD_MASK | WCH_UPF_LOW_LATENCY)

#define WCH_UIF_CHECK_CD (1 << 25)
#define WCH_UIF_CTS_FLOW (1 << 26)

#define WCH_UIF_NORMAL_ACTIVE (1 << 29)
#define WCH_UIF_INITIALIZED   (1 << 31)

#define WCH_ENABLE_MS(port, cflag) ((port)->flags & WCH_UPF_HARDPPS_CD || (cflag)&CRTSCTS || !((cflag)&CLOCAL))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
#define WCH_SER_DEVNUM(x) ((x)->index)
#else
#define WCH_SER_DEVNUM(x) (MINOR((x)->device) - (x)->driver.minor_start)
#endif

struct ser_info;
struct ser_port;

struct ser_icount {
    __u32 cts;
    __u32 dsr;
    __u32 rng;
    __u32 dcd;
    __u32 rx;
    __u32 tx;
    __u32 frame;
    __u32 overrun;
    __u32 parity;
    __u32 brk;
    __u32 buf_overrun;
};

struct ser_info {
    struct tty_struct *tty;
    struct circ_buf xmit;
    unsigned int flags;
    unsigned char *tmpbuf;
    struct semaphore tmpbuf_sem;
    int blocked_open;
    struct tasklet_struct tlet;

    wait_queue_head_t open_wait;
    wait_queue_head_t delta_msr_wait;
};

struct ser_driver {
    const char *dev_name;
    int major;
    int minor;
    int nr;
    struct ser_state *state;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    struct tty_driver *tty_driver;
#else
    struct tty_driver tty_driver;
#endif
};

struct ser_port {
    spinlock_t lock;
    void *port_membase;
    void *board_membase;
    unsigned long iobase;
    unsigned int irq;
    unsigned int uartclk;
    unsigned int fifosize;
    unsigned char x_char;
    unsigned char iotype;

    unsigned int read_status_mask;
    unsigned int ignore_status_mask;
    struct ser_info *info;
    struct ser_state *state;
    struct ser_icount icount;

    unsigned int flags;
    unsigned int mctrl;
    unsigned int timeout;
    unsigned int type;
    unsigned int custom_divisor;
    unsigned int line;
    struct device *dev;

    int board_enum;
    unsigned int bus_number;
    unsigned int dev_number;
    struct pci_board pb_info;
    unsigned long vector;
    unsigned int chip_iobase;
    unsigned int vector_mask;
    unsigned char chip_flag;
    unsigned int port_flag;
    unsigned int baud_base;
    int rx_trigger;
    bool bext1stport;
    bool bspe1stport;
    bool hardflow;
    unsigned char ldisc_stop_rx;

    unsigned int setserial_flag;
};

struct ser_state {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
    struct tty_port port0;
#endif
    unsigned int close_delay;
    unsigned int closing_wait;
    int count;
    struct ser_info *info;
    struct ser_port *port;
    struct semaphore sem;
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
#else
struct wch_pci_info {
    unsigned short vendor;
    unsigned short device;
    unsigned short subvendor;
    unsigned short subdevice;
    unsigned short driver_data;
};
extern static struct wch_pci_info wch_pci_board_id[];
#endif

static inline int ser_handle_break(struct ser_port *port)
{
    struct ser_info *info = port->info;

    if (info->flags & WCH_UPF_SAK) {
        do_SAK(info->tty);
    }
    return 0;
}

static inline void ser_handle_dcd_change(struct ser_port *port, unsigned int status)
{
    struct ser_info *info = port->info;

    port->icount.dcd++;

    if (info->flags & WCH_UIF_CHECK_CD) {
        if (status) {
            wake_up_interruptible(&info->open_wait);
        } else if (info->tty) {
            tty_hangup(info->tty);
        }
    }
}

#include <linux/tty_flip.h>

static inline void ser_insert_buffer(struct ser_port *port, unsigned int status, unsigned int overrun,
                                     unsigned char *buf, unsigned int count, unsigned char flag)
{
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 30))
    struct tty_struct *tty = port->info->tty;
    int i;

    if ((status & port->ignore_status_mask & ~overrun) == 0) {
        tty_buffer_request_room(tty, count);
        for (i = 0; i < count; ++i)
            tty_insert_flip_char(tty, buf[i], flag);
    }

    if (status & ~port->ignore_status_mask & overrun) {
        tty_insert_flip_char(tty, 0, TTY_OVERRUN);
    }
#elif (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 33))
    struct tty_struct *tty = port->info->tty;

    if ((status & port->ignore_status_mask & ~overrun) == 0) {
        tty_insert_flip_string(tty, buf, count);
    }

    if (status & ~port->ignore_status_mask & overrun) {
        tty_insert_flip_char(tty, 0, TTY_OVERRUN);
    }
#elif (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 13))
    struct tty_struct *tty = port->info->tty;

    if ((status & port->ignore_status_mask & ~overrun) == 0) {
        tty_insert_flip_string_fixed_flag(tty, buf, flag, count);
    }

    if (status & ~port->ignore_status_mask & overrun) {
        tty_insert_flip_char(tty, 0, TTY_OVERRUN);
    }
#else
    struct tty_port *tty = &port->state->port0;

    if ((status & port->ignore_status_mask & ~overrun) == 0) {
        if (tty_insert_flip_string_fixed_flag(tty, buf, flag, count) == 0)
            ++port->icount.buf_overrun;
    }

    if (status & ~port->ignore_status_mask & overrun) {
        if (tty_insert_flip_char(tty, 0, TTY_OVERRUN) == 0)
            ++port->icount.buf_overrun;
    }
#endif
}

static inline void ser_insert_char(struct ser_port *port, unsigned int status, unsigned int overrun, unsigned int ch,
                                   unsigned int flag)
{
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 13))
    struct tty_struct *tty = port->info->tty;

    if ((status & port->ignore_status_mask & ~overrun) == 0) {
        tty_insert_flip_char(tty, ch, flag);
    }

    if (status & ~port->ignore_status_mask & overrun) {
        tty_insert_flip_char(tty, 0, TTY_OVERRUN);
    }
#else
    struct tty_port *tty = &port->state->port0;

    if ((status & port->ignore_status_mask & ~overrun) == 0) {
        if (tty_insert_flip_char(tty, ch, flag) == 0)
            ++port->icount.buf_overrun;
    }

    if (status & ~port->ignore_status_mask & overrun) {
        if (tty_insert_flip_char(tty, 0, TTY_OVERRUN) == 0)
            ++port->icount.buf_overrun;
    }
#endif
}



/*******************************************************
 * wch serial port struct
 *******************************************************/
struct wch_ser_port {
    struct ser_port port;
    struct timer_list timer;
    struct list_head list;

    unsigned int capabilities;
    unsigned char ier;
    unsigned char lcr;
    unsigned char mcr;
    unsigned char mcr_mask;
    unsigned char mcr_force;
    unsigned char lsr_break_flag;
};

/*-------------------------------------------------------------------------------
 * function and variable extern
 -------------------------------------------------------------------------------*/
// wch_devtable.c
extern struct pci_board wch_pci_board_conf[];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
extern struct pci_device_id wch_pci_board_id[];
#endif

// wch_serial.c
extern int wch_ser_register_ports(struct ser_driver *);
extern void wch_ser_unregister_ports(struct ser_driver *);
extern int wch_ser_register_driver(struct ser_driver *);
extern void wch_ser_unregister_driver(struct ser_driver *);
extern int wch_ser_interrupt(struct wch_board *, struct wch_ser_port *);

// wch_main.c
extern struct wch_board wch_board_table[WCH_BOARDS_MAX];
extern struct wch_ser_port wch_ser_table[WCH_SER_TOTAL_MAX + 1];
extern int wch_35x_init(void);
extern void wch_35x_exit(void);
extern unsigned char ch365_32s;
