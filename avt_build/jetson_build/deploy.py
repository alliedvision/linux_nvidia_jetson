import os
import logging
import shutil
import glob
import string
import sys
import tarfile

from . import common
from . import tools
from . import build

from pathlib import Path, PurePath
import re

from .board import KERNEL_RELEASE


def build_kernel_deb(args, board):
  t = tools.tools(args)
  logging.info("Building AVT Kernel Package")
  for subdir in ['out/usr/share/doc', 'out/boot', 'out/lib/modules'] + [f"out/{PurePath(e[1]).parent}" for e in board.kernel_extra_files]:
    os.makedirs(board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel/debian/{subdir}", exist_ok=True)

  file = open(common.common_dir(args) / 'kernel/include/config/kernel.release')
  kernel_release = file.read().strip()
  file.close()

  install_modules(args, board)
  t.execute(['sudo', 'rm', '-rf', board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian/out/lib/firmware"])
  t.execute(['sudo', 'rm', '-rf', board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel/debian/out/lib/modules/{kernel_release}/build"])
  t.execute(['sudo', 'rm', '-rf', board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel/debian/out/lib/modules/{kernel_release}/source"])

  shutil.copy(common.common_dir(args) / "kernel/arch/arm64/boot/Image", board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian/out/boot/Image")

  if 't19x' in board.bups:
    logging.info("Signing kernel image for t19x")
    shutil.copy(board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian/out/boot/Image", board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian/out/boot/Image.t19x")
    t.execute(['sudo',board.build_dir / f"Linux_for_Tegra/l4t_sign_image.sh",'--file','Image.t19x','--chip','0x19','--type','kernel'],cwd=board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian/out/boot/")

  if 't23x' in board.bups:
    logging.info("Adding display drivers")
    os.makedirs(board.build_dir / "Linux_for_Tegra/kernel/origin/display", exist_ok=True)
    t.execute(["tar","xf",board.build_dir / "Linux_for_Tegra/kernel/kernel_display_supplements.tbz2","-C",board.build_dir / "Linux_for_Tegra/kernel/origin/display"])
    t.execute(['sudo', 'cp', '-a', board.build_dir / f"Linux_for_Tegra/kernel/origin/display/lib/modules/{KERNEL_RELEASE}/extra", board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel/debian/out/lib/modules/{kernel_release}/extra"])


  for ef in board.kernel_extra_files:
    shutil.copy(board.build_dir / f"Linux_for_Tegra/{ef[0]}", board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel/debian/out/{ef[1]}")

  base_dir = Path(__file__).parent
  for f in ['rules', 'install', 'compat', 'source/format']:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    shutil.copy(base_dir / "files/kernel-deb" / f, dstfile)

  for f in ['control', 'changelog']:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    t.apply_template(base_dir / "files/kernel-deb" / f, dstfile, board)

  for f in ['postinst']:
    fin = open(base_dir / "files/kernel-deb" / f,"rt")
    data = ""
    for line in fin.readlines():
        data += line
    fin.close()
    fout = open(board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian" / f,"wt")
    fout.write(data.replace(KERNEL_RELEASE,kernel_release))
    fout.close()

  for f in ['triggers', 'postrm']:
    fin = open(board.build_dir / "Linux_for_Tegra/kernel/origin/kernel/debian" / f,"rt")
    data = ""
    for line in fin.readlines():
        if not line.startswith("nv-update-extlinux") and "modprobe" not in line:
            data += line
    fin.close()
    fout = open(board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian" / f,"wt")
    fout.write(data.replace(KERNEL_RELEASE,kernel_release))
    fout.close()



    #shutil.copy(board.build_dir / "Linux_for_Tegra/kernel/origin/kernel/debian" / f, board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian")



  logging.info("Building actual Debian package")
  env = { **os.environ, 'CC': 'aarch64-linux-gnu-gcc' }
  t.execute(['dpkg-buildpackage', '-uc', '-b', '-d', '-a', 'arm64'], cwd=board.build_dir / "Linux_for_Tegra/kernel/avt/kernel", env=env)


def build_dtb_deb(args, board):
  t = tools.tools(args)
  logging.info("Building AVT Device Tree Package")
  for subdir in ['out/usr/share/doc', 'out/boot']:
    os.makedirs(board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel-dtbs/debian/{subdir}", exist_ok=True)

  copy_device_trees(args, board, "Linux_for_Tegra/kernel/avt/kernel-dtbs/debian/out/boot")
  sign_device_trees(args, board, "Linux_for_Tegra/kernel/avt/kernel-dtbs/debian/out/boot")

  base_dir = Path(__file__).parent
  for f in ['rules', 'install', 'compat', 'source/format']:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-dtbs/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    shutil.copy(base_dir / "files/kernel-dtb-deb" / f, dstfile)

  for f in ['control', 'changelog']:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-dtbs/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    t.apply_template(base_dir / "files/kernel-dtb-deb" / f, dstfile, board)

  for f in ['postinst', 'postrm']:
    shutil.copy(board.build_dir / "Linux_for_Tegra/kernel/origin/kernel-dtbs/debian" / f, board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-dtbs/debian")
  
  logging.info("Building actual Debian package")
  env = { **os.environ, 'CC': 'aarch64-linux-gnu-gcc' }
  t.execute(['dpkg-buildpackage', '-uc', '-b', '-d', '-a', 'arm64'], cwd=board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-dtbs", env=env)

def build_bootloader_deb(args, board):
  t = tools.tools(args)
  logging.info("Building AVT Bootloader Package")
  for subdir in ['out/usr/share/doc'] + [f"out/{PurePath(e[1]).parent}" for e in board.bootloader_payload_files]:
    os.makedirs(board.build_dir / f"Linux_for_Tegra/kernel/avt/bootloader/debian/{subdir}", exist_ok=True)


  base_dir = Path(__file__).parent
  for f in ['rules', 'install', 'compat', 'source/format', 'postinst', 'postrm', 'config', 'templates']:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/bootloader/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    shutil.copy(base_dir / "files/bootloader" / f, dstfile)

  for ef in board.bootloader_payload_files:
    shutil.copy(board.build_dir / f"Linux_for_Tegra/{ef[0]}", board.build_dir / f"Linux_for_Tegra/kernel/avt/bootloader/debian/out/{ef[1]}")

  for f in ['control', 'changelog']:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/bootloader/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    t.apply_template(base_dir / "files/bootloader" / f, dstfile, board)


  logging.info("Building actual Debian package")
  env = { **os.environ, 'CC': 'aarch64-linux-gnu-gcc' }

  t.execute(['dpkg-buildpackage', '-uc', '-b', '-d', '-a', 'arm64'], cwd=board.build_dir / "Linux_for_Tegra/kernel/avt/bootloader", env=env)

def build_headers_deb(args, board):
  t = tools.tools(args)
  logging.info("Building AVT Header Package")

  file = open(common.common_dir(args) / 'kernel/include/config/kernel.release')
  kernel_release = file.read().strip()
  file.close()

  for subdir in ['usr/share/doc', f'lib/modules/{kernel_release}']:
    os.makedirs(board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel-headers/debian/out/{subdir}", exist_ok=True)

  base_dir = Path(__file__).parent
  for f in ['rules', 'install', 'compat', 'source/format',]:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-headers/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    shutil.copy(base_dir / "files/kernel-headers-deb" / f, dstfile)

  for f in ['control', 'changelog']:
    dstfile = board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-headers/debian" / f
    os.makedirs(dstfile.parent, exist_ok=True)
    t.apply_template(base_dir / "files/kernel-headers-deb" / f, dstfile, board)

  for f in ['postrm']:
    shutil.copy(board.build_dir / "Linux_for_Tegra/kernel/origin/kernel-headers/debian" / f, board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-headers/debian")


  shutil.copytree(board.build_dir / "Linux_for_Tegra/kernel/origin/kernel-headers/usr/src", board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-headers/debian/out/usr/src")

  shutil.copy(board.build_dir / f"Linux_for_Tegra/kernel/origin/kernel-headers/lib/modules/{KERNEL_RELEASE}/build", board.build_dir / f"Linux_for_Tegra/kernel/avt/kernel-headers/debian/out/lib/modules/{kernel_release}",follow_symlinks=False)

  logging.info("Building actual Debian package")
  env = { **os.environ, 'CC': 'aarch64-linux-gnu-gcc' }

  t.execute(['dpkg-buildpackage', '-uc', '-b', '-d', '-a', 'arm64'], cwd=board.build_dir / "Linux_for_Tegra/kernel/avt/kernel-headers", env=env)

def build_repository(args, board):
  logging.info("Building debian repository")
  t = tools.tools(args)

  packages = open(board.build_dir / "Linux_for_Tegra/kernel/avt/Packages",'wt')
  release = open(board.build_dir / "Linux_for_Tegra/kernel/avt/Release",'wt')

  t.execute(['apt-ftparchive','packages','.'],cwd=board.build_dir / "Linux_for_Tegra/kernel/avt",outfile=packages)
  t.execute(['apt-ftparchive','release','.'],cwd=board.build_dir / "Linux_for_Tegra/kernel/avt",outfile=release)

  packages.close()
  release.close()

def sign_repository(args,board):
  if args.sign is not None:
    logging.info("Signing debian repository")
    t = tools.tools(args)
    base_dir = Path(__file__).parent
    t.execute(['gpg','--armor','--output',board.build_dir / "Linux_for_Tegra/kernel/avt/KEY.gpg",'--export',args.sign],cwd=board.build_dir / "Linux_for_Tegra/kernel/avt")
    sig_release_file = board.build_dir / "Linux_for_Tegra/kernel/avt/Release.gpg"
    if os.path.exists(sig_release_file):
      os.remove(sig_release_file)
    t.execute(['gpg','--default-key',args.sign,'-abs','-o','Release.gpg','Release'],cwd=board.build_dir / "Linux_for_Tegra/kernel/avt")

def bundle_repository(args,board):
  logging.info("Bundling debian repository")
  t = tools.tools(args)
  p = board.build_dir / "Linux_for_Tegra/kernel/avt/"
  bundle_file = p / 'avt_l4t_repository.tar.xz'
  repo_bundle = tarfile.open(bundle_file,'w:xz')
  if args.sign is not None:
    for file in ['Release.gpg','KEY.gpg']:
      repo_bundle.add(p / file, file)

  for file in ['Release','Packages']:
    repo_bundle.add(p / file, file)
  for f in p.glob('*.deb'):
    repo_bundle.add(f, f.name)
  repo_bundle.close()


def build_tar_bundle(args, board):
  t = tools.tools(args)
  logging.info("Building tar bundle")
  base_dir = Path(__file__).parent
  p = board.build_dir / "Linux_for_Tegra/kernel/avt/"
  template_args = t.get_template_args(board)
  if board.bundle_name is None:
    bundle_name = string.Template("AlliedVision_NVidia_L4T_${L4T_VERSION}_${AVT_RELEASE}").substitute(template_args)
  else:
    bundle_name = string.Template("AlliedVision_NVidia_${BOARD_NAME}_L4T_${L4T_VERSION}_${AVT_RELEASE}").substitute(template_args)

  bundle_file = p / f'{bundle_name}.tar.gz'

  bundle = tarfile.open(bundle_file,'w:gz')
  bundle.add(p / f'avt_l4t_repository.tar.xz', f'{bundle_name}/avt_l4t_repository.tar.xz')
  #bundle.add(base_dir / f'installer/install.py', f'{bundle_name}/install.py')
  bundle.add(base_dir / f'installer/install.sh', f'{bundle_name}/install.sh')
  bundle.close()

def extract_debs(args, board):
  logging.info("Extracting original NVIDIA Kernel Packages")
  t = tools.tools(args)
  os.makedirs(board.build_dir / "Linux_for_Tegra/kernel/origin", exist_ok=True)
  for deb in glob.glob(str(board.build_dir / "Linux_for_Tegra/kernel/nvidia-l4t-kernel*.deb")):
    deb = Path(deb)
    logging.verbose(f"Extracting {deb.name}")
    part = re.match(r"^nvidia-l4t-(kernel(-[^_]+)?)", deb.name)
    t.execute(['dpkg', '-x', deb.name, f"origin/{part[1]}"], cwd=board.build_dir / "Linux_for_Tegra/kernel")
    t.execute(['dpkg', '-e', deb.name, f"origin/{part[1]}/debian"], cwd=board.build_dir / "Linux_for_Tegra/kernel")
  

def soc_from_bup(bup):
  if bup == 't19x':
    return 't194'
  elif bup == 't23x':
    return 't234'
  else:
    return ''

def build_bups(args, board):
  logging.info("Creating BUPs")
  t = tools.tools(args)
  for bup in board.bups:
    logging.info(f"BUP for {bup}")
    t.execute(['sudo', './l4t_generate_soc_bup.sh', bup], cwd=board.build_dir / "Linux_for_Tegra")
    t.execute(['sudo', board.build_dir / 'Linux_for_Tegra/generate_capsule/l4t_generate_soc_capsule.sh',
               '-i','bl_only_payload','-o','TEGRA_BL.Cap',soc_from_bup(bup)]
              , cwd=board.build_dir / f"Linux_for_Tegra/bootloader/payloads_{bup}")



def install_modules(args, board):
  logging.info(f"Installing kernel modules");
  t = tools.tools(args)
  env = { **os.environ, 'ARCH': 'arm64', 'CROSS_COMPILE': common.common_dir(args) / 'gcc/bin/aarch64-linux-gnu-', 'LANG': 'C' }
  kernel_build_dir=common.common_dir(args) / "kernel"
  t.execute(['sudo', 'make', f'O={kernel_build_dir}', f'INSTALL_MOD_PATH={board.build_dir / "Linux_for_Tegra/kernel/avt/kernel/debian/out"}', 'modules_install'], cwd=build.kernel_source_dir(args), env=env)


def get_dtb_names(args, board):
  return sum((glob.glob(str(common.common_dir(args) / f"kernel/arch/arm64/boot/dts/nvidia/{filter}*.dtb*")) for filter in board.dtb_filters), [])


def copy_device_trees(args, board, subdir):
  logging.info(f"Copying device tree blobs");
  t = tools.tools(args)
  t.execute(["sudo", "cp"] + get_dtb_names(args, board) + [board.build_dir / subdir])

def sign_device_trees(args,board,subdir):
  t = tools.tools(args)
  p = board.build_dir / subdir
  for f in p.glob('tegra194*.dtb'):
    t.execute(["sudo",board.build_dir / f"Linux_for_Tegra/l4t_sign_image.sh",'--file',f.name,'--chip','0x19','--type','kernel_dtb'],cwd=p)

def copy_kernel_image(args, board):
  t = tools.tools(args)
  logging.info(f"Copying kernel image");
  t.execute(["sudo", "cp", common.common_dir(args) / "kernel/arch/arm64/boot/Image", board.build_dir / "Linux_for_Tegra/kernel"])


def copy_files_to_l4t(args, board):
  copy_kernel_image(args, board)
  copy_device_trees(args, board, "Linux_for_Tegra/kernel/dtb")

def clean_deploy_directory(args,board):
  if os.path.exists(board.build_dir / "Linux_for_Tegra/kernel/avt"):
    t = tools.tools(args)
    logging.info("Cleaning debain package build directory")
    t.execute(['sudo', 'rm', '-rf', board.build_dir / "Linux_for_Tegra/kernel/avt"])


def deploy(args, board):
  logging.info(f"Deploying for {board.name}")
  copy_files_to_l4t(args, board)
  build_bups(args, board)
  clean_deploy_directory(args,board)
  extract_debs(args, board)
  build_kernel_deb(args, board)
  build_dtb_deb(args, board)
  build_bootloader_deb(args,board)
  build_headers_deb(args,board)
  build_repository(args, board)
  sign_repository(args,board)
  bundle_repository(args,board)
  build_tar_bundle(args,board)
