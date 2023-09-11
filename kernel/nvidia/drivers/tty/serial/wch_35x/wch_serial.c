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

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
static DEFINE_SEMAPHORE(ser_port_sem);
#else
static DECLARE_MUTEX(ser_port_sem);
#endif

#define WCH_HIGH_BITS_OFFSET ((sizeof(long) - sizeof(int)) * 8)
#define wch_ser_users(state) ((state)->count + ((state)->info ? (state)->info->blocked_open : 0))

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
struct serial_uart_config {
    char *name;
    int dfl_xmit_fifo_size;
    int flags;
};
#endif

static const struct serial_uart_config wch_uart_config[PORT_SER_MAX_UART + 1] = {
    {  "unknown",  1,                               0},
    {     "8250",  1,                               0},
    {    "16450",  1,                               0},
    {    "16550",  1,                               0},
    {   "16550A", 16, UART_CLEAR_FIFO | UART_USE_FIFO},
    {   "Cirrus",  1,                               0},
    {  "ST16650",  1,                               0},
    {"ST16650V2", 32, UART_CLEAR_FIFO | UART_USE_FIFO},
    {  "TI16750", 64, UART_CLEAR_FIFO | UART_USE_FIFO},
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0))
static int ser_refcount;
static struct tty_struct *ser_tty[WCH_SER_TOTAL_MAX];
static struct termios *ser_termios[WCH_SER_TOTAL_MAX];
static struct termios *ser_termios_locked[WCH_SER_TOTAL_MAX];
#endif

static _INLINE_ void ser_handle_cts_change(struct ser_port *, unsigned int);
static _INLINE_ void ser_update_mctrl(struct ser_port *, unsigned int, unsigned int);
static void ser_write_wakeup(struct ser_port *);
static void ser_stop(struct tty_struct *);
static void _ser_start(struct tty_struct *);
static void ser_start(struct tty_struct *);
static void ser_tasklet_action(unsigned long);
static int ser_startup(struct ser_state *, int);
static void ser_shutdown(struct ser_state *);
static _INLINE_ void _ser_put_char(struct ser_port *, struct circ_buf *, unsigned char);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26))
static int ser_put_char(struct tty_struct *, unsigned char);
#else
static void ser_put_char(struct tty_struct *, unsigned char);
#endif
static void ser_flush_chars(struct tty_struct *);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static unsigned int ser_chars_in_buffer(struct tty_struct *tty);
#else
static int ser_chars_in_buffer(struct tty_struct *tty);
#endif
static void ser_flush_buffer(struct tty_struct *);
static void ser_send_xchar(struct tty_struct *, char);
static void ser_throttle(struct tty_struct *);
static void ser_unthrottle(struct tty_struct *);
static int ser_get_info(struct ser_state *, struct serial_struct *);
static int ser_set_info(struct ser_state *, struct serial_struct *);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static unsigned int ser_write_room(struct tty_struct *tty);
#else
static int ser_write_room(struct tty_struct *tty);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10))
static int ser_write(struct tty_struct *, const unsigned char *, int);
#else
static int ser_write(struct tty_struct *, int, const unsigned char *, int);
#endif
// static int ser_get_lsr_info(struct ser_state *, unsigned int *);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
static int ser_tiocmget(struct tty_struct *);
static int ser_tiocmset(struct tty_struct *, unsigned int, unsigned int);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
static int ser_tiocmget(struct tty_struct *, struct file *);
static int ser_tiocmset(struct tty_struct *, struct file *, unsigned int, unsigned int);
#else
static int ser_get_modem_info(struct ser_state *, unsigned int *);
static int ser_set_modem_info(struct ser_state *, unsigned int, unsigned int *);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
static int ser_break_ctl(struct tty_struct *, int);
#else
static void ser_break_ctl(struct tty_struct *, int);
#endif
static int ser_wait_modem_status(struct ser_state *, unsigned long);
static int ser_get_count(struct ser_state *, struct serial_icounter_struct *);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
static int ser_ioctl(struct tty_struct *, unsigned int, unsigned long);
#else
static int ser_ioctl(struct tty_struct *, struct file *, unsigned int, unsigned long);
#endif
static void ser_hangup(struct tty_struct *);
unsigned int ser_get_divisor(struct ser_port *, unsigned int, bool);
unsigned int ser_get_baud_rate(struct ser_port *, struct WCHTERMIOS *, struct WCHTERMIOS *, unsigned int, unsigned int);
static void ser_change_speed(struct ser_state *, struct WCHTERMIOS *);
static void ser_set_termios(struct tty_struct *, struct WCHTERMIOS *);
static void ser_update_termios(struct ser_state *);
static void ser_update_timeout(struct ser_port *, unsigned int, unsigned int);
static struct ser_state *ser_get(struct ser_driver *, int);
static int ser_block_til_ready(struct file *, struct ser_state *);
static void ser_wait_until_sent(struct tty_struct *, int);
static int ser_open(struct tty_struct *, struct file *);
static void ser_close(struct tty_struct *, struct file *);

static void wch_ser_set_mctrl(struct ser_port *, unsigned int);
static unsigned int wch_ser_tx_empty(struct ser_port *);
static unsigned int wch_ser_get_mctrl(struct ser_port *);
static void wch_ser_stop_tx(struct ser_port *, unsigned int);
static void wch_ser_start_tx(struct ser_port *, unsigned int);
static void wch_ser_stop_rx(struct ser_port *);
static void wch_ser_enable_ms(struct ser_port *);
static void wch_ser_break_ctl(struct ser_port *, int);
static int wch_ser_startup(struct ser_port *);
static void wch_ser_shutdown(struct ser_port *);
static unsigned int wch_ser_get_divisor(struct ser_port *, unsigned int, bool);
static void wch_ser_set_termios(struct ser_port *, struct WCHTERMIOS *, struct WCHTERMIOS *);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
static void wch_ser_timeout(unsigned long);
#else
static void wch_ser_timeout(struct timer_list *);
#endif

static _INLINE_ void ser_receive_chars(struct wch_ser_port *, unsigned char *, unsigned char);
static _INLINE_ void ser_transmit_chars(struct wch_ser_port *);
static _INLINE_ void ser_check_modem_status(struct wch_ser_port *, unsigned char);
static _INLINE_ void ser_handle_port(struct wch_ser_port *, unsigned char);

extern int wch_ser_interrupt(struct wch_board *, struct wch_ser_port *);
static void wch_ser_release_io(struct ser_port *);
static void wch_ser_request_io(struct ser_port *);
static void wch_ser_configure_port(struct ser_driver *, struct ser_state *, struct ser_port *);
static void wch_ser_unconfigure_port(struct ser_driver *, struct ser_state *);
static int wch_ser_add_one_port(struct ser_driver *, struct ser_port *);
static int wch_ser_remove_one_port(struct ser_driver *, struct ser_port *);
extern int wch_ser_register_ports(struct ser_driver *);
extern void wch_ser_unregister_ports(struct ser_driver *);
extern int wch_ser_register_driver(struct ser_driver *);
extern void wch_ser_unregister_driver(struct ser_driver *);

static unsigned char READ_INTERRUPT_VECTOR_BYTE(struct wch_ser_port *);
static unsigned int READ_INTERRUPT_VECTOR_WORD(struct wch_ser_port *sp);
static unsigned char READ_UART_RX(struct wch_ser_port *);
static unsigned char READ_UART_IER(struct wch_ser_port *);
static unsigned char READ_UART_IIR(struct wch_ser_port *);
static unsigned char READ_UART_LCR(struct wch_ser_port *);
static unsigned char READ_UART_MCR(struct wch_ser_port *);
static unsigned char READ_UART_LSR(struct wch_ser_port *);
static unsigned char READ_UART_MSR(struct wch_ser_port *);
static void WRITE_UART_TX(struct wch_ser_port *, unsigned char);
static void WRITE_UART_IER(struct wch_ser_port *, unsigned char);
static void WRITE_UART_FCR(struct wch_ser_port *, unsigned char);
static void WRITE_UART_LCR(struct wch_ser_port *, unsigned char);
static void WRITE_UART_MCR(struct wch_ser_port *, unsigned char);
static void WRITE_UART_DLL(struct wch_ser_port *, int);
static void WRITE_UART_DLM(struct wch_ser_port *, int);

static unsigned char READ_INTERRUPT_VECTOR_BYTE(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.vector) {
        data = inb(sp->port.vector);

        return data;
    }

    return 0;
}

static unsigned int READ_INTERRUPT_VECTOR_WORD(struct wch_ser_port *sp)
{
    unsigned int data = 0;
    unsigned int vet1 = 0;
    unsigned int vet2 = 0;

    if (sp->port.vector) {
        vet1 = inb(sp->port.vector);
        vet2 = inb(sp->port.vector - 0x10);

        vet2 <<= 8;
        data = (vet1 | vet2);

        return data;
    }

    return 0;
}

static unsigned long READ_INTERRUPT_VECTOR_DWORD(struct wch_ser_port *sp)
{
    unsigned long data = 0;

    if (sp->port.iobase) {
        data = inl(sp->port.chip_iobase + 0xE8);
    }

    return data;
}

static unsigned char READ_UART_RX(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.iobase) {
        if (ch365_32s) {
            data = readb(sp->port.port_membase + UART_RX);
        } else {
            data = inb(sp->port.iobase + UART_RX);
        }

        return data;
    }

    return 0;
}

static unsigned char READ_UART_RX_BUFFER(struct wch_ser_port *sp, unsigned char *buf, int count)
{
    if (sp->port.iobase) {
        {
            insb(sp->port.iobase + UART_RX, buf, count);
        }
    }

    return 0;
}

static void WRITE_UART_TX(struct wch_ser_port *sp, unsigned char data)
{
    if (sp->port.iobase) {
        if (ch365_32s) {
            writeb(data, sp->port.port_membase + UART_TX);
        } else {
            outb(data, sp->port.iobase + UART_TX);
        }
    }
}

static void WRITE_UART_IER(struct wch_ser_port *sp, unsigned char data)
{
    if (sp->port.iobase) {
        if (ch365_32s) {
            writeb(data, sp->port.port_membase + UART_IER);
        } else {
            outb(data, sp->port.iobase + UART_IER);
        }
    }
}

static unsigned char READ_UART_IER(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.iobase) {
        if (ch365_32s) {
            data = readb(sp->port.port_membase + UART_IER);
        } else {
            data = inb(sp->port.iobase + UART_IER);
        }

        return data;
    }

    return 0;
}

static unsigned char READ_UART_IIR(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.iobase) {
        if (ch365_32s) {
            data = readb(sp->port.port_membase + UART_IIR);
        } else {
            data = inb(sp->port.iobase + UART_IIR);
        }

        return data;
    }

    return 0;
}

static void WRITE_UART_FCR(struct wch_ser_port *sp, unsigned char data)
{
    if (sp->port.iobase) {
        if (ch365_32s) {
            writeb(data, sp->port.port_membase + UART_FCR);
        } else {
            outb(data, sp->port.iobase + UART_FCR);
        }
    }
}

static unsigned char READ_UART_LCR(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.iobase) {
        if (ch365_32s) {
            data = readb(sp->port.port_membase + UART_LCR);
        } else {
            data = inb(sp->port.iobase + UART_LCR);
        }

        return data;
    }

    return 0;
}

static unsigned char READ_UART_MCR(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.iobase) {
        if (ch365_32s) {
            data = readb(sp->port.port_membase + UART_MCR);
        } else {
            data = inb(sp->port.iobase + UART_MCR);
        }

        return data;
    }

    return 0;
}

