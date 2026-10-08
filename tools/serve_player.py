#!/usr/bin/env python3
"""Serve the self-contained cartridge player (MIT, 2026 port contributors)."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
class PlayerHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.split('?',1)[0]=='/':self.path='/PLAY.html'
        super().do_GET()

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=8080)
    args=p.parse_args()
    server=ThreadingHTTPServer(('0.0.0.0',args.port),partial(PlayerHandler,directory=str(ROOT)))
    print('Zig Saber cartridge player: http://0.0.0.0:'+str(args.port),flush=True)
    server.serve_forever()
