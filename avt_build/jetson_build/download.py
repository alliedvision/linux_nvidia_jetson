import logging
def download(args, board):
  logging.info(f"Downloading for {board.name}")
  for f in board.files:
    f.download()