static void WRITE_UART_LCR(struct wch_ser_port *sp, unsigned char data)
{
    if (sp->port.iobase) {
        if (ch365_32s) {
            writeb(data, sp->port.port_membase + UART_LCR);
        } else {
            outb(data, sp->port.iobase + UART_LCR);
        }
    }
}

static void WRITE_UART_MCR(struct wch_ser_port *sp, unsigned char data)
{
    if (sp->port.iobase) {
        if (ch365_32s) {
            writeb(data, sp->port.port_membase + UART_MCR);
        } else {
            outb(data, sp->port.iobase + UART_MCR);
        }
    }
}

static unsigned char READ_UART_LSR(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.iobase) {
        if (ch365_32s) {
            data = readb(sp->port.port_membase + UART_LSR);
        } else {
            data = inb(sp->port.iobase + UART_LSR);
        }

        return data;
    }

    return 0;
}

static unsigned char READ_UART_MSR(struct wch_ser_port *sp)
{
    unsigned char data = 0;

    if (sp->port.iobase) {
        if (ch365_32s) {
            data = readb(sp->port.port_membase + UART_MSR);
        } else {
            data = inb(sp->port.iobase + UART_MSR);
        }

        return data;
    }

    return 0;
}

static void WRITE_UART_DLL(struct wch_ser_port *sp, int data)
{
    if (sp->port.iobase) {
        if (ch365_32s) {
            writeb(data, sp->port.port_membase + UART_DLL);
        } else {
            outb(data, sp->port.iobase + UART_DLL);
        }
    }
}

static void WRITE_UART_DLM(struct wch_ser_port *sp, int data)
{
    if (sp->port.iobase) {
        if (ch365_32s) {
            writeb(data, sp->port.port_membase + UART_DLM);
        } else {
            outb(data, sp->port.iobase + UART_DLM);
        }
    }
}

static _INLINE_ void ser_handle_cts_change(struct ser_port *port, unsigned int status)
{
    struct ser_info *info = port->info;
    struct tty_struct *tty = info->tty;

    port->icount.cts++;

    if (info->flags & WCH_UIF_CTS_FLOW) {
        if (tty->hw_stopped) {
            if (status) {
                tty->hw_stopped = 0;
                wch_ser_start_tx(port, 0);
                ser_write_wakeup(port);
            }
        } else {
            if (!status) {
                tty->hw_stopped = 1;
                wch_ser_stop_tx(port, 0);
            }
        }
    }
}

static _INLINE_ void ser_update_mctrl(struct ser_port *port, unsigned int set, unsigned int clear)
{
    unsigned long flags;
    unsigned int old;

    spin_lock_irqsave(&port->lock, flags);

    old = port->mctrl;
    port->mctrl = (old & ~clear) | set;

    if (port->hardflow)
        port->mctrl |= UART_MCR_RTS;

    if (old != port->mctrl) {
        wch_ser_set_mctrl(port, port->mctrl);
    }
    spin_unlock_irqrestore(&port->lock, flags);
}

#define set_mctrl(port, set)     ser_update_mctrl(port, set, 0)
#define clear_mctrl(port, clear) ser_update_mctrl(port, 0, clear)

static void ser_write_wakeup(struct ser_port *port)
{
    struct ser_info *info = port->info;
    tasklet_schedule(&info->tlet);
}

static void ser_stop(struct tty_struct *tty)
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    unsigned long flags;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;
    port = state->port;

    spin_lock_irqsave(&port->lock, flags);
    wch_ser_stop_tx(port, 1);
    spin_unlock_irqrestore(&port->lock, flags);
}

static void _ser_start(struct tty_struct *tty)
{
    struct ser_state *state = tty->driver_data;
    struct ser_port *port = state->port;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0))
    if (!ser_circ_empty(&state->info->xmit) && state->info->xmit.buf && !tty->flow.stopped && !tty->hw_stopped)
#else
    if (!ser_circ_empty(&state->info->xmit) && state->info->xmit.buf && !tty->stopped && !tty->hw_stopped)
#endif
        wch_ser_start_tx(port, 1);
}

static void ser_start(struct tty_struct *tty)
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    unsigned long flags;

    int line = WCH_SER_DEVNUM(tty);
    if (line >= wch_ser_port_total_cnt)
        return;

    state = tty->driver_data;
    port = state->port;

    spin_lock_irqsave(&port->lock, flags);
    _ser_start(tty);
    spin_unlock_irqrestore(&port->lock, flags);
}

static void ser_tasklet_action(unsigned long data)
{
    struct ser_state *state = (struct ser_state *)data;
    struct tty_struct *tty = NULL;
    tty = state->info->tty;
    if (tty) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31))
        if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc->ops->write_wakeup) {
            tty->ldisc->ops->write_wakeup(tty);
        }

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 30))
        {
            if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.ops->write_wakeup) {
                tty->ldisc.ops->write_wakeup(tty);
            }
        }
#else
        if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) {
            tty->ldisc.write_wakeup(tty);
        }
#endif
        wake_up_interruptible(&tty->write_wait);
    }
}

static int ser_startup(struct ser_state *state, int init_hw)
{
    struct ser_info *info = state->info;
    struct ser_port *port = state->port;
    unsigned long page;
    int retval = 0;

    if (info->flags & WCH_UIF_INITIALIZED) {
        return 0;
    }

    if (info->tty) {
        set_bit(TTY_IO_ERROR, &info->tty->flags);
    }

    if (port->type == PORT_UNKNOWN) {
        return 0;
    }

    if (!info->xmit.buf) {
        page = get_zeroed_page(GFP_KERNEL);
        if (!page) {
            return -ENOMEM;
        }

        info->xmit.buf = (unsigned char *)page;
        info->tmpbuf = info->xmit.buf + WCH_UART_XMIT_SIZE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
        sema_init(&info->tmpbuf_sem, 1);
#else
        init_MUTEX(&info->tmpbuf_sem);
#endif
        ser_circ_clear(&info->xmit);
    }

    retval = wch_ser_startup(port);

    if (retval == 0) {
        if (init_hw) {
            ser_change_speed(state, NULL);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
            if (info->tty->termios->c_cflag & CBAUD)
#else
            if (info->tty->termios.c_cflag & CBAUD)
#endif
            {
                set_mctrl(port, TIOCM_RTS | TIOCM_DTR);
            }
        }

        info->flags |= WCH_UIF_INITIALIZED;

        clear_bit(TTY_IO_ERROR, &info->tty->flags);
    }

    if (retval && capable(CAP_SYS_ADMIN)) {
        retval = 0;
    }
    set_mctrl(port, TIOCM_OUT2);

    return retval;
}

static void ser_shutdown(struct ser_state *state)
{
    struct ser_info *info = state->info;
    struct ser_port *port = state->port;
    struct wch_ser_port *sp = (struct wch_ser_port *)port;

    if (!(info->flags & WCH_UIF_INITIALIZED)) {
        return;
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
    if (!info->tty || (info->tty->termios->c_cflag & HUPCL))
#else
    if (!info->tty || (info->tty->termios.c_cflag & HUPCL))
#endif
    {
        clear_mctrl(port, TIOCM_DTR | TIOCM_RTS);
    }

    wake_up_interruptible(&info->delta_msr_wait);

    wch_ser_shutdown(port);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    synchronize_irq(port->irq);
#endif

    if (info->xmit.buf) {
        free_page((unsigned long)info->xmit.buf);
        info->xmit.buf = NULL;
        if (info->tmpbuf)
            info->tmpbuf = NULL;
    }

    tasklet_kill(&info->tlet);

    if (info->tty) {
        set_bit(TTY_IO_ERROR, &info->tty->flags);
    }

    // modified on 20200929
    sp->mcr = 0;
    clear_mctrl(port, TIOCM_OUT2 | TIOCM_DTR | TIOCM_RTS);

    info->flags &= ~WCH_UIF_INITIALIZED;
}

static _INLINE_ void _ser_put_char(struct ser_port *port, struct circ_buf *circ, unsigned char c)
{
    unsigned long flags;
    if (!circ->buf) {
        return;
    }

    spin_lock_irqsave(&port->lock, flags);

    if (ser_circ_chars_free(circ) != 0) {
        circ->buf[circ->head] = c;
        circ->head = (circ->head + 1) & (WCH_UART_XMIT_SIZE - 1);
    }
    spin_unlock_irqrestore(&port->lock, flags);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26))
static int ser_put_char(struct tty_struct *tty, unsigned char ch)
#else
static void ser_put_char(struct tty_struct *tty, unsigned char ch)
#endif
{
    struct ser_state *state = NULL;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26))
        return 0;
#else
        return;
#endif
    }

    state = tty->driver_data;
    _ser_put_char(state->port, &state->info->xmit, ch);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26))
    return 0;
#endif
}

static void ser_flush_chars(struct tty_struct *tty)
{
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    ser_start(tty);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static unsigned int ser_chars_in_buffer(struct tty_struct *tty)
#else
static int ser_chars_in_buffer(struct tty_struct *tty)
#endif
{
    struct ser_state *state = NULL;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return 0;
    }

    state = tty->driver_data;
    return ser_circ_chars_pending(&state->info->xmit);
}

static void ser_flush_buffer(struct tty_struct *tty)
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    unsigned long flags;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;
    port = state->port;

    if (!state || !state->info) {
        return;
    }

    spin_lock_irqsave(&port->lock, flags);
    ser_circ_clear(&state->info->xmit);
    spin_unlock_irqrestore(&port->lock, flags);

    wake_up_interruptible(&tty->write_wait);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31))
    if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc->ops->write_wakeup) {
        (tty->ldisc->ops->write_wakeup)(tty);
    }

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 30))

    if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.ops->write_wakeup) {
        (tty->ldisc.ops->write_wakeup)(tty);
    }

#else
    if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) {
        (tty->ldisc.write_wakeup)(tty);
    }
#endif
}

static void ser_send_xchar(struct tty_struct *tty, char ch)
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    unsigned long flags;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;
    port = state->port;
    port->x_char = ch;

    if (ch) {
        spin_lock_irqsave(&port->lock, flags);
        wch_ser_start_tx(port, 0);
        spin_unlock_irqrestore(&port->lock, flags);
    }
}

static void ser_throttle(struct tty_struct *tty)
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;
    port = state->port;
    port->ldisc_stop_rx = 1;

    if (I_IXOFF(tty)) {
        ser_send_xchar(tty, STOP_CHAR(tty));
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
    if (tty->termios->c_cflag & CRTSCTS)
#else
    if (tty->termios.c_cflag & CRTSCTS)
#endif
    {
        clear_mctrl(state->port, TIOCM_RTS);
    }
}

static void ser_unthrottle(struct tty_struct *tty)
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;
    port = state->port;
    port->ldisc_stop_rx = 0;

    if (I_IXOFF(tty)) {
        if (port->x_char) {
            port->x_char = 0;
        } else {
            ser_send_xchar(tty, START_CHAR(tty));
        }
    }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
    if (tty->termios->c_cflag & CRTSCTS)
#else
    if (tty->termios.c_cflag & CRTSCTS)
#endif
    {
        set_mctrl(port, TIOCM_RTS);
    }
}

