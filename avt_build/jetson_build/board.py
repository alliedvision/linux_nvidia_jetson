import logging
from collections import namedtuple
from . import upstream
from . import build

AVT_RELEASE = "4.6.2~beta1"
KERNEL_RELEASE = "4.9.253-tegra"
L4T_VERSION = "32.7.2"

FileSet = namedtuple('FileSet', [
  'driver_package',
  #'rootfs',
  'public_sources'])


def get_tx2_agx_nx_upstream_files(UpstreamFile):
  driver_package  = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.2/t186/jetson_linux_r32.7.2_aarch64.tbz2",                     "2b98fa531414ed5bff38de0d4a8b10d272008a84c227bbf16a9082ce924afa70")
  #rootfs          = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/t186/tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2", "17996e861dd092043509e0b7e9ae038e271e5b0b7b78f26a34db4f03df2b12b8")
  public_sources  = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.2/sources/t186/public_sources.tbz2",                             "5ff4a46c6b85926450c456b0ee2d78f1801aab9c51b3f51b777c7c27ddf517fd")

  return FileSet(
    public_sources=public_sources,
    #rootfs=rootfs,
    driver_package=driver_package)

def get_nano_upstream_files(UpstreamFile):
  driver_package = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.2/t210/jetson-210_linux_r32.7.2_aarch64.tbz2",                   "3165351568f2b6677aceeb7c789bf986d8feb95fdec63eb3989f1c715b1fa795")
  #rootfs         = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/t210/tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2", "17996e861dd092043509e0b7e9ae038e271e5b0b7b78f26a34db4f03df2b12b8")
  public_sources = UpstreamFile("https://developer.nvidia.com/embedded/l4t/r32_release_v7.2/sources/t210/public_sources.tbz2",                             "4aaccdc4f51c969120ac9bcefe47522d519c7b00fe70a7c2d84bc95c77e56858")

  return FileSet(
    public_sources=public_sources,
    #rootfs=rootfs,
    driver_package=driver_package)
  
bootloader_payload_files_xavier = [
  ("bootloader/payloads_t18x/bl_only_payload", "opt/ota_package/t18x/bl_only_payload"),
  ("bootloader/payloads_t19x/bl_only_payload", "opt/ota_package/t19x/bl_only_payload"),
  ("bootloader/payloads_t18x/bl_update_payload", "opt/ota_package/t18x/bl_update_payload"),
  ("bootloader/payloads_t19x/bl_update_payload", "opt/ota_package/t19x/bl_update_payload")
]

bootloader_payload_files_tx2 = [
  ("bootloader/payloads_t18x/bl_update_payload", "opt/ota_package/t18x/bl_update_payload"),
  ("bootloader/payloads_t19x/bl_update_payload", "opt/ota_package/t19x/bl_update_payload")
]

kernel_extra_files_xavier = [
  ("bootloader/payloads_t18x/kernel_only_payload", "opt/ota_package/t18x/kernel_only_payload"),
  ("bootloader/payloads_t19x/kernel_only_payload", "opt/ota_package/t19x/kernel_only_payload")
]

kernel_extra_files_nano = []
bootloader_payload_files_nano = [
  ("bootloader/payloads_t21x/bl_update_payload", "opt/ota_package/t21x/bl_update_payload")
]

l4t_patches_xavier = [
  ("cboot_t194.bin", "bootloader")
]

class Board:
  def __init__(self, name, bundle_name, build_dir, files, dtb_filters, bups, kernel_extra, bootloader_payload, tools_version, l4t_patches):
    self.name = name
    self.bundle_name = bundle_name
    self.build_dir = build_dir
    self.files = files
    self.dtb_filters = dtb_filters
    self.bups = bups
    self.kernel_extra_files = kernel_extra
    self.bootloader_payload_files = bootloader_payload
    self.l4t_tools_version = tools_version
    self.l4t_patches = l4t_patches


known_boards = {
  'nano':   ("Nano, Nano 2GB", "nano", "nano", get_nano_upstream_files, ['tegra210'], ['t21x'],
             kernel_extra_files_nano, bootloader_payload_files_nano, "32.7.1-20220219090432", None),
  'xavier': ("AGX, NX, TX2", "xavier", None, get_tx2_agx_nx_upstream_files, ['tegra194', 'tegra186'], ['t19x', 't18x'],
             kernel_extra_files_xavier, bootloader_payload_files_xavier, "32.7.1-20220219090344", l4t_patches_xavier),
}


def boards(args):

  build_dir = build.build_dir(args)

  def mk_board(brd):
    return Board(brd[0], brd[2], build_dir(brd[1]), brd[3](UpstreamFile), brd[4], brd[5], brd[6], brd[7],brd[8],brd[9])

  UpstreamFile = upstream.upstream(args)
  return [mk_board(known_boards[board]) for board in args.boards]
  

def add_arguments(parser):
  parser.add_argument('-b', '--boards', choices=known_boards.keys(), nargs='+', help='Board model to build. Use \'nano\' for Nano and Nano 2GB, use \'xavier\' for AGX Xavier, Xavier NX and TX2.', metavar='BOARD', default=[])
