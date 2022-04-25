import logging
import os
import urllib.request
import urllib.parse
from pathlib import Path, PurePath
import hashlib
import shutil
import sys
import shutil

def add_arguments(parser):
  parser.add_argument('--always-download', action='store_true', help='Ignore existing files and always download. Skips checksum comparisons')
  parser.add_argument('--dont-verify',     action='store_true', help='Don''t compare checksum of downloaded file.')
  parser.add_argument('--dl-cache', default='.dlcache', type=Path, metavar="PATH", help='Local cache directory for upstream files')


def download_cache_dir(args):
  cache_dir = args.dl_cache

  if not cache_dir.is_absolute():
    cache_dir = Path(Path.cwd(), cache_dir)

  return cache_dir


def clear_cache(args):
  cache_dir = download_cache_dir(args)
  logging.info(f"Clearing download cache {cache_dir}")
  shutil.rmtree(cache_dir)


def upstream(args):
  always_download = args.always_download
  dont_verify = args.dont_verify

  cache_dir = download_cache_dir(args)

  class UpstreamFile:
    def __init__(self, url, sha):
      self.url = url
      self.expected_sha = sha

      uparts = urllib.parse.urlparse(url)
      self.rel_dl_file = PurePath(uparts.path).relative_to('/embedded')
      self.local_file = Path(cache_dir, self.rel_dl_file).resolve()

    def local_sha(self):
      hash_sha = hashlib.sha256()
      with open(self.local_file, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
          hash_sha.update(chunk)
      return hash_sha.hexdigest()

    def is_valid(self):
      return self.local_sha() == self.expected_sha

    def needs_download(self):
      if always_download:
        logging.info(f"Requested to always download, not checking {self.local_file}")
        return True

      try:
        return not self.is_valid()
      except FileNotFoundError:
        logging.info('File {} not found, needs download'.format(self.rel_dl_file))
        return True

    def make_local_dir(self):
      os.makedirs(self.local_file.parent, exist_ok=True)

    def download(self):
      if self.needs_download():
        self.make_local_dir()

        with urllib.request.urlopen(self.url) as u:
          full_length = int(u.getheader('Content-Length'))
          with open(self.local_file, 'wb') as f:
            read_length = 0
            for chunk in iter(lambda: u.read(4096), b''):
              f.write(chunk)
              read_length = read_length + len(chunk)
              progress = int(20*read_length / full_length)
              sys.stdout.write(f"\rDownloading {self.rel_dl_file}, size {full_length} bytes [{'#'*progress}{' '*(20-progress)}] {100*read_length / full_length:6.2f}%")
              sys.stdout.flush()
          print()

          if not dont_verify:
            new_sha = self.local_sha()
            if not self.is_valid():
              raise RuntimeError(f'File {self.rel_dl_file} download checksum mismatch (expected {self.expected_sha}, found {new_sha})')
            logging.verbose(f'File {self.rel_dl_file} downloaded successfully, checksum {new_sha}')
          else:
            logging.verbose(f'File {self.rel_dl_file} downloaded successfully, checksum checks disabled')

      else:
        logging.verbose(f'File {self.rel_dl_file} exists and is up to date, no download necessary')
    
  return lambda url, sha: UpstreamFile(url, sha)