static int ser_get_info(struct ser_state *state, struct serial_struct *retinfo)
{
    struct ser_port *port = state->port;
    struct serial_struct tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.type = port->type;
    tmp.line = port->line;
    tmp.port = port->iobase;

    if (WCH_HIGH_BITS_OFFSET) {
        tmp.port_high = (long)port->iobase >> WCH_HIGH_BITS_OFFSET;
    }

    tmp.irq = port->irq;
    tmp.flags = port->flags;
    tmp.xmit_fifo_size = port->fifosize;
    tmp.baud_base = port->uartclk / 16;
    tmp.close_delay = state->close_delay;
    tmp.closing_wait = state->closing_wait;
    tmp.custom_divisor = port->custom_divisor;
    tmp.io_type = port->iotype;

    if (copy_to_user(retinfo, &tmp, sizeof(*retinfo))) {
        return -EFAULT;
    }

    return 0;
}

static int ser_set_info(struct ser_state *state, struct serial_struct *newinfo)
{
    struct serial_struct new_serial;
    struct ser_port *port = state->port;
    unsigned long new_port;
    unsigned int change_irq;
    unsigned int change_port;
    unsigned int old_custom_divisor;
    unsigned int closing_wait;
    unsigned int close_delay;
    unsigned int old_flags;
    unsigned int new_flags;
    int retval = 0;
    if (copy_from_user(&new_serial, newinfo, sizeof(new_serial))) {
        return -EFAULT;
    }

    new_port = new_serial.port;

    if (WCH_HIGH_BITS_OFFSET) {
        new_port += (unsigned long)new_serial.port_high << WCH_HIGH_BITS_OFFSET;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    new_serial.irq = irq_canonicalize(new_serial.irq);
#endif

    close_delay = new_serial.close_delay;
    closing_wait =
        new_serial.closing_wait == ASYNC_CLOSING_WAIT_NONE ? WCH_USF_CLOSING_WAIT_NONE : new_serial.closing_wait;

    down(&state->sem);

    change_irq = new_serial.irq != port->irq;

    change_port = new_port != port->iobase || new_serial.io_type != port->iotype || new_serial.type != port->type;

    old_flags = port->flags;
    new_flags = new_serial.flags;
    old_custom_divisor = port->custom_divisor;

    if (!capable(CAP_SYS_ADMIN)) {
        retval = -EPERM;
        if (change_irq || change_port || (new_serial.baud_base != port->uartclk / 16) ||
            (close_delay != state->close_delay) || (closing_wait != state->closing_wait) ||
            (new_serial.xmit_fifo_size != port->fifosize) || (((new_flags ^ old_flags) & ~WCH_UPF_USR_MASK) != 0)) {
            goto exit;
        }

        port->flags = ((port->flags & ~WCH_UPF_USR_MASK) | (new_flags & WCH_UPF_USR_MASK));
        port->custom_divisor = new_serial.custom_divisor;
        goto check_and_exit;
    }

    if (change_port || change_irq) {
        retval = -EBUSY;

        if (wch_ser_users(state) > 1) {
            goto exit;
        }

        ser_shutdown(state);
    }

    if (change_port) {
        unsigned long old_iobase;
        unsigned int old_type;
        unsigned int old_iotype;

        old_iobase = port->iobase;
        old_type = port->type;
        old_iotype = port->iotype;

        if (old_type != PORT_UNKNOWN) {
            wch_ser_release_io(port);
        }

        port->iobase = new_port;
        port->type = new_serial.type;
        port->iotype = new_serial.io_type;

        retval = 0;
    }

    port->irq = new_serial.irq;
    port->uartclk = new_serial.baud_base * 16;
    port->flags = ((port->flags & ~WCH_UPF_CHANGE_MASK) | (new_flags & WCH_UPF_CHANGE_MASK));
    port->custom_divisor = new_serial.custom_divisor;
    state->close_delay = close_delay;
    state->closing_wait = closing_wait;
    port->fifosize = new_serial.xmit_fifo_size;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
    if (state->info->tty) {
        state->info->tty->low_latency = (port->flags & WCH_UPF_LOW_LATENCY) ? 1 : 0;
    }
#endif
check_and_exit:
    retval = 0;
    if (port->type == PORT_UNKNOWN) {
        goto exit;
    }

    if (state->info->flags & WCH_UIF_INITIALIZED) {
        if (((old_flags ^ port->flags) & WCH_UPF_SPD_MASK) || old_custom_divisor != port->custom_divisor) {
            if (port->flags & WCH_UPF_SPD_MASK) {
                printk("WCH Info : %s sets custom speed on ttyWCH%d. This is deprecated.\n", current->comm, port->line);
            }
            ser_change_speed(state, NULL);
        }
    } else {
        retval = ser_startup(state, 1);
    }
exit:
    up(&state->sem);

    return retval;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static unsigned int ser_write_room(struct tty_struct *tty)
#else
static int ser_write_room(struct tty_struct *tty)
#endif
{
    struct ser_state *state = NULL;
    int line = WCH_SER_DEVNUM(tty);
    int status = 0;
    if (line >= wch_ser_port_total_cnt) {
        return 0;
    }

    state = tty->driver_data;

    status = ser_circ_chars_free(&state->info->xmit);
    return status;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10))
static int ser_write(struct tty_struct *tty, const unsigned char *buf, int count)
#else
static int ser_write(struct tty_struct *tty, int from_user, const unsigned char *buf, int count)
#endif
{
    struct ser_state *state = tty->driver_data;
    struct ser_port *port = NULL;
    struct circ_buf *circ = NULL;
    unsigned long flags;
    int c;
    int ret = 0;

    if (!state || !state->info) {
        return -EL3HLT;
    }

    port = state->port;
    circ = &state->info->xmit;

    if (!circ->buf) {
        return 0;
    }

    spin_lock_irqsave(&port->lock, flags);
    while (1) {
        c = CIRC_SPACE_TO_END(circ->head, circ->tail, WCH_UART_XMIT_SIZE);
        if (count < c) {
            c = count;
        }

        if (c <= 0) {
            break;
        }
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 9))
        memcpy(circ->buf + circ->head, buf, c);
#else
        if (from_user) {
            if (copy_from_user((circ->buf + circ->head), buf, c) == c) {
                ret = -EFAULT;
                break;
            }
        } else {
            memcpy(circ->buf + circ->head, buf, c);
        }
#endif
        circ->head = (circ->head + c) & (WCH_UART_XMIT_SIZE - 1);
        buf += c;
        count -= c;
        ret += c;
    }
    _ser_start(tty);
    spin_unlock_irqrestore(&port->lock, flags);

    return ret;
}

/*
static int
ser_get_lsr_info(
                                 struct ser_state *state,
                                 unsigned int *value
                                 )
{
    struct ser_port *port = state->port;
    unsigned int result = 0;
    result = wch_ser_tx_empty(port);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0))
    if ((port->x_char) ||
        ((ser_circ_chars_pending(&state->info->xmit) > 0) &&
        !state->info->tty->flow.stopped && !state->info->tty->hw_stopped))
#else
    if ((port->x_char) ||
        ((ser_circ_chars_pending(&state->info->xmit) > 0) &&
        !state->info->tty->stopped && !state->info->tty->hw_stopped))
#endif
        result &= ~TIOCSER_TEMT;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18))
    if (copy_to_user(value, &result, sizeof(int)))
        return -EFAULT;

    return 0;
#else
    return put_user(result, value);
#endif
}
*/

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
static int ser_tiocmget(struct tty_struct *tty)
{
    struct ser_state *state;
    struct ser_port *port;
    int result = -EIO;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return 0;
    }

    state = tty->driver_data;
    port = state->port;

    down(&state->sem);

    if (!(tty->flags & (1 << TTY_IO_ERROR))) {
        result = port->mctrl;
        result |= wch_ser_get_mctrl(port);
    }

    up(&state->sem);

    return result;
}

static int ser_tiocmset(struct tty_struct *tty, unsigned int set, unsigned int clear)
{
    struct ser_state *state;
    struct ser_port *port;
    int ret = -EIO;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return 0;
    }

    state = tty->driver_data;
    port = state->port;

    down(&state->sem);

    if (!(tty->flags & (1 << TTY_IO_ERROR))) {
        ser_update_mctrl(port, set, clear);
        ret = 0;
    }

    up(&state->sem);

    return ret;
}

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
static int ser_tiocmget(struct tty_struct *tty, struct file *file)
{
    struct ser_state *state;
    struct ser_port *port;
    int result = -EIO;
    int line = WCH_SER_DEVNUM(tty);
    if (line >= wch_ser_port_total_cnt) {
        return 0;
    }

    state = tty->driver_data;
    port = state->port;

    down(&state->sem);

    if ((!file || !tty_hung_up_p(file)) && !(tty->flags & (1 << TTY_IO_ERROR))) {
        result = port->mctrl;
        result |= wch_ser_get_mctrl(port);
    }

    up(&state->sem);

    return result;
}

static int ser_tiocmset(struct tty_struct *tty, struct file *file, unsigned int set, unsigned int clear)
{
    struct ser_state *state;
    struct ser_port *port;
    int ret = -EIO;
    int line = WCH_SER_DEVNUM(tty);
    if (line >= wch_ser_port_total_cnt) {
        return 0;
    }

    state = tty->driver_data;
    port = state->port;

    down(&state->sem);

    if ((!file || !tty_hung_up_p(file)) && !(tty->flags & (1 << TTY_IO_ERROR))) {
        ser_update_mctrl(port, set, clear);
        ret = 0;
    }

    up(&state->sem);

    return ret;
}

#else
static int ser_get_modem_info(struct ser_state *state, unsigned int *value)
{
    struct ser_port *port = NULL;
    int line;
    unsigned int result;
    if (!state) {
        return -EIO;
    }

    port = state->port;

    if (!port) {
        return -EIO;
    }

    line = port->line;

    if (line >= wch_ser_port_total_cnt) {
        return -EIO;
    }

    result = port->mctrl;
    result |= wch_ser_get_mctrl(port);

    put_user(result, (unsigned long *)value);
    return 0;
}

static int ser_set_modem_info(struct ser_state *state, unsigned int cmd, unsigned int *value)
{
    struct ser_port *port = NULL;
    int line;
    unsigned int set = 0;
    unsigned int clr = 0;
    unsigned int arg;
    if (!state) {
        return -EIO;
    }

    port = state->port;

    if (!port) {
        return -EIO;
    }

    line = port->line;

    if (line >= wch_ser_port_total_cnt) {
        return -EIO;
    }

    get_user(arg, (unsigned long *)value);

    switch (cmd) {
    case TIOCMBIS: {
        if (arg & TIOCM_RTS) {
            set |= TIOCM_RTS;
        }

        if (arg & TIOCM_DTR) {
            set |= TIOCM_DTR;
        }

        if (arg & TIOCM_LOOP) {
            set |= TIOCM_LOOP;
        }
        break;
    }

    case TIOCMBIC: {
        if (arg & TIOCM_RTS) {
            clr |= TIOCM_RTS;
        }

        if (arg & TIOCM_DTR) {
            clr |= TIOCM_DTR;
        }

        if (arg & TIOCM_LOOP) {
            clr |= TIOCM_LOOP;
        }
        break;
    }

    case TIOCMSET: {
        if (arg & TIOCM_RTS) {
            set |= TIOCM_RTS;
        } else {
            clr |= TIOCM_RTS;
        }

        if (arg & TIOCM_DTR) {
            set |= TIOCM_DTR;
        } else {
            clr |= TIOCM_DTR;
        }

        if (arg & TIOCM_LOOP) {
            set |= TIOCM_LOOP;
        } else {
            clr |= TIOCM_LOOP;
        }
        break;
    }

    default: {
        return -EINVAL;
    }
    }

    ser_update_mctrl(port, set, clr);
    return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
static int ser_break_ctl(struct tty_struct *tty, int break_state)
#else
static void ser_break_ctl(struct tty_struct *tty, int break_state)
#endif
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    int line = WCH_SER_DEVNUM(tty);
    if (line >= wch_ser_port_total_cnt) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
        return 0;
#else
        return;
#endif
    }

    state = tty->driver_data;
    port = state->port;

    down(&state->sem);

    if (port->type != PORT_UNKNOWN) {
        wch_ser_break_ctl(port, break_state);
    }

    up(&state->sem);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
    return 0;
