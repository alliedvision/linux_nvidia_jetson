import logging
import string
import subprocess
import os
import selectors
import sys
from collections import namedtuple

from .board import KERNEL_RELEASE, L4T_VERSION, AVT_RELEASE

Tools = namedtuple('Tools', ['extract', 'execute', 'apply_template','get_template_args'])

def tools(args):
  def get_template_args(board):
    avt_release = os.getenv('AVT_RELEASE')

    if avt_release is None:
      process = subprocess.run(['git','rev-parse','--short','HEAD'],stdout=subprocess.PIPE)
      git_hash = process.stdout.decode("utf-8").strip()
      avt_release = f'{AVT_RELEASE}~g{git_hash}'

    template_args = {
      "L4T_TOOLS_VERSION": board.l4t_tools_version,
      "KERNEL_RELEASE": KERNEL_RELEASE,
      "L4T_VERSION": L4T_VERSION,
      "AVT_RELEASE": avt_release,
      "BOARD_NAME": board.bundle_name
    }
    return template_args

  def apply_template(src, dest, board):
    template_args = get_template_args(board)

    fin = open(src, 'rt')
    template = string.Template(fin.read())
    fin.close()
    fout = open(dest, 'wt')
    fout.write(template.substitute(template_args))
    fout.close()

  def execute(args, **kwargs):
    src = args[0]
    try:
      sudo = kwargs.get('sudo', False)
      stdin = None
      if sudo:
        args = ['sudo', '-S', '-n'] + args
        stdin = subprocess.PIPE
        src = 'sudo ' + src

      outfile = kwargs.get('outfile', subprocess.PIPE)

      sub = subprocess.Popen(args, bufsize=1, stdout=outfile, stderr=subprocess.PIPE, cwd=kwargs.get('cwd', None), env=kwargs.get('env', None))

      logging.verbose(f"Executing `{' '.join(str(x) for x in args)}`")

      sel = selectors.DefaultSelector()
      if outfile == subprocess.PIPE:
        sel.register(sub.stdout, selectors.EVENT_READ)
      sel.register(sub.stderr, selectors.EVENT_READ)
      stderr_log = ""
      stdout_log = ""
      while sub.poll() == None:
        events = sel.select()
        for k, _ in events:
          fd = k.fileobj
          data = fd.readline().decode('UTF-8')

          if len(data) > 0:
            data = data.rstrip()
            if fd == sub.stdout:
              stdout_log += data + "\n"
              logging.debug(f'({src},stdout) {data}')
            elif fd == sub.stderr:
              stderr_log += data + "\n"
              logging.debug(f'({src},stderr) {data}')

      if sub.returncode != 0:
        logging.error(f'Error {sub.returncode} executing `{" ".join(str(x) for x in args)}`')
        logging.error(f'stdout:\n{stdout_log}\nstderr:\n{stderr_log}')
        raise RuntimeError(f'Subprocess returned error {sub.returncode}')

    except Exception as err:
      raise RuntimeError(f'Error {str(err)} executing `{" ".join(str(x) for x in args)}`') from err
      

  def extract(archive_file, destination, **kwargs):
    logging.verbose(f'Extracting {archive_file.local_file} to {destination}')
    os.makedirs(destination, exist_ok=True)
    suffixes = archive_file.local_file.suffixes
    args = []
    if '.tbz2' in suffixes or '.tgz' in suffixes or '.tar' in suffixes:
      strip = kwargs.get('strip', None)
      cmd = 'xfv'
      if '.xz' in suffixes:
        cmd = cmd + 'J'
      elif '.bz2' in suffixes or '.tbz2' in suffixes:
        cmd = cmd + 'j'
      elif '.gz' in suffixes or '.tgz' in suffixes:
        cmd = cmd + 'z'

      args.extend(['tar', cmd, archive_file.local_file])
      if strip:
        args.append(f'--strip-components={strip}')
      args.extend(['-C', destination])

    if kwargs.get('sudo', False):
      args.insert(0, 'sudo')

    try:
      execute(args)
    except Exception as err:
      logging.error(f'Failed to extract {archive_file.local_file}')
      raise RuntimeError(f'Error extracting archive') from err

  return Tools(extract=extract, execute=execute, apply_template=apply_template, get_template_args=get_template_args)
