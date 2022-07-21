import shutil
import logging
from collections import namedtuple
from . import tools
from . import upstream
from . import build

CommonFiles = namedtuple('CommonFiles', ['gcc'])

def get_gcc_upstream_file(UpstreamFile):
  f = UpstreamFile("https://developer.nvidia.com/embedded/dlc/l4t-gcc-7-3-1-toolchain-64-bit", "73eed74e593e2267504efbcf3678918bb22409ab7afa3dc7c135d2c6790c2345")
  f.local_file = f.local_file.parent / (f.local_file.name + '.tar.xz')
  return f

def common(args):
  UpstreamFile = upstream.upstream(args)
  return CommonFiles(gcc=get_gcc_upstream_file(UpstreamFile))

def common_dir(args):
  return build.build_dir(args)('common')

def clean_common(args):
  logging.info(f"Cleaning common parts from {common_dir(args)}")
  shutil.rmtree(common_dir(args))

def prepare_common(args, common_files):
  logging.info(f"Preparing common parts in {common_dir(args)}")
  gcc_dir = common_dir(args) / 'gcc'
  logging.info("Extracting cross compiler")
  t = tools.tools(args)
  t.extract(common_files.gcc, gcc_dir, strip=1)

def download_common(args, common_files):
  logging.info(f"Downloading common files")
  for f in common_files:
    f.download()
