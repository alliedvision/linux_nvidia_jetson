import sys
import logging
import subprocess

from . import tools

def is_package_installed(packagename):
  p = subprocess.Popen(['dpkg', '-s', packagename], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
  p.wait()
  return p.returncode == 0


def check_preconditions(args):
  t = tools.tools(args)
  required_packages = ['lbzip2', 'device-tree-compiler', 'build-essential', 'libxml2-utils', 'dh-make', 'gnupg']
  missing_packages = [x for x in required_packages if not is_package_installed(x)]

  logging.info("Checking preconditions")
  if missing_packages:
    if args.install_missing:
      t.execute(['sudo','apt-get','update'])
      t.execute(['sudo','apt-get','install','--yes'] + required_packages)
    else:
      logging.error("Missing requirements for build")
      logging.error("Run the following command to install missing packages:")
      logging.error(f"sudo apt install {', '.join(missing_packages)}")
      raise RuntimeError("Required packages missing")
