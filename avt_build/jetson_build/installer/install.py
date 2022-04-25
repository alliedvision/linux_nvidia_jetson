#!/usr/bin/env python3
import argparse
import curses
import os
import pathlib
import subprocess
import sys
import tarfile
import apt
import apt.progress
import apt_pkg
from apt.progress.base import AcquireProgress, InstallProgress, OpProgress
from apt_pkg import parse_commandline

KEY_SUBMIT = 10
KEY_ESC = 27

bootloader_conf_path = '/etc/nv_boot_control.conf'


class Board:
    def __init__(self, name, chip_id, board_id,  configurations):
        self.name = name
        self.chip_id = chip_id
        self.board_id = board_id
        self.configurations = configurations


class Configuration:
    def __init__(self, name, target_board, board_sku):
        self.name = name
        self.target_board = target_board
        self.board_sku = board_sku


tx2_devkit = Board('Jetson TX2 devkit', '0x18', '3310', [
    Configuration('2 cameras', 'jetson-tx2-devkit', '1000'),
    Configuration('6 cameras', 'jetson-tx2-devkit-6cam', '1000')
])

tx2_nx_devkit = Board('Jetson TX2 NX devkit', '0x18', '3636',  [
    Configuration('default', 'jetson-xavier-nx-devkit-tx2-nx', '0001'),
])

agx_devkit = Board('Jetson Xavier AGX devkit', '0x19', '2888', [
    Configuration('default', 'jetson-agx-xavier-devkit', '0001'),
    Configuration('default', 'jetson-agx-xavier-devkit', '0004'),
])

nx_devkit = Board('Jetson Xavier NX devkit', '0x19', '3668',  [
    Configuration('default', 'jetson-xavier-nx-devkit', '0000'),
    Configuration('default', 'jetson-xavier-nx-devkit-emmc', '0001'),
])

nano_devkit = Board('Jetson Nano', '0x21', '3448', [
    Configuration('default', 'jetson-nano-devkit', '0000'),
    Configuration('default', 'jetson-nano-2gb-devkit', '0001'),
])

boards = [
    tx2_devkit,
    tx2_nx_devkit,
    agx_devkit,
    nx_devkit,
    nano_devkit
]


class ProcessView:
    def __init__(self, stdscr, step_names):
        self.stdscr = stdscr
        self.step_names = step_names

        self.step_count = len(self.step_names)
        self.step_process = 0
        self.overall_process = 0
        self.current_step = 0

        (h, w) = stdscr.getmaxyx()

        self.process_width = w - 10

        self.step_process_label = stdscr.subwin(1, self.process_width + 1, 9, 5)
        self.step_process_win = stdscr.subwin(1, self.process_width + 1, 10, 5)
        self.step_process_win.attrset(curses.color_pair(2))
        self.step_process_win.bkgdset(' ', curses.color_pair(2))
        self.step_process_win.clear()
        self.step_process_win.touchwin()

        self.overall_process_label = stdscr.subwin(1, self.process_width + 1, 14, 5)
        self.overall_process_label.addstr(0, 0, self.step_names[self.current_step])
        self.overall_process_label.touchwin()
        self.overall_process_label.refresh()
        self.overall_process_win = stdscr.subwin(1, self.process_width + 1, 15, 5)
        self.overall_process_win.attrset(curses.color_pair(2))
        self.overall_process_win.bkgdset(' ', curses.color_pair(2))
        self.overall_process_win.clear()
        self.overall_process_win.touchwin()

        self.stdscr.touchwin()
        self.stdscr.refresh()

    def set_step_process(self, process, description = None):
        self.step_process = process
        self.step_process_label.clear()
        if description is not None:
            self.step_process_label.addstr(0, 0, description)
        self.step_process_label.touchwin()
        self.step_process_label.refresh()
        self.update()

    def next_step(self):
        self.current_step += 1
        self.overall_process_label.clear()
        self.step_process_label.clear()
        if self.current_step < self.step_count:
            self.overall_process_label.addstr(0, 0, self.step_names[self.current_step])
            self.step_process = 0
            self.overall_process = (self.current_step * 100) // self.step_count
        else:
            self.overall_process_label.addstr(0, 0, 'Installation complete. Press any key to exit')
            self.step_process_label.addstr(0, 0, 'Installation complete. Press any key to exit')
            self.step_process = 100
            self.overall_process = 100

        self.overall_process_label.touchwin()
        self.overall_process_label.refresh()
        self.step_process_label.touchwin()
        self.step_process_label.refresh()
        self.update()

    def update(self):
        step_process_chars = self.process_width * self.step_process // 100
        self.step_process_win.attron(curses.A_REVERSE)
        self.step_process_win.move(0, 0)
        self.step_process_win.clear()
        for i in range(int(step_process_chars)):
            self.step_process_win.addch(' ')

        self.step_process_win.attroff(curses.A_REVERSE)
        self.step_process_win.touchwin()
        overall_process_chars = self.process_width * self.overall_process // 100
        self.overall_process_win.attron(curses.A_REVERSE)
        self.overall_process_win.move(0, 0)
        for i in range(int(overall_process_chars)):
            self.overall_process_win.addch(' ')

        self.overall_process_win.attroff(curses.A_REVERSE)
        self.overall_process_win.touchwin()
        self.step_process_win.refresh()
        self.overall_process_win.refresh()


