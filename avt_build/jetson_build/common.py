import shutil
import logging
from collections import namedtuple
from . import tools
from . import upstream
from . import build

CommonFiles = namedtuple('CommonFiles', ['gcc'])

def get_gcc_upstream_file(UpstreamFile):
  f = UpstreamFile("https://developer.nvidia.com/embedded/jetson-linux/bootlin-toolchain-gcc-93", "7725b4603193a9d3751d2715ef242bd16ded46b4e0610c83e76d8891cf580975")
  f.local_file = f.local_file.parent / (f.local_file.name + '.tar.gz')
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

