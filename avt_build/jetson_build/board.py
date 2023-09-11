import logging
from collections import namedtuple
from . import upstream
from . import build

AVT_RELEASE = "5.1.2"
KERNEL_RELEASE = "5.10.120-tegra"
L4T_VERSION = "35.4.1"

FileSet = namedtuple('FileSet', [
  'driver_package',
  #'rootfs',
  'public_sources'])


def get_tx2_agx_nx_upstream_files(UpstreamFile):
  driver_package  = UpstreamFile("https://developer.download.nvidia.com/embedded/L4T/r35_Release_v4.1/release/Jetson_Linux_R35.4.1_aarch64.tbz2",                     "72b75a0c7fa3bf6ef41ae06634bb67c38a92682155d1206026dbee4a6b9a016f")
  #rootfs          = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/t186/tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2", "17996e861dd092043509e0b7e9ae038e271e5b0b7b78f26a34db4f03df2b12b8")
  public_sources  = UpstreamFile("https://developer.download.nvidia.com/embedded/L4T/r35_Release_v4.1/sources/public_sources.tbz2",                             "cad6179ae16cc23720dc019f3a36f054df5d153ce833f45cf1bd09a376f5442c")

  return FileSet(
    public_sources=public_sources,
    #rootfs=rootfs,
    driver_package=driver_package)

#def get_nano_upstream_files(UpstreamFile):
#  driver_package = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/t210/jetson-210_linux_r32.7.1_aarch64.tbz2",                   "7b6f4a698278226ae1d92661270c5441e16d01eafffb4bfb086de80b6964ae6f")
  #rootfs         = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/t210/tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2", "17996e861dd092043509e0b7e9ae038e271e5b0b7b78f26a34db4f03df2b12b8")
#  public_sources = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/sources/t210/public_sources.tbz2",                             "fdf5fb80fca25d107bb1545f9f8985237404b621d751b1c1ddc9a25c1b25cd21")

#  return FileSet(
#    public_sources=public_sources,
    #rootfs=rootfs,
#    driver_package=driver_package)
  
bootloader_payload_files_xavier = [
  ("bootloader/payloads_t19x/bl_only_payload", "opt/ota_package/t19x/bl_only_payload"),
  ("bootloader/payloads_t19x/TEGRA_BL.Cap", "opt/ota_package/t19x/TEGRA_BL.Cap"),
  ("bootloader/BOOTAA64.efi", "opt/ota_package/t19x/BOOTAA64.efi"),
  ("bootloader/payloads_t23x/bl_only_payload", "opt/ota_package/t23x/bl_only_payload"),
  ("bootloader/payloads_t23x/TEGRA_BL.Cap", "opt/ota_package/t23x/TEGRA_BL.Cap"),
  ("bootloader/BOOTAA64.efi", "opt/ota_package/t23x/BOOTAA64.efi")
]

bootloader_payload_files_tx2 = [
  ("bootloader/payloads_t18x/bl_update_payload", "opt/ota_package/t18x/bl_update_payload"),
  ("bootloader/payloads_t19x/bl_update_payload", "opt/ota_package/t19x/bl_update_payload")
]

kernel_extra_files_xavier = [
  ("bootloader/payloads_t19x/kernel_only_payload", "opt/ota_package/t19x/kernel_only_payload"),
  ("bootloader/payloads_t23x/kernel_only_payload", "opt/ota_package/t23x/kernel_only_payload")
]

kernel_extra_files_nano = []
bootloader_payload_files_nano = [
  ("bootloader/payloads_t21x/bl_update_payload", "opt/ota_package/t21x/bl_update_payload")
]


class Board:
  def __init__(self, name, bundle_name, build_dir, files, dtb_filters, bups, kernel_extra, bootloader_payload, tools_version):
    self.name = name
    self.bundle_name = bundle_name
    self.build_dir = build_dir
    self.files = files
    self.dtb_filters = dtb_filters
    self.bups = bups
    self.kernel_extra_files = kernel_extra
    self.bootloader_payload_files = bootloader_payload
    self.l4t_tools_version = tools_version


known_boards = {
  #'nano':   ("Nano, Nano 2GB", "nano", get_nano_upstream_files,       ['tegra210'],             ['t21x'], kernel_extra_files_nano,bootloader_payload_files_nano, "32.7.1-20220219090432"),
  'xavier': ("AGX, NX", "xavier", None, get_tx2_agx_nx_upstream_files, ['tegra194','tegra234'], ['t19x','t23x'], kernel_extra_files_xavier,bootloader_payload_files_xavier, "34.1.0-20220406120854"),
}


def boards(args):

  build_dir = build.build_dir(args)

  def mk_board(brd):
    return Board(brd[0], brd[2], build_dir(brd[1]), brd[3](UpstreamFile), brd[4], brd[5], brd[6], brd[7],brd[8])

  UpstreamFile = upstream.upstream(args)
  return [mk_board(known_boards[board]) for board in args.boards]
  

def add_arguments(parser):
  parser.add_argument('-b', '--boards', choices=known_boards.keys(), nargs='+', help='Board model to build. Use \'nano\' for Nano and Nano 2GB, use \'xavier\' for AGX Xavier, Xavier NX and TX2.', metavar='BOARD', default=[])