class FetchProcess(AcquireProgress):
    def __init__(self, process_view: ProcessView):
        super().__init__()
        self.process_view = process_view

    def done(self, item):
        self.process_view.set_step_process(self.current_items * 100 // self.total_items, item.description)

    def stop(self):
        self.process_view.next_step()


class InstallProcess(InstallProgress):
    def __init__(self, process_view: ProcessView):
        super().__init__()
        self.process_view = process_view

    def status_change(self, pkg, percent, status):
        self.process_view.set_step_process(int(percent), f'{status}')

    def fork(self):
        pid = os.fork()
        if pid == 0:
            logfd = os.open("install.log", os.O_RDWR | os.O_APPEND | os.O_CREAT, 0o644)
            os.dup2(logfd, 1)
            os.dup2(logfd, 2)
        return pid


def draw_centered_text(window, y, text: str,):
    (h, w) = window.getmaxyx()
    x_pos = w//2 - len(text)//2
    window.addstr(y, x_pos, text)


def main():
    if not os.geteuid() == 0:
        return os.system(f'sudo python3 {" ".join(sys.argv)}')

    logfd = os.open("install.log", os.O_RDWR | os.O_APPEND | os.O_CREAT, 0o644)

    parser = argparse.ArgumentParser()

    parser.add_argument('--board', choices=[
        conf.target_board for board in boards for conf in board.configurations
    ])

    args = parser.parse_args()

    stdscr = curses.initscr()
    curses.noecho()
    curses.curs_set(0)
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLUE)

    curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLACK)

    stdscr.keypad(True)
    stdscr.attrset(curses.color_pair(1))
    stdscr.bkgdset(' ', curses.color_pair(1))
    stdscr.clear()
    (h, w) = stdscr.getmaxyx()

    bootloader_conf_file = open(bootloader_conf_path, 'r')
    lines = bootloader_conf_file.readlines()
    bootloader_conf_file.close()

    chip_id = None
    board_id = None
    board_sku = None

    target_board = None

    for line in lines:
        if line.startswith('TNSPEC'):
            tns_spec = line.replace('TNSPEC ', '')
            tns_spec_parts = tns_spec.split('-')

            if len(tns_spec_parts) >= 6:
                board_id = tns_spec_parts[0].strip()
                board_sku = tns_spec_parts[2].strip()

            tns_spec_tail = tns_spec_parts.pop(len(tns_spec_parts) - 1)

            tns_spec_base = '-'.join([tns_spec_parts.pop(0) for i in range(6)])

            target_board = '-'.join(tns_spec_parts)
        elif line.startswith('COMPATIBLE_SPEC'):
            compatible_spec = line.replace('COMPATIBLE_SPEC ', '')
            compatible_spec_parts = compatible_spec.split('-')

            if len(compatible_spec_parts) >= 1:
                board_id = compatible_spec_parts[0].strip()

            compatible_spec_tail = compatible_spec_parts.pop(len(compatible_spec_parts) - 1)

            compatible_spec_base = '-'.join([compatible_spec_parts.pop(0) for i in range(6)])

            print(compatible_spec_base)

            target_board = '-'.join(compatible_spec_parts)
        elif line.startswith('TEGRA_CHIPID'):
            chip_id = line.replace('TEGRA_CHIPID ', '').strip()

    detected_boards = [b for b in boards if b.chip_id == chip_id and b.board_id == board_id]

    update_bootloader_conf = False

    if len(detected_boards) < 1:
        print(f"Board {board_id} not supported")
        exit(0)

    board = detected_boards[0]

    active_configurations = [conf for conf in board.configurations if conf.board_sku == board_sku]

    if len(active_configurations) < 1:
        print("Board {board_id} not supported")
        exit(0)

    selected = 0

    for i in range(len(active_configurations)):
        print(active_configurations[i].target_board)
        if active_configurations[i].target_board == target_board:
            selected = i
            update_bootloader_conf = True

    board_title = f'Current board {board.name}'
    title_start_w = w//2 - len(board_title)//2

    title_win = stdscr.subwin(3, len(board_title) + 2, 1, title_start_w - 1)
    title_win.box()
    title_win.move(1, 1)
    title_win.addstr(board_title)
    title_win.touchwin()


    key = None

    while key is not KEY_SUBMIT:
        if KEY_ESC == key:
            curses.echo()
            curses.curs_set(1)
            curses.endwin()
            exit(0)
        elif curses.KEY_UP == key:
            if selected > 0:
                selected -= 1
        elif curses.KEY_DOWN == key:
            if selected < len(active_configurations) - 1:
                selected += 1

        pos = 0

        for conf in active_configurations:
            start_w = w//2 - len(conf.name)//2

            if pos == selected:
                stdscr.addstr(5 + pos, start_w, conf.name, curses.A_REVERSE)
            else:
                stdscr.addstr(5 + pos, start_w, conf.name)
            pos += 1

        stdscr.touchwin()
        stdscr.refresh()
        key = stdscr.getch()

    stdscr.clear()

    process_view = ProcessView(stdscr, [
        "Extracting repository",
        "Repository setup",
        "Bootloader setup",
        "Update cache",
        "Download packages",
        "Install packages",
    ])

    packages_path = pathlib.PurePath('/opt/avt/packages')

    os.makedirs(packages_path,exist_ok=True)

    repo_package = tarfile.open('avt_l4t_repository.tar.xz')
    repo_package.extractall(path=packages_path)
    repo_package.close()

    process_view.next_step()

    subprocess.run(['apt-key', 'add', packages_path / 'KEY.gpg'], stdout=logfd, stderr=logfd)

    avt_sources_list = "/etc/apt/sources.list.d/avt-l4t-sources.list"
    if not os.path.exists(avt_sources_list):
        sources_list_file = open(avt_sources_list, 'wt')
        sources_list_file.write('deb file:/opt/avt/packages ./')
        sources_list_file.close()

    process_view.next_step()

    if update_bootloader_conf:
        new_data = ""
        for line in lines:
            if line.startswith('TNSPEC'):
                new_data += f'TNSPEC {tns_spec_base}-{active_configurations[selected].target_board}-{tns_spec_tail}'
            elif line.startswith('COMPATIBLE_SPEC'):
                new_data += f'COMPATIBLE_SPEC {compatible_spec_base}-{active_configurations[selected].target_board}-{compatible_spec_tail}'
            else:
                new_data += line

        bootloader_conf_file = open(bootloader_conf_path, 'wt')
        bootloader_conf_file.write(new_data)
        bootloader_conf_file.close()
        process_view.next_step()
    else:
        process_view.next_step()


    cache = apt.Cache(None)
    cache.update(FetchProcess(process_view))
    cache.open(None)
    bootloader_package = cache['avt-nvidia-l4t-bootloader']
    kernel_package = cache['avt-nvidia-l4t-kernel']
    dtb_package = cache['avt-nvidia-l4t-kernel-dtbs']
    headers_package = cache['avt-nvidia-l4t-kernel-headers']
    if bootloader_package.is_installed and not bootloader_package.is_upgradable:
        if update_bootloader_conf:
            subprocess.run(['dpkg-reconfigure', 'avt-nvidia-l4t-bootloader'], stdout=logfd, stderr=logfd)
    else:
        bootloader_package.mark_install()

    kernel_package.mark_install()
    dtb_package.mark_install()
    headers_package.mark_install()
    cache.commit(FetchProcess(process_view), InstallProcess(process_view), True)
    cache.close()
    process_view.next_step()
    stdscr.clear()
    msg = 'Installation complete. A restart is required!'
    dialog = stdscr.subwin(5, len(msg) + 2, h//2-2, w//2 - len(msg)//2)
    dialog.box()
    draw_centered_text(dialog, 1,msg)
    draw_centered_text(dialog, 2, 'Restart now ?')
    y_xpos = len(msg)//2 - 5
    n_xpos = len(msg)//2 + 5
    restart = True

    key = None

    while key != KEY_SUBMIT:
        if curses.KEY_RIGHT == key and restart:
            restart = False
        elif curses.KEY_LEFT == key and not restart:
            restart = True

        if restart:
            dialog.addstr(3, y_xpos, "Yes", curses.A_REVERSE)
            dialog.addstr(3, n_xpos, "No")
        else:
            dialog.addstr(3, y_xpos, "Yes")
            dialog.addstr(3, n_xpos, "No", curses.A_REVERSE)

        dialog.touchwin()
        dialog.refresh()
        key = stdscr.getch()

    curses.curs_set(1)
    curses.echo()
    curses.endwin()

    if restart:
        subprocess.run(['reboot'])


if __name__ == '__main__':
    main()