#endif
}

static int ser_wait_modem_status(struct ser_state *state, unsigned long arg)
{
    struct ser_port *port = state->port;
    DECLARE_WAITQUEUE(wait, current);
    struct ser_icount cprev;
    struct ser_icount cnow;
    int ret = 0;
    spin_lock_irq(&port->lock);
    memcpy(&cprev, &port->icount, sizeof(struct ser_icount));

    wch_ser_enable_ms(port);

    spin_unlock_irq(&port->lock);

    add_wait_queue(&state->info->delta_msr_wait, &wait);

    for (;;) {
        spin_lock_irq(&port->lock);
        memcpy(&cnow, &port->icount, sizeof(struct ser_icount));
        spin_unlock_irq(&port->lock);
        set_current_state(TASK_INTERRUPTIBLE);

        if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) || ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
            ((arg & TIOCM_CD) && (cnow.dcd != cprev.dcd)) || ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts))) {
            ret = 0;
            break;
        }

        schedule();
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            break;
        }
        cprev = cnow;
    }

    set_current_state(TASK_RUNNING);

    remove_wait_queue(&state->info->delta_msr_wait, &wait);
    return ret;
}

static int ser_get_count(struct ser_state *state, struct serial_icounter_struct *icnt)
{
    struct serial_icounter_struct icount;
    struct ser_icount cnow;
    struct ser_port *port = state->port;
    spin_lock_irq(&port->lock);
    memcpy(&cnow, &port->icount, sizeof(struct ser_icount));
    spin_unlock_irq(&port->lock);

    icount.cts = cnow.cts;
    icount.dsr = cnow.dsr;
    icount.rng = cnow.rng;
    icount.dcd = cnow.dcd;
    icount.rx = cnow.rx;
    icount.tx = cnow.tx;
    icount.frame = cnow.frame;
    icount.overrun = cnow.overrun;
    icount.parity = cnow.parity;
    icount.brk = cnow.brk;
    icount.buf_overrun = cnow.buf_overrun;

    return copy_to_user(icnt, &icount, sizeof(icount)) ? -EFAULT : 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
static void ser_config_rs485(struct ser_state *state, struct serial_rs485 *rs485)
{
    struct ser_port *port = state->port;
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    struct serial_rs485 rs485val = *rs485;
    unsigned char cval, mval;

    cval = READ_UART_LCR(sp);
    mval = READ_UART_MCR(sp);
    WRITE_UART_LCR(sp, cval | UART_LCR_DLAB);

    if (rs485val.flags & SER_RS485_ENABLED) {
        WRITE_UART_MCR(sp, mval | BIT(7));
    } else {
        WRITE_UART_MCR(sp, mval & ~BIT(7));
    }
    WRITE_UART_LCR(sp, cval);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
static int ser_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
#else
static int ser_ioctl(struct tty_struct *tty, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
    struct ser_state *state = NULL;
    int ret = -ENOIOCTLCMD;
    int line = WCH_SER_DEVNUM(tty);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0))
    int status = 0;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
    struct serial_rs485 rs485;
#endif

    if (line < wch_ser_port_total_cnt) {
        state = tty->driver_data;
    }

    switch (cmd) {
    case TIOCGSERIAL: {
        if (line < wch_ser_port_total_cnt) {
            ret = ser_get_info(state, (struct serial_struct *)arg);
        }
        break;
    }

    case TIOCSSERIAL: {
        if (line < wch_ser_port_total_cnt) {
            state->port->setserial_flag = WCH_SER_BAUD_SETSERIAL;
            ret = ser_set_info(state, (struct serial_struct *)arg);
        }
        break;
    }

    case TCGETS: {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 11, 0))
        struct ktermios kterm;
        mutex_lock(&tty->throttle_mutex);
        kterm = tty->termios;
        mutex_unlock(&tty->throttle_mutex);

        if (copy_to_user((struct termios __user *)arg, &kterm, sizeof(struct termios)))
            ret = -EFAULT;
        else
            ret = 0;
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
        struct ktermios kterm;
        mutex_lock(&tty->termios_mutex);
        kterm = tty->termios;
        mutex_unlock(&tty->termios_mutex);

        if (copy_to_user((struct termios __user *)arg, &kterm, sizeof(struct termios)))
            ret = -EFAULT;
        else
            ret = 0;
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19))
        struct ktermios *kterm;
        mutex_lock(&tty->termios_mutex);
        kterm = tty->termios;
        mutex_unlock(&tty->termios_mutex);

        if (copy_to_user((struct termios __user *)arg, &kterm, sizeof(struct termios)))
            ret = -EFAULT;
        else
            ret = 0;
#endif
        break;
    }

    case TCSETS: {
        if (line < wch_ser_port_total_cnt) {
            state->port->flags &= ~(WCH_UPF_SPD_HI | WCH_UPF_SPD_VHI | WCH_UPF_SPD_SHI | WCH_UPF_SPD_WARP);
            state->port->setserial_flag = WCH_SER_BAUD_NOTSETSER;
            ser_update_termios(state);
        }
        break;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
    case TIOCSRS485: {
        if (copy_from_user(&rs485, (void __user *)arg, sizeof(rs485)))
            return -EFAULT;

        ser_config_rs485(state, &rs485);
        break;
    }
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0))
    case TIOCMGET: {
        if (line < wch_ser_port_total_cnt) {
            ret = verify_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned int));

            if (ret) {
                return ret;
            }

            status = ser_get_modem_info(state, (unsigned int *)arg);
            return status;
        }
        break;
    }

    case TIOCMBIS:
    case TIOCMBIC:
    case TIOCMSET: {
        if (line < wch_ser_port_total_cnt) {
            status = ser_set_modem_info(state, cmd, (unsigned int *)arg);
            return status;
        }
        break;
    }
#endif

    case TIOCSERGWILD:
    case TIOCSERSWILD: {
        if (line < wch_ser_port_total_cnt) {
            ret = 0;
        }
        break;
    }
    case TIOCMIWAIT:
        if (line < wch_ser_port_total_cnt) {
            ret = ser_wait_modem_status(state, arg);
        }
        break;
    case TIOCGICOUNT:
        if (line < wch_ser_port_total_cnt) {
            ret = ser_get_count(state, (struct serial_icounter_struct *)arg);
        }
        break;
    }

    if (ret != -ENOIOCTLCMD) {
        goto out;
    }

    if (tty->flags & (1 << TTY_IO_ERROR)) {
        ret = -EIO;
        goto out;
    }
out:
    return ret;
}

static void ser_hangup(struct tty_struct *tty)
{
    struct ser_state *state = NULL;
    int line = WCH_SER_DEVNUM(tty);

    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;

    down(&state->sem);

    if (state->info && state->info->flags & WCH_UIF_NORMAL_ACTIVE) {
        ser_flush_buffer(tty);
        ser_shutdown(state);
        state->count = 0;
        state->info->flags &= ~WCH_UIF_NORMAL_ACTIVE;
        state->info->tty = NULL;
        wake_up_interruptible(&state->info->open_wait);
        wake_up_interruptible(&state->info->delta_msr_wait);
    }

    up(&state->sem);
}

unsigned int ser_get_divisor(struct ser_port *port, unsigned int baud, bool btwicefreq)
{
    unsigned int quot;

    if (baud == 38400 && (port->flags & WCH_UPF_SPD_MASK) == WCH_UPF_SPD_CUST) {
        quot = port->custom_divisor;
    } else {
        if (btwicefreq) {
            if (10 * port->uartclk / 16 / baud % 10 >= 5)
                quot = port->uartclk / 16 / baud + 1;
            else
                quot = port->uartclk / 16 / baud;
        } else {
            if (10 * port->uartclk / 24 / 16 / baud % 10 >= 5)
                quot = port->uartclk / 24 / 16 / baud + 1;
            else
                quot = port->uartclk / 24 / 16 / baud;
        }
    }

    return quot;
}

unsigned int ser_get_baud_rate(struct ser_port *port, struct WCHTERMIOS *termios, struct WCHTERMIOS *old,
                               unsigned int min, unsigned int max)
{
    unsigned int try;
    unsigned int baud;
    unsigned int altbaud = 38400;
    int hung_up = 0;
    unsigned int flags = port->flags & WCH_UPF_SPD_MASK;

    if (port->flags & WCH_UPF_SPD_MASK) {
        altbaud = 38400;
        if (flags == WCH_UPF_SPD_HI) {
            altbaud = 57600;
        }

        if (flags == WCH_UPF_SPD_VHI) {
            altbaud = 115200;
        }

        if (flags == WCH_UPF_SPD_SHI) {
            altbaud = 230400;
        }

        if (flags == WCH_UPF_SPD_WARP) {
            altbaud = 460800;
        }
    }

    for (try = 0; try < 2; try++) {
        baud = tty_termios_baud_rate(termios);

        if (try == 0 && baud == 38400)
            baud = altbaud;

        if (baud == 0) {
            hung_up = 1;
            baud = 9600;
        }

        if (baud >= min && baud <= max) {
            return baud;
        }

        termios->c_cflag &= ~CBAUD;
        if (old) {
            baud = tty_termios_baud_rate(old);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
            if (!hung_up)
                tty_termios_encode_baud_rate(termios, baud, baud);
#endif
            old = NULL;
            continue;
        }
        /*
         * As a last resort, if the range cannot be met then clip to
         * the nearest chip supported rate.
         */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
        if (!hung_up) {
            if (baud <= min)
                tty_termios_encode_baud_rate(termios, min + 1, min + 1);
            else
                tty_termios_encode_baud_rate(termios, max - 1, max - 1);
        }
#endif
    }

    return 0;
}

static void ser_change_speed(struct ser_state *state, struct WCHTERMIOS *old_termios)
{
    struct tty_struct *tty = state->info->tty;
    struct ser_port *port = state->port;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
    struct WCHTERMIOS *termios;
    if (!tty || !tty->termios || port->type == PORT_UNKNOWN) {
        return;
    }

    termios = tty->termios;

    if (termios->c_cflag & CRTSCTS) {
        state->info->flags |= WCH_UIF_CTS_FLOW;
    } else {
        state->info->flags &= ~WCH_UIF_CTS_FLOW;
    }

    if (termios->c_cflag & CLOCAL) {
        state->info->flags &= ~WCH_UIF_CHECK_CD;
    } else {
        state->info->flags |= WCH_UIF_CHECK_CD;
    }
    wch_ser_set_termios(port, termios, old_termios);
#else
    struct WCHTERMIOS termios;
    if (!tty || port->type == PORT_UNKNOWN) {
        return;
    }
    termios = tty->termios;
    if (termios.c_cflag & CRTSCTS) {
        state->info->flags |= WCH_UIF_CTS_FLOW;
    } else {
        state->info->flags &= ~WCH_UIF_CTS_FLOW;
    }

    if (termios.c_cflag & CLOCAL) {
        state->info->flags &= ~WCH_UIF_CHECK_CD;
    } else {
        state->info->flags |= WCH_UIF_CHECK_CD;
    }
    wch_ser_set_termios(port, &termios, old_termios);
#endif
}

