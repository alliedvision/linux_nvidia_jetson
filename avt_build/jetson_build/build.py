from pathlib import Path
import argparse
import logging
import os
import sys
import multiprocessing

from . import tools
from . import system
from . import board
from . import upstream
from . import download
from . import prepare
from . import deploy
from . import common


def add_arguments(parser):
  class SplitArgs(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
      setattr(namespace, self.dest, values.split(','))

  parser.add_argument('--logfile', default='build.log', type=Path, help="File to write complete build log to")
  parser.add_argument('--skip-precheck', default=False, action="store_true", help="Skip apt based precondition checks")
  parser.add_argument('--build-dir', default='work', type=Path, metavar="PATH", help="Local build directory")
  parser.add_argument('--kernel-dir', default='../kernel/kernel-5.10', type=Path, metavar="PATH", help="Kernel source directory")
  default_steps=['download-common', 'prepare-common', 'download', 'prepare', 'build', 'deploy']
  parser.add_argument('--steps', default=default_steps, action=SplitArgs, help=f"Steps to perform: clear-cache, clean-common, download-common, prepare-common, clean, download, prepare, build, deploy. Order is ignored and will always happen as stated here. Default is {','.join(default_steps)}. WARNING: skipping a step also skips all verification of that step.")
  parser.add_argument('--verbose', '-v', default=False, action='store_true', help="Enable verbose output")
  parser.add_argument('--debug', '-d', default=False, action='store_true', help="Enable debug output (includes verbose output)")
  parser.add_argument('--install-missing',  default=False, action='store_true', help="Install missing host packages")
  parser.add_argument('--sign','-s',default=None,type=str, help="Sign debian repository with given key")

def build_dir(args):
  b_dir = args.build_dir

  if not b_dir.is_absolute():
    b_dir = Path(Path.cwd(), b_dir)

  return lambda base: Path(b_dir, base).resolve()

def kernel_source_dir(args):
  k_dir = args.kernel_dir

  if not k_dir.is_absolute():
    k_dir = Path(Path.cwd(), k_dir)

  return k_dir.resolve()
  

def build(args):
  logging.info(f"Building")
  t= tools.tools(args)
  kernel_build_dir=common.common_dir(args) / "kernel"

#env = { **os.environ, 'ARCH': 'arm64', 'CROSS_COMPILE': common.common_dir(args) / 'gcc/bin/aarch64-linux-gnu-', 'LANG': 'C', 'INSTALL_MOD_PATH': board.build_dir / 'Linux_for_Tegra/rootfs' }
  env = { **os.environ, 'ARCH': 'arm64', 'CROSS_COMPILE': common.common_dir(args) / 'gcc/bin/aarch64-buildroot-linux-gnu-', 'LANG': 'C' }


  t.execute(['make', 'mrproper'], cwd=kernel_source_dir(args), env=env)

  def make(name, target, jobs = multiprocessing.cpu_count()):
    logging.info(f"Making {name}")
    t.execute(['make', '-j', str(jobs), f'O={kernel_build_dir}', target], cwd=kernel_source_dir(args), env=env)


  make('Kernel Config',     'tegra_avcamera_defconfig', 1)
  make('Kernel Image',      'Image')
  make('Kernel Modules',    'modules')
  make('Device Tree Files', 'dtbs')



def main():
  VERBOSE_LEVEL_NUM = 15
  logging.VERBOSE=VERBOSE_LEVEL_NUM
  def verbose(msg, *args, **kwargs):
    logging.log(VERBOSE_LEVEL_NUM, msg, *args, **kwargs)
  logging.verbose = verbose
  logging.addLevelName(VERBOSE_LEVEL_NUM, "VERBOSE")

  try:
    parser = argparse.ArgumentParser()
    add_arguments(parser)
    upstream.add_arguments(parser)
    board.add_arguments(parser)


    args = parser.parse_args()

    logging.basicConfig(level=logging.DEBUG, filename=args.logfile, filemode='w')
    console = logging.StreamHandler()

    if args.debug:
      console.setLevel(logging.DEBUG)
    elif args.verbose:
      console.setLevel(logging.VERBOSE)
    else:
      console.setLevel(logging.INFO)

    formatter = logging.Formatter('%(levelname)-8s %(message)s')
    console.setFormatter(formatter)
    logging.getLogger('').addHandler(console)

    logging.info("Allied Vision Driver Build Tool for NVIDIA Jetson")

    if not args.skip_precheck:
      system.check_preconditions(args)

    boards = board.boards(args)

    steps = args.steps
    if 'clear-cache' in steps:
      upstream.clear_cache(args)

    if 'clean-common' in steps:
      common.clean_common(args)

    if 'download-common' in steps or 'prepare-common' in steps:
      common_files = common.common(args)
      if 'download-common' in steps:
        common.download_common(args, common_files)

      if 'prepare-common' in steps:
        common.prepare_common(args, common_files)

    if 'build' in steps:
      build(args)

    for brd in boards:
      if 'clean' in steps:
        prepare.clean(args, brd)
      if 'download' in steps:
        download.download(args, brd)
      if 'prepare' in steps:
        prepare.prepare(args, brd)
      if 'deploy' in steps:
        deploy.deploy(args, brd)
  except Exception as e:
    raise
  #  logging.error("Build failed.")
  #  sys.stderr.write(f"See {args.logfile.resolve()} for additional information\n")
  #  sys.exit(1)
