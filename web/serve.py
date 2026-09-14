#!/usr/bin/env python3
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=8765);args=p.parse_args()
class Handler(SimpleHTTPRequestHandler):
 def __init__(self,*a,**kw):super().__init__(*a,directory=str(Path(__file__).resolve().parent/'site'),**kw)
 def end_headers(self):
  self.send_header('Cache-Control','no-store');super().end_headers()
 def log_message(self,format,*args):pass
print(f'Pokéwalker browser port: http://127.0.0.1:{args.port}',flush=True)
ThreadingHTTPServer(('127.0.0.1',args.port),Handler).serve_forever()