static void ser_set_termios(struct tty_struct *tty, struct WCHTERMIOS *old_termios)
{
    struct ser_state *state;
    unsigned long flags;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
    unsigned int cflag = tty->termios->c_cflag;
#else
    unsigned int cflag = tty->termios.c_cflag;
#endif
    int line = WCH_SER_DEVNUM(tty);
    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK))

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
    if ((cflag ^ old_termios->c_cflag) == 0 && RELEVANT_IFLAG(tty->termios->c_iflag ^ old_termios->c_iflag) == 0)
#else
    if ((cflag ^ old_termios->c_cflag) == 0 && RELEVANT_IFLAG(tty->termios.c_iflag ^ old_termios->c_iflag) == 0)
#endif
    {
        return;
    }

    ser_change_speed(state, old_termios);

    /* Drop RTS and DTR */
    if ((cflag & CBAUD) == B0) {
        clear_mctrl(state->port, TIOCM_RTS | TIOCM_DTR);
    } else {
        /* Ensure RTS and DTR are raised when baudrate changed from 0 */
        if (old_termios && ((old_termios->c_cflag & CBAUD) == B0)) {
            unsigned int mask = TIOCM_DTR;
            if (!(cflag & CRTSCTS) || !test_bit(TTY_THROTTLED, &tty->flags)) {
                mask |= TIOCM_RTS;
            }
            set_mctrl(state->port, mask);
        }
    }

    if ((old_termios->c_cflag & CRTSCTS) && !(cflag & CRTSCTS)) {
        spin_lock_irqsave(&state->port->lock, flags);
        tty->hw_stopped = 0;
        _ser_start(tty);
        spin_unlock_irqrestore(&state->port->lock, flags);
    }
}

static void ser_update_termios(struct ser_state *state)
{
    struct tty_struct *tty = state->info->tty;
    struct ser_port *port = state->port;

    if (!(tty->flags & (1 << TTY_IO_ERROR))) {
        ser_change_speed(state, NULL);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
        if (tty->termios->c_cflag & CBAUD)
#else
        if (tty->termios.c_cflag & CBAUD)
#endif
        {
            set_mctrl(port, TIOCM_DTR | TIOCM_RTS);
        }
    }
}

static void ser_update_timeout(struct ser_port *port, unsigned int cflag, unsigned int baud)
{
    unsigned int bits;

    switch (cflag & CSIZE) {
    case CS5:
        bits = 7;
        break;

    case CS6:
        bits = 8;
        break;

    case CS7:
        bits = 9;
        break;

    default:
        bits = 10;
        break;
    }

    if (cflag & CSTOPB) {
        bits++;
    }

    if (cflag & PARENB) {
        bits++;
    }

    bits = bits * port->fifosize;

    port->timeout = (HZ * bits) / baud + HZ / 50;
}

static struct ser_state *ser_get(struct ser_driver *drv, int line)
{
    struct ser_state *state = NULL;

    down(&ser_port_sem);

    state = drv->state + line;

    if (down_interruptible(&state->sem)) {
        state = ERR_PTR(-ERESTARTSYS);
        goto out;
    }

    state->count++;

    if (!state->port) {
        state->count--;
        up(&state->sem);
        state = ERR_PTR(-ENXIO);
        goto out;
    }

    if (!state->port->iobase) {
        state->count--;
        up(&state->sem);
        state = ERR_PTR(-ENXIO);
        goto out;
    }

    if (!state->info) {
        state->info = kmalloc(sizeof(struct ser_info), GFP_KERNEL);

        if (state->info) {
            memset(state->info, 0, sizeof(struct ser_info));
            init_waitqueue_head(&state->info->open_wait);
            init_waitqueue_head(&state->info->delta_msr_wait);

            state->port->info = state->info;

            tasklet_init(&state->info->tlet, ser_tasklet_action, (unsigned long)state);
        } else {
            state->count--;
            up(&state->sem);
            state = ERR_PTR(-ENOMEM);
        }
    }

out:
    up(&ser_port_sem);
    return state;
}

static int ser_block_til_ready(struct file *filp, struct ser_state *state)
{
    DECLARE_WAITQUEUE(wait, current);
    struct ser_info *info = state->info;
    struct ser_port *port = state->port;

    info->blocked_open++;
    state->count--;

    add_wait_queue(&info->open_wait, &wait);

    while (1) {
        set_current_state(TASK_INTERRUPTIBLE);

        if (tty_hung_up_p(filp) || info->tty == NULL) {
            break;
        }

        if (!(info->flags & WCH_UIF_INITIALIZED)) {
            break;
        }

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
        if ((filp->f_flags & O_NONBLOCK) || (info->tty->termios->c_cflag & CLOCAL) ||
            (info->tty->flags & (1 << TTY_IO_ERROR)))
#else
        if ((filp->f_flags & O_NONBLOCK) || (info->tty->termios.c_cflag & CLOCAL) ||
            (info->tty->flags & (1 << TTY_IO_ERROR)))
#endif
        {
            break;
        }

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
        if (info->tty->termios->c_cflag & CBAUD)
#else
        if (info->tty->termios.c_cflag & CBAUD)
#endif
        {
            set_mctrl(port, TIOCM_DTR);
        }

        if (wch_ser_get_mctrl(port) & TIOCM_CAR) {
            break;
        }

        up(&state->sem);
        schedule();
        down(&state->sem);

        if (signal_pending(current)) {
            break;
        }
    }

    set_current_state(TASK_RUNNING);
    remove_wait_queue(&info->open_wait, &wait);

    state->count++;
    info->blocked_open--;

    if (signal_pending(current)) {
        return -ERESTARTSYS;
    }

    if (!info->tty || tty_hung_up_p(filp)) {
        return -EAGAIN;
    }

    return 0;
}

static void ser_wait_until_sent(struct tty_struct *tty, int timeout)
{
    struct ser_state *state = NULL;
    struct ser_port *port = NULL;
    unsigned long char_time;
    unsigned long expire;
    int line = WCH_SER_DEVNUM(tty);
    if (line >= wch_ser_port_total_cnt) {
        return;
    }

    state = tty->driver_data;
    port = state->port;

    if (port->type == PORT_UNKNOWN || port->fifosize == 0) {
        return;
    }

    char_time = (port->timeout - HZ / 50) / port->fifosize;

    char_time = char_time / 5;

    if (char_time == 0) {
        char_time = 1;
    }

    if (timeout && timeout < char_time) {
        char_time = timeout;
    }

    if (timeout == 0 || timeout > 2 * port->timeout) {
        timeout = 2 * port->timeout;
    }

    expire = jiffies + timeout;

    while (!wch_ser_tx_empty(port)) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(char_time);

        if (signal_pending(current)) {
            break;
        }

        if (time_after(jiffies, expire)) {
            break;
        }
    }
    set_current_state(TASK_RUNNING);
}

static int ser_open(struct tty_struct *tty, struct file *filp)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    struct ser_driver *drv = (struct ser_driver *)tty->driver->driver_state;
#else
    struct ser_driver *drv = (struct ser_driver *)tty->driver.driver_state;
#endif
    struct ser_state *state;
    int retval = 0;
    int line = WCH_SER_DEVNUM(tty);

    if (line < wch_ser_port_total_cnt) {
        retval = -ENODEV;

        if (line >= wch_ser_port_total_cnt) {
            goto fail;
        }

        state = ser_get(drv, line);
        if (IS_ERR(state)) {
            retval = PTR_ERR(state);
            goto fail;
        }

        if (!state) {
            goto fail;
        }
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 13))
        tty->low_latency = (state->port->flags & WCH_UPF_LOW_LATENCY) ? 1 : 0;
        tty->alt_speed = 0;
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 12, 0))
        state->port->state = state;
        state->port0.low_latency = (state->port->flags & WCH_UPF_LOW_LATENCY) ? 1 : 0;
#else
        state->port->state = state;
#endif
        tty->driver_data = state;
        state->info->tty = tty;

        if (tty_hung_up_p(filp)) {
            retval = -EAGAIN;
            state->count--;
            up(&state->sem);
            goto fail;
        }

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
        tty_port_tty_set(&state->port0, tty);  // 20140606 add
#endif
        retval = ser_startup(state, 0);

        if (retval == 0) {
            retval = ser_block_til_ready(filp, state);
        }

        up(&state->sem);

        if (retval == 0 && !(state->info->flags & WCH_UIF_NORMAL_ACTIVE)) {
            state->info->flags |= WCH_UIF_NORMAL_ACTIVE;

            ser_update_termios(state);
        }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        try_module_get(THIS_MODULE);
#else
        MOD_INC_USE_COUNT;
#endif
    }

fail:
    return retval;
}

static void ser_close(struct tty_struct *tty, struct file *filp)
{
    struct ser_state *state = tty->driver_data;
    struct ser_port *port;
    int line = WCH_SER_DEVNUM(tty);
    if (line < wch_ser_port_total_cnt) {
        if (!state || !state->port) {
            return;
        }

        port = state->port;

        down(&state->sem);
        if (tty_hung_up_p(filp)) {
            goto done;
        }
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        if ((tty->count == 1) && (state->count != 1)) {
            printk("WCH Info : bad serial port count; tty->count is 1, state->count is %d\n", state->count);
            state->count = 1;
        }
#endif

        if (--state->count < 0) {
            printk("WCH Info : bad serial port count for ttyWCH%d: %d\n", port->line, state->count);
            state->count = 0;
        }

        if (state->count) {
            goto done;
        }

        tty->closing = 1;

        if (state->closing_wait != WCH_USF_CLOSING_WAIT_NONE) {
            tty_wait_until_sent(tty, state->closing_wait);
        }

        if (state->info->flags & WCH_UIF_INITIALIZED) {
            unsigned long flags;
            spin_lock_irqsave(&port->lock, flags);
            wch_ser_stop_rx(port);
            spin_unlock_irqrestore(&port->lock, flags);

            ser_wait_until_sent(tty, port->timeout);
        }

        ser_shutdown(state);
        ser_flush_buffer(tty);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31))

        tty_ldisc_flush(tty);
        /*
        if (tty->ldisc->ops->flush_buffer) {
    tty->ldisc->ops->flush_buffer(tty);
}
        */
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27) && LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 30))
        {
            if (tty->ldisc.ops->flush_buffer) {
                tty->ldisc.ops->flush_buffer(tty);
            }
        }
#else

        if (tty->ldisc.flush_buffer) {
            tty->ldisc.flush_buffer(tty);
        }

#endif

        tty->closing = 0;
        if (state->info->tty)
            state->info->tty = NULL;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
        if (state->port0.tty)
            tty_port_tty_set(&state->port0, NULL);  // 20140606 add
#endif
        if (state->info->blocked_open) {
            if (state->close_delay) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(state->close_delay);
            }
        }

        state->info->flags &= ~WCH_UIF_NORMAL_ACTIVE;
        wake_up_interruptible(&state->info->open_wait);
    done:
        up(&state->sem);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        module_put(THIS_MODULE);
#else
        MOD_DEC_USE_COUNT;
#endif
    }
}

static void wch_ser_set_mctrl(struct ser_port *port, unsigned int mctrl)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    unsigned char mcr = 0;

    if (mctrl & TIOCM_RTS) {
        mcr |= UART_MCR_RTS;
    }

    if (mctrl & TIOCM_DTR) {
        mcr |= UART_MCR_DTR;
    }

    if (mctrl & TIOCM_OUT1) {
        mcr |= UART_MCR_OUT1;
    }

    if (mctrl & TIOCM_OUT2) {
        mcr |= UART_MCR_OUT2;
    }

    if (mctrl & TIOCM_LOOP) {
        mcr |= UART_MCR_LOOP;
    }

    mcr = (mcr & sp->mcr_mask) | sp->mcr_force | sp->mcr;

    WRITE_UART_MCR(sp, mcr);
}

