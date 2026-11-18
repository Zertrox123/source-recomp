#!/usr/bin/env python3
"""Mount the writable Portal 2 SDK content tree for Puzzle Maker."""
import argparse
from pathlib import Path
import re
import tempfile


def prepare_puzzlemaker(game_dir, user_dir):
    # CBaseFileSystem rejects absolute writes outside its search paths before
    # calling fopen. The editor exports into sdk_content, a sibling of portal2.
    # Use a separate path ID to preserve the order of GAME asset lookups.
    source = Path(game_dir) / 'portal2/gameinfo.txt'
    try:
        data = source.read_bytes()
    except FileNotFoundError:
        return
    target = Path(user_dir) / 'portal2/gameinfo.txt'
    if not re.search(rb'(?im)^[ \t]*"?SDKCONTENT"?[ \t]+', data):
        search = re.search(rb'(?im)^([ \t]*)"?SearchPaths"?[ \t\r\n]*\{', data)
        if not search:
            raise ValueError(f'{source}: SearchPaths block missing')
        newline = b'\r\n' if b'\r\n' in data else b'\n'
        entry = newline + search[1] + b'\tSDKCONTENT\t\t|gameinfo_path|../sdk_content'
        data = data[:search.end()] + entry + data[search.end():]
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.read_bytes() != data:
        out = tempfile.NamedTemporaryFile(dir=target.parent, prefix='.gameinfo.', delete=False)
        temporary = Path(out.name)
        try:
            with out:
                out.write(data)
            temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    (Path(user_dir) / 'sdk_content/maps').mkdir(parents=True, exist_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_dir', type=Path)
    parser.add_argument('user_dir', type=Path)
    args = parser.parse_args()
    prepare_puzzlemaker(args.game_dir, args.user_dir)