static unsigned int wch_ser_tx_empty(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    unsigned long flags;
    unsigned int ret;
    spin_lock_irqsave(&sp->port.lock, flags);
    ret = READ_UART_LSR(sp) & UART_LSR_TEMT ? TIOCSER_TEMT : 0;
    spin_unlock_irqrestore(&sp->port.lock, flags);

    return ret;
}

static unsigned int wch_ser_get_mctrl(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    unsigned long flags;
    unsigned char status;
    unsigned int ret = 0;
    spin_lock_irqsave(&sp->port.lock, flags);
    status = READ_UART_MSR(sp);
    spin_unlock_irqrestore(&sp->port.lock, flags);

    if (status & UART_MSR_DCD) {
        ret |= TIOCM_CAR;
    }

    if (status & UART_MSR_RI) {
        ret |= TIOCM_RNG;
    }

    if (status & UART_MSR_DSR) {
        ret |= TIOCM_DSR;
    }

    if (status & UART_MSR_CTS) {
        ret |= TIOCM_CTS;
    }

    return ret;
}

static void wch_ser_stop_tx(struct ser_port *port, unsigned int tty_stop)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    if (sp->ier & UART_IER_THRI) {
        sp->ier &= ~UART_IER_THRI;
        WRITE_UART_IER(sp, sp->ier);
    }
}

static void wch_ser_start_tx(struct ser_port *port, unsigned int tty_start)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    if (!(sp->ier & UART_IER_THRI)) {
        sp->ier |= UART_IER_THRI;
        WRITE_UART_IER(sp, sp->ier);
    }
}

static void wch_ser_stop_rx(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    sp->ier &= ~UART_IER_RLSI;
    sp->port.read_status_mask &= ~UART_LSR_DR;
    WRITE_UART_IER(sp, sp->ier);
}

static void wch_ser_enable_ms(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    sp->ier |= UART_IER_MSI;
    WRITE_UART_IER(sp, sp->ier);
}

static void wch_ser_break_ctl(struct ser_port *port, int break_state)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    unsigned long flags;
    spin_lock_irqsave(&sp->port.lock, flags);

    if (break_state == -1) {
        sp->lcr |= UART_LCR_SBC;
    } else {
        sp->lcr &= ~UART_LCR_SBC;
    }

    WRITE_UART_LCR(sp, sp->lcr);
    spin_unlock_irqrestore(&sp->port.lock, flags);
}

static int wch_ser_startup(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;

    sp->capabilities = wch_uart_config[sp->port.type].flags;
    sp->mcr = 0;

    if (sp->capabilities & UART_CLEAR_FIFO) {
        WRITE_UART_FCR(sp, UART_FCR_ENABLE_FIFO);
        WRITE_UART_FCR(sp, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
        WRITE_UART_FCR(sp, 0);
    }

    (void)READ_UART_LSR(sp);
    (void)READ_UART_RX(sp);
    (void)READ_UART_IIR(sp);
    (void)READ_UART_MSR(sp);

    if (!(sp->port.flags & WCH_UPF_BUGGY_UART) && (READ_UART_LSR(sp) == 0xff)) {
        printk("WCH Info : ttyWCH%d: LSR safety check engaged!\n", sp->port.line);
        return -ENODEV;
    }

    WRITE_UART_LCR(sp, UART_LCR_WLEN8);

    sp->ier = UART_IER_RLSI | UART_IER_RDI;

    WRITE_UART_IER(sp, sp->ier);

    (void)READ_UART_LSR(sp);
    (void)READ_UART_RX(sp);
    (void)READ_UART_IIR(sp);
    (void)READ_UART_MSR(sp);
    return 0;
}

static void wch_ser_shutdown(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;

    sp->ier = 0;
    WRITE_UART_IER(sp, 0);

    WRITE_UART_LCR(sp, READ_UART_LCR(sp) & ~UART_LCR_SBC);

    WRITE_UART_FCR(sp, UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
    WRITE_UART_FCR(sp, 0);

    (void)READ_UART_RX(sp);
}

static unsigned int wch_ser_get_divisor(struct ser_port *port, unsigned int baud, bool btwicefreq)
{
    unsigned int quot;

    if ((port->flags & WCH_UPF_MAGIC_MULTIPLIER) && baud == (port->uartclk / 4)) {
        quot = 0x8001;
    } else if ((port->flags & WCH_UPF_MAGIC_MULTIPLIER) && baud == (port->uartclk / 8)) {
        quot = 0x8002;
    } else {
        quot = ser_get_divisor(port, baud, btwicefreq);
    }

    return quot;
}

static void wch_ser_set_termios(struct ser_port *port, struct WCHTERMIOS *termios, struct WCHTERMIOS *old)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    unsigned char cval;
    unsigned char fcr = 0;
    unsigned long flags;
    unsigned int baud;
    unsigned int quot;
    unsigned char ier;

    switch (termios->c_cflag & CSIZE) {
    case CS5:
        cval = 0x00;
        break;

    case CS6:
        cval = 0x01;
        break;

    case CS7:
        cval = 0x02;
        break;

    default:
    case CS8:
        cval = 0x03;
        break;
    }

    if (termios->c_cflag & CSTOPB) {
        cval |= 0x04;
    }

    if (termios->c_cflag & PARENB) {
        cval |= UART_LCR_PARITY;
    }

    if (!(termios->c_cflag & PARODD)) {
        cval |= UART_LCR_EPAR;
    }

#ifdef CMSPAR
    if (termios->c_cflag & CMSPAR) {
        cval |= UART_LCR_SPAR;
    }
#endif

    /* fixed on 20210706 */
    if (port->bspe1stport == true) {
        baud = ser_get_baud_rate(port, termios, old, port->uartclk / 16 / 65536, port->uartclk / 16);
        if (baud <= 0)
            baud = 9600;
        quot = wch_ser_get_divisor(port, baud, true);
        sp->ier &= ~(1 << 5);
        ier = READ_UART_IER(sp + 1);
        WRITE_UART_IER(sp + 1, ier | (1 << 5));
    } else if (port->bext1stport == false) {
        baud = ser_get_baud_rate(port, termios, old, port->uartclk / 16 / 65536, port->uartclk / 16);
        if (baud <= 0)
            baud = 9600;
        quot = wch_ser_get_divisor(port, baud, true);
        sp->ier |= 1 << 5;
    } else {
        baud = ser_get_baud_rate(port, termios, old, port->uartclk / 24 / 16 / 65536, port->uartclk / 24 / 16);
        if (baud <= 0)
            baud = 9600;
        quot = wch_ser_get_divisor(port, baud, false);
        sp->ier &= ~(1 << 5);
    }

    if (sp->capabilities & UART_USE_FIFO) {
        fcr = UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10;
    }

    sp->mcr &= ~UART_MCR_AFE;

    if (termios->c_cflag & CRTSCTS) {
        sp->mcr |= UART_MCR_AFE;
        sp->mcr |= UART_MCR_RTS;
        port->hardflow = true;
    } else {
        sp->mcr &= ~UART_MCR_AFE;
        sp->mcr &= ~UART_MCR_RTS;
        port->hardflow = false;
    }

    /* added on 20200928 */
    sp->mcr |= UART_MCR_OUT2;

    spin_lock_irqsave(&sp->port.lock, flags);

    ser_update_timeout(port, termios->c_cflag, baud);

    sp->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;

    if (termios->c_iflag & INPCK) {
        sp->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
    }

    if (termios->c_iflag & (BRKINT | PARMRK)) {
        sp->port.read_status_mask |= UART_LSR_BI;
    }

    sp->port.ignore_status_mask = 0;

    if (termios->c_iflag & IGNPAR) {
        sp->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
    }

    if (termios->c_iflag & IGNBRK) {
        sp->port.ignore_status_mask |= UART_LSR_BI;

        if (termios->c_iflag & IGNPAR) {
            sp->port.ignore_status_mask |= UART_LSR_OE;
        }
    }

    if ((termios->c_cflag & CREAD) == 0) {
        sp->port.ignore_status_mask |= UART_LSR_DR;
    }

    sp->ier &= ~UART_IER_MSI;
    if (WCH_ENABLE_MS(&sp->port, termios->c_cflag)) {
        sp->ier |= UART_IER_MSI;
    }

    WRITE_UART_LCR(sp, cval | UART_LCR_DLAB);

    WRITE_UART_DLL(sp, quot & 0xff);
    WRITE_UART_DLM(sp, quot >> 8);

    WRITE_UART_FCR(sp, fcr);

    WRITE_UART_LCR(sp, cval);

    sp->lcr = cval;

    wch_ser_set_mctrl(&sp->port, sp->port.mctrl);

    WRITE_UART_IER(sp, sp->ier);

    spin_unlock_irqrestore(&sp->port.lock, flags);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
static void wch_ser_timeout(unsigned long data)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)data;
#else
static void wch_ser_timeout(struct timer_list *t)
{
    struct wch_ser_port *sp = from_timer(sp, t, timer);
#endif
    unsigned int timeout;
    unsigned int iir;
    iir = READ_UART_IIR(sp);

    if (!(iir & UART_IIR_NO_INT)) {
        spin_lock(&sp->port.lock);
        ser_handle_port(sp, iir);
        spin_unlock(&sp->port.lock);
    }

    timeout = sp->port.timeout;
    timeout = timeout > 6 ? (timeout / 2 - 2) : 1;

    mod_timer(&sp->timer, jiffies + timeout);
}

static _INLINE_ void ser_receive_chars(struct wch_ser_port *sp, unsigned char *status, unsigned char iir)
{
    struct tty_struct *tty = sp->port.info->tty;
    unsigned char ch = 0;
    int max_count = 256;
    unsigned char lsr = *status;
    unsigned char flag;

    unsigned char rbuf[256];
    int readcont = 0;
    int count;

    do {
        if (iir == UART_IIR_RDI) {
            READ_UART_RX_BUFFER(sp, rbuf, sp->port.rx_trigger);
            sp->port.icount.rx += sp->port.rx_trigger;
            count = sp->port.rx_trigger;
            readcont = 1;
            iir = 0;
        } else {
            ch = READ_UART_RX(sp);
            sp->port.icount.rx++;
            readcont = 0;
        }
        flag = TTY_NORMAL;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 4, 18))
        if (unlikely(lsr & (UART_LSR_BI | UART_LSR_PE | UART_LSR_FE | UART_LSR_OE)))
#else
        if (lsr & (UART_LSR_BI | UART_LSR_PE | UART_LSR_FE | UART_LSR_OE))
#endif
        {
            if (lsr & UART_LSR_BI) {
                lsr &= ~(UART_LSR_FE | UART_LSR_PE);
                sp->port.icount.brk++;

                if (ser_handle_break(&sp->port)) {
                    goto ignore_char;
                }
            } else if (lsr & UART_LSR_PE) {
                sp->port.icount.parity++;
            } else if (lsr & UART_LSR_FE) {
                sp->port.icount.frame++;
            }

            if (lsr & UART_LSR_OE) {
                sp->port.icount.overrun++;
            }

            lsr &= sp->port.read_status_mask;

            if (lsr & UART_LSR_BI) {
                flag = TTY_BREAK;
            } else if (lsr & UART_LSR_PE) {
                flag = TTY_PARITY;
            } else if (lsr & UART_LSR_FE) {
                flag = TTY_FRAME;
            }
        }

        if ((I_IXOFF(tty)) || I_IXON(tty)) {
            if (ch == START_CHAR(tty)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0))
                tty->flow.stopped = 0;
#else
                tty->stopped = 0;
#endif
                wch_ser_start_tx(&sp->port, 1);
                goto ignore_char;
            } else if (ch == STOP_CHAR(tty)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0))
                tty->flow.stopped = 1;
#else
                tty->stopped = 1;
#endif
                wch_ser_stop_tx(&sp->port, 1);
                goto ignore_char;
            }
        }

        if (readcont)
            ser_insert_buffer(&sp->port, lsr, UART_LSR_OE, rbuf, count, flag);
        else
            ser_insert_char(&sp->port, lsr, UART_LSR_OE, ch, flag);

    ignore_char:
        lsr = READ_UART_LSR(sp);

        if (lsr == 0xff) {
            lsr = 0x01;
        }

    } while (lsr & (UART_LSR_DR | UART_LSR_BI) && (max_count-- > 0));

    spin_unlock(&sp->port.lock);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 13))
    tty_flip_buffer_push(tty);
#else
    tty_flip_buffer_push(&(sp->port.state->port0));
#endif
    spin_lock(&sp->port.lock);
    *status = lsr;
}

static _INLINE_ void ser_transmit_chars(struct wch_ser_port *sp)
{
    struct circ_buf *xmit = &sp->port.info->xmit;
    int count;

    if ((!sp) || (!sp->port.iobase)) {
        return;
    }

    if (!sp->port.info) {
        return;
    }

    if (!xmit) {
        return;
    }

    if (sp->port.x_char) {
        WRITE_UART_TX(sp, sp->port.x_char);
        sp->port.icount.tx++;
        sp->port.x_char = 0;
        return;
    }

    if (ser_circ_empty(xmit) || ser_tx_stopped(&sp->port)) {
        wch_ser_stop_tx(&sp->port, 0);
        return;
    }

    count = sp->port.fifosize / 2;

    do {
        WRITE_UART_TX(sp, xmit->buf[xmit->tail]);
        xmit->tail = (xmit->tail + 1) & (WCH_UART_XMIT_SIZE - 1);
        sp->port.icount.tx++;

        if (ser_circ_empty(xmit)) {
            break;
        }

    } while (--count > 0);

    if (ser_circ_chars_pending(xmit) < WAKEUP_CHARS) {
        ser_write_wakeup(&sp->port);
    }
    /*
        if (ser_circ_empty(xmit))
        {
            wch_ser_stop_tx(&sp->port, 0);
        }
    */
}

static _INLINE_ void ser_check_modem_status(struct wch_ser_port *sp, unsigned char status)
{
    if ((status & UART_MSR_ANY_DELTA) == 0) {
        return;
    }

    if (!sp->port.info) {
        return;
    }

    if (status & UART_MSR_TERI) {
        sp->port.icount.rng++;
    }

    if (status & UART_MSR_DDSR) {
        sp->port.icount.dsr++;
    }

    if (status & UART_MSR_DDCD) {
        ser_handle_dcd_change(&sp->port, status & UART_MSR_DCD);
    }

    if (status & UART_MSR_DCTS) {
        ser_handle_cts_change(&sp->port, status & UART_MSR_CTS);
    }

    wake_up_interruptible(&sp->port.info->delta_msr_wait);
}

static _INLINE_ void ser_handle_port(struct wch_ser_port *sp, unsigned char iir)
{
    unsigned char lsr = READ_UART_LSR(sp);
    unsigned char msr = 0;

    if (lsr == 0xff) {
        lsr = 0x01;
    }

    if ((iir == UART_IIR_RLSI) || (iir == UART_IIR_CTO) || (iir == UART_IIR_RDI)) {
        ser_receive_chars(sp, &lsr, iir);
    }

    if ((iir == UART_IIR_THRI) && (lsr & UART_LSR_THRE)) {
        ser_transmit_chars(sp);
    }

    msr = READ_UART_MSR(sp);

    if (msr & UART_MSR_ANY_DELTA) {
        ser_check_modem_status(sp, msr);
    }
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
static struct tty_operations wch_tty_ops = {
    .open = ser_open,
    .close = ser_close,
    .write = ser_write,
    .put_char = ser_put_char,
    .flush_chars = ser_flush_chars,
    .write_room = ser_write_room,
    .chars_in_buffer = ser_chars_in_buffer,
    .flush_buffer = ser_flush_buffer,
    .ioctl = ser_ioctl,
    .throttle = ser_throttle,
    .unthrottle = ser_unthrottle,
    .send_xchar = ser_send_xchar,
    .set_termios = ser_set_termios,
    .stop = ser_stop,
    .start = ser_start,
    .hangup = ser_hangup,
    .break_ctl = ser_break_ctl,
    .wait_until_sent = ser_wait_until_sent,
    .tiocmget = ser_tiocmget,
    .tiocmset = ser_tiocmset,
};
#endif

extern int wch_ser_register_driver(struct ser_driver *drv)
{
    struct tty_driver *normal = NULL;
    int i;
    int ret = 0;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    drv->nr = wch_ser_port_total_cnt;
    drv->state = kmalloc(sizeof(struct ser_state) * drv->nr, GFP_KERNEL);
    ret = -ENOMEM;

    if (!drv->state) {
        printk("WCH Error: Allocate memory fail !\n\n");
        goto out;
    }

    memset(drv->state, 0, sizeof(struct ser_state) * drv->nr);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
    normal = tty_alloc_driver(drv->nr, TTY_DRIVER_REAL_RAW);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    normal = alloc_tty_driver(drv->nr);
#else
    normal = &drv->tty_driver;
#endif
    if (!normal) {
        printk("WCH Error: Allocate tty driver fail !\n\n");
        goto out;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    drv->tty_driver = normal;
#endif
    normal->magic = TTY_DRIVER_MAGIC;
    normal->name = drv->dev_name;
    normal->major = drv->major;
    normal->minor_start = drv->minor;
    normal->num = wch_ser_port_total_cnt;
    normal->type = TTY_DRIVER_TYPE_SERIAL;
    normal->subtype = SERIAL_TYPE_NORMAL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
    normal->flags = TTY_DRIVER_REAL_RAW;
#endif
    normal->init_termios = tty_std_termios;
    normal->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    normal->init_termios.c_iflag = 0;

    normal->driver_state = drv;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    tty_set_operations(normal, &wch_tty_ops);
#else
    normal->refcount = &ser_refcount;
    normal->table = ser_tty;
    normal->termios = ser_termios;
    normal->termios_locked = ser_termios_locked;

    normal->open = ser_open;
    normal->close = ser_close;
    normal->write = ser_write;
    normal->put_char = ser_put_char;
    normal->flush_chars = ser_flush_chars;
    normal->write_room = ser_write_room;
    normal->chars_in_buffer = ser_chars_in_buffer;
    normal->flush_buffer = ser_flush_buffer;
    normal->ioctl = ser_ioctl;
    normal->throttle = ser_throttle;
    normal->unthrottle = ser_unthrottle;
    normal->send_xchar = ser_send_xchar;
    normal->set_termios = ser_set_termios;
    normal->stop = ser_stop;
    normal->start = ser_start;
    normal->hangup = ser_hangup;
    normal->break_ctl = ser_break_ctl;
    normal->wait_until_sent = ser_wait_until_sent;
#endif

    for (i = 0; i < drv->nr; i++) {
        struct ser_state *state = drv->state + i;
        if (!state) {
            ret = -1;
            printk("WCH Error: Memory error !\n\n");
            goto out;
        }

        state->close_delay = 5 * HZ / 100;
        state->closing_wait = 3 * HZ;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
        tty_port_init(&state->port0);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
        sema_init(&state->sem, 1);
#else
        init_MUTEX(&state->sem);
#endif
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
    kref_init(&normal->kref);
#endif
    ret = tty_register_driver(normal);
    if (ret < 0) {
        printk("WCH Error: Register tty driver fail !\n\n");
        goto out;
    }

out:
    if (ret < 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
        tty_driver_kref_put(normal);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        put_tty_driver(normal);
#endif
        kfree(drv->state);
    }

    return (ret);
}

extern void wch_ser_unregister_driver(struct ser_driver *drv)
{
    struct tty_driver *normal;
#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
    normal = drv->tty_driver;
    if (!normal) {
        return;
    }

    tty_unregister_driver(normal);
    tty_driver_kref_put(normal);
    drv->tty_driver = NULL;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
    normal = drv->tty_driver;
    if (!normal) {
        return;
    }

    tty_unregister_driver(normal);
    put_tty_driver(normal);
    drv->tty_driver = NULL;
#else
    normal = &drv->tty_driver;
    if (!normal) {
        return;
    }

    tty_unregister_driver(normal);
#endif
    if (drv->state) {
        kfree(drv->state);
    }
}

static void wch_ser_request_io(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;
    switch (sp->port.iotype) {
    case WCH_UPIO_PORT:
        request_region(sp->port.iobase, WCH_SER_ADDRESS_LENGTH, "wch_ser");
        break;
    case WCH_UPIO_MEM:
        break;
    default:
        break;
    }
}

static void wch_ser_configure_port(struct ser_driver *drv, struct ser_state *state, struct ser_port *port)
{
    unsigned long flags;
    if (!port->iobase) {
        return;
    }

    flags = WCH_UART_CONFIG_TYPE;

    if (port->type != PORT_UNKNOWN) {
        wch_ser_request_io(port);

        spin_lock_irqsave(&port->lock, flags);

        wch_ser_set_mctrl(port, 0);
        spin_unlock_irqrestore(&port->lock, flags);
    }
}

static int wch_ser_add_one_port(struct ser_driver *drv, struct ser_port *port)
{
    struct ser_state *state = NULL;

    int ret = 0;

    if (port->line >= drv->nr) {
        return -EINVAL;
    }

    state = drv->state + port->line;

    down(&ser_port_sem);

    if (state->port) {
        ret = -EINVAL;
        goto out;
    }

    state->port = port;

    port->info = state->info;
//    port->state = state;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
    drv->tty_driver->ports[port->line] = &state->port0;
#endif
    wch_ser_configure_port(drv, state, port);

out:
    up(&ser_port_sem);
    return ret;
}

extern int wch_ser_register_ports(struct ser_driver *drv)
{
    int i;
    int ret;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif
    for (i = 0; i < wch_ser_port_total_cnt; i++) {
        struct wch_ser_port *sp = &wch_ser_table[i];

        if (!sp) {
            return -1;
        }

        sp->port.line = i;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0))
        if (sp->port.iobase)
#endif
        {

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
            init_timer(&sp->timer);
            sp->timer.function = wch_ser_timeout;
#else
            timer_setup(&sp->timer, wch_ser_timeout, 0);
#endif

            sp->mcr_mask = ~0;
            sp->mcr_force = 0;

            ret = wch_ser_add_one_port(drv, &sp->port);

            if (ret != 0) {
                return ret;
            }
        }
        printk("Setup ttyWCH%d - PCIe port: port %lx, irq %d, type %d\n", sp->port.line, sp->port.iobase, sp->port.irq,
               sp->port.iotype);
    }

    return 0;
}

static void wch_ser_release_io(struct ser_port *port)
{
    struct wch_ser_port *sp = (struct wch_ser_port *)port;

    switch (sp->port.iotype) {
    case WCH_UPIO_PORT:
        release_region(sp->port.iobase, WCH_SER_ADDRESS_LENGTH);
        break;
    case WCH_UPIO_MEM:
        break;
    default:
        break;
    }
}

static void wch_ser_unconfigure_port(struct ser_driver *drv, struct ser_state *state)
{
    struct ser_port *port = state->port;
    struct ser_info *info = state->info;
    if (info && info->tty) {
        tty_hangup(info->tty);
    }

    down(&state->sem);

    state->info = NULL;

    if (port->type != PORT_UNKNOWN) {
        wch_ser_release_io(port);
    }

    port->type = PORT_UNKNOWN;

    if (info) {
        tasklet_kill(&info->tlet);
        kfree(info);
    }

    up(&state->sem);
}

static int wch_ser_remove_one_port(struct ser_driver *drv, struct ser_port *port)
{
    struct ser_state *state = drv->state + port->line;

    if (state->port != port) {
        printk("WCH Info : Removing wrong port: %p != %p\n\n", state->port, port);
    }

    down(&ser_port_sem);

    wch_ser_unconfigure_port(drv, state);

    state->port = NULL;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
    drv->tty_driver->ports[port->line] = NULL;
#endif
    up(&ser_port_sem);
    return 0;
}

extern void wch_ser_unregister_ports(struct ser_driver *drv)
{
    int i;

#if WCH_DBG
    printk("%s : %s\n", __FILE__, __FUNCTION__);
#endif

    for (i = 0; i < wch_ser_port_total_cnt; i++) {
        struct wch_ser_port *sp = &wch_ser_table[i];

        if (sp->port.iobase) {
            wch_ser_remove_one_port(drv, &sp->port);
        }
    }
}

extern int wch_ser_interrupt(struct wch_board *sb, struct wch_ser_port *first_sp)
{
    struct wch_ser_port *sp;
    int i;
    int max;
    unsigned long irqbits;
    unsigned char ch438irqbits;
    unsigned long bits;
    int pass_counter = 0;
    unsigned char iir;

    max = sb->ser_ports;

    if ((first_sp->port.port_flag & PORTFLAG_REMAP) == PORTFLAG_REMAP) {  // CH352_2S CH352_1S1P
        while (1) {
            for (i = 0; i < max; i++) {
                sp = first_sp + i;

                if (!sp->port.iobase) {
                    continue;
                }

                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }

            if (pass_counter++ > INTERRUPT_COUNT) {
                break;
            }
        }
    } else if ((first_sp->port.pb_info.vendor_id == VENDOR_ID_WCH_PCI) &&
               (first_sp->port.pb_info.device_id == DEVICE_ID_WCH_CH353_4S)) {  // CH353_4S
        while (1) {
            irqbits = READ_INTERRUPT_VECTOR_BYTE(first_sp) & first_sp->port.vector_mask;
            if (irqbits == 0x0000) {
                break;
            }

            for (i = 0, bits = 1; i < max; i++, bits <<= 1) {
                if (i == 0x02) {
                    bits <<= 2;
                }
                if (!(bits & irqbits)) {
                    continue;
                }

                sp = first_sp + i;

                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }

            if (pass_counter++ > INTERRUPT_COUNT) {
                break;
            }
        }
    } else if ((first_sp->port.pb_info.vendor_id == VENDOR_ID_WCH_PCI) &&
               (first_sp->port.pb_info.device_id == DEVICE_ID_WCH_CH359_16S)) {  // CH359_16S
        while (1) {
            irqbits = READ_INTERRUPT_VECTOR_WORD(first_sp) & first_sp->port.vector_mask;
            if (irqbits == 0x0000) {
                break;
            }

            for (i = 0, bits = 1; i < max; i++, bits <<= 1) {
                if (!(bits & irqbits)) {
                    continue;
                }

                sp = first_sp + i;

                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }

            if (pass_counter++ > INTERRUPT_COUNT) {
                break;
            }
        }
    } else if ((first_sp->port.pb_info.vendor_id == VENDOR_ID_WCH_PCIE &&
                first_sp->port.pb_info.device_id == DEVICE_ID_WCH_CH384_28S) ||
               (first_sp->port.pb_info.vendor_id == VENDOR_ID_WCH_PCIE &&
                first_sp->port.pb_info.device_id == DEVICE_ID_WCH_CH384_8S)) {  // CH384_8S CH384_28S
        while (1) {
            irqbits = READ_INTERRUPT_VECTOR_DWORD(first_sp) & first_sp->port.vector_mask;

            if ((irqbits & 0x80000000) != 0 && (irqbits & 0x40000000) != 0 && (irqbits & 0x20000000) != 0 &&
                (irqbits & 0x00000100) == 0 && (irqbits & 0x00000200) == 0 && (irqbits & 0x00000400) == 0 &&
                (irqbits & 0x00000800) == 0) {
                break;
            }

            /* handle CH438 #3 */
            if ((irqbits & 0x80000000) == 0) {
                sp = first_sp + 0x14;
                ch438irqbits = READ_INTERRUPT_VECTOR_BYTE(sp) & sp->port.vector_mask;
                if (ch438irqbits != 0) {
                    for (i = 0; i < 0x08; i++) {
                        if (ch438irqbits & (1 << i)) {
                            sp += i;
                            iir = READ_UART_IIR(sp) & 0x0f;
                            if (iir & UART_IIR_NO_INT) {
                                continue;
                            } else {
                                spin_lock(&sp->port.lock);
                                ser_handle_port(sp, iir);
                                spin_unlock(&sp->port.lock);
                            }
                        }
                    }
                }
            }
            /* handle CH438 #2 */
            if ((irqbits & 0x40000000) == 0) {
                sp = first_sp + 0x0C;
                ch438irqbits = READ_INTERRUPT_VECTOR_BYTE(sp) & sp->port.vector_mask;
                if (ch438irqbits != 0) {
                    for (i = 0; i < 0x08; i++) {
                        if (ch438irqbits & (1 << i)) {
                            sp += i;
                            iir = READ_UART_IIR(sp) & 0x0f;
                            if (iir & UART_IIR_NO_INT) {
                                continue;
                            } else {
                                spin_lock(&sp->port.lock);
                                ser_handle_port(sp, iir);
                                spin_unlock(&sp->port.lock);
                            }
                        }
                    }
                }
            }
            /* handle CH438 #1 */
            if ((irqbits & 0x20000000) == 0) {
                /* CH384_28S */
                if (first_sp->port.pb_info.device_id == DEVICE_ID_WCH_CH384_28S) {
                    sp = first_sp + 0x04;
                } else {
                    /* CH384_8S */
                    sp = first_sp;
                }
                ch438irqbits = READ_INTERRUPT_VECTOR_BYTE(sp) & sp->port.vector_mask;
                if (ch438irqbits != 0) {
                    for (i = 0; i < 0x08; i++) {
                        if (ch438irqbits & (1 << i)) {
                            sp += i;
                            iir = READ_UART_IIR(sp) & 0x0f;
                            if (iir & UART_IIR_NO_INT) {
                                continue;
                            } else {
                                spin_lock(&sp->port.lock);
                                ser_handle_port(sp, iir);
                                spin_unlock(&sp->port.lock);
                            }
                        }
                    }
                }
            }
            if ((irqbits & 0x00000100) == 0x00000100) {
                sp = first_sp;
                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }
            if ((irqbits & 0x00000200) == 0x00000200) {
                sp = first_sp + 0x01;
                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }
            if ((irqbits & 0x00000400) == 0x00000400) {
                sp = first_sp + 0x02;
                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }
            if ((irqbits & 0x00000800) == 0x00000800) {
                sp = first_sp + 0x03;
                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }

            if (pass_counter++ > INTERRUPT_COUNT) {
                break;
            }
        }
    } else if ((first_sp->port.pb_info.vendor_id == VENDOR_ID_WCH_PCI) &&
               (first_sp->port.pb_info.device_id == DEVICE_ID_WCH_CH365_32S)) {
        while (1) {
            if ((inb(first_sp->port.chip_iobase + 0xF8) & 0x04) != 0x04) {
                break;
            }

            irqbits = inb(first_sp->port.chip_iobase) & first_sp->port.vector_mask;
            if (irqbits == 0xFF) {
                break;
            }

            if ((irqbits & 0x00000010) == 0) {  // first ch438 irq
                for (i = 0, bits = 1; i < 0x08; i++, bits <<= 1) {
                    sp = first_sp + i;
                    ch438irqbits = readb(sp->port.board_membase + 0x100 + 0x4F) & sp->port.vector_mask;
                    if (ch438irqbits == 0x00000000) {
                        break;
                    }
                    if (!(bits & ch438irqbits)) {
                        continue;
                    } else {
                        break;
                    }
                }

                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
                outb(inb(sp->port.chip_iobase + 0xF8) & 0xFB, sp->port.chip_iobase + 0xF8);
            } else if ((irqbits & 0x00000020) == 0) {
                for (i = 0, bits = 1; i < 0x08; i++, bits <<= 1) {
                    sp = first_sp + i + 0x08;
                    ch438irqbits = readb(sp->port.board_membase + 0x180 + 0x4F) & sp->port.vector_mask;
                    if (ch438irqbits == 0x00000000) {
                        break;
                    }
                    if (!(bits & ch438irqbits)) {
                        continue;
                    } else {
                        break;
                    }
                }
                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
                outb(inb(sp->port.chip_iobase + 0xF8) & 0xFB, sp->port.chip_iobase + 0xF8);
            } else if ((irqbits & 0x00000040) == 0) {
                for (i = 0, bits = 1; i < 0x08; i++, bits <<= 1) {
                    sp = first_sp + i + 0x10;
                    ch438irqbits = readb(sp->port.board_membase + 0x200 + 0x4F) & sp->port.vector_mask;
                    if (ch438irqbits == 0x00000000) {
                        break;
                    }
                    if (!(bits & ch438irqbits)) {
                        continue;
                    } else {
                        break;
                    }
                }
                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
                outb(inb(sp->port.chip_iobase + 0xF8) & 0xFB, sp->port.chip_iobase + 0xF8);
            } else if ((irqbits & 0x00000080) == 0) {
                for (i = 0, bits = 1; i < 0x08; i++, bits <<= 1) {
                    sp = first_sp + i + 0x18;
                    ch438irqbits = readb(sp->port.board_membase + 0x280 + 0x4F) & sp->port.vector_mask;
                    if (ch438irqbits == 0x00000000) {
                        break;
                    }
                    if (!(bits & ch438irqbits)) {
                        continue;
                    } else {
                        break;
                    }
                }
                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
                outb(inb(sp->port.chip_iobase + 0xF8) & 0xFB, sp->port.chip_iobase + 0xF8);
            } else {
                break;
            }

            if (pass_counter++ > INTERRUPT_COUNT) {
                break;
            }
        }
    } else {  // CH353_2S1P CH353_2S1PAR CH355_4S CH356_4S1P CH356_8S CH358_4S1P CH358_8S CH382_2S1P CH384_4S1P
        while (1) {
            irqbits = READ_INTERRUPT_VECTOR_BYTE(first_sp) & first_sp->port.vector_mask;
            if (irqbits == 0x0000) {
                break;
            }

            for (i = 0, bits = 1; i < max; i++, bits <<= 1) {
                if (!(bits & irqbits)) {
                    continue;
                }

                sp = first_sp + i;

                iir = READ_UART_IIR(sp) & 0x0f;

                if (iir & UART_IIR_NO_INT) {
                    continue;
                } else {
                    spin_lock(&sp->port.lock);
                    ser_handle_port(sp, iir);
                    spin_unlock(&sp->port.lock);
                }
            }

            if (pass_counter++ > INTERRUPT_COUNT) {
                break;
            }
        }
    }
    return 0;
}
