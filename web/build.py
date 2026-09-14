#!/usr/bin/env python3
"""Compile the release C sources directly into a freestanding WebAssembly module."""
from pathlib import Path
import subprocess, os, re, json, hashlib, shutil
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
OUT=ROOT/'web/generated';OUT.mkdir(exist_ok=True)
subprocess.run(['python3',str(ROOT/'web/generate_endian.py')],check=True)
for folder in ['src','include']:
 shutil.copytree(ROOT/folder,ROOT/'web/source'/folder,dirs_exist_ok=True)
CC=os.environ.get('PW_WASM_CC','/opt/homebrew/opt/llvm/bin/clang')
EXCLUDE={'dbsct','accelerometer','battery','m95512','irc'}
sources=[p for p in sorted((ROOT/'src').glob('*.c')) if p.stem not in EXCLUDE]
flags=['--target=wasm32-unknown-unknown','-std=gnu99','-O2','-g','-ffreestanding','-fno-builtin','-fno-strict-aliasing','-fwrapv','-fpack-struct=2','-Wno-unknown-pragmas','-Wno-incompatible-pointer-types-discards-qualifiers','-Wno-constant-conversion','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-Wno-deprecated-non-prototype','-Iinclude','-Iweb','-Iweb/compat','-Iweb/generated']
# Keep instrumentation in generated copies, preserving readable ported sources.
functions=[];compile_sources=[]
pattern=re.compile(r'(?m)^(?:(?:static|const|volatile)\s+)*(?:void|u8|u16|u32|s8|s16|s32|uint|Bool)(?:\s*\*)?\s+(\w+)\s*\([^;{}]*\)\s*\n\{')
for source in sources:
 text=source.read_text()
 def instrument(m):
  idx=len(functions);line=text[:m.start()].count('\n')+1
  functions.append({'id':idx,'name':m[1],'file':'src/'+source.name,'line':line})
  return m[0]+f'\n  BrowserTrace({idx});'
 target=OUT/source.name
 target.write_text('#include "browser.h"\n'+pattern.sub(instrument,text))
 compile_sources.append(target)
(OUT/'functions.json').write_text(json.dumps(functions,indent=2)+'\n')
(OUT/'function_names.h').write_text('static const char *trace_names[] = {\n'+''.join(json.dumps(f['name'])+',\n' for f in functions)+'};\n')
cmd=[CC,*flags,*map(str,compile_sources),'web/generated/registers.c','web/backend.c','-nostdlib','-Wl,--no-entry','-Wl,--export-all','-Wl,--export-table','-Wl,--initial-memory=4194304','-Wl,--max-memory=4194304','-Wl,-z,stack-size=262144','-Wl,--global-base=327680','-Wl,-Map=web/generated/firmware.map','-o','web/firmware.wasm']
p=subprocess.run(cmd,capture_output=True,text=True)
(OUT/'build.log').write_text(p.stdout+p.stderr)
if p.returncode:
 print((p.stdout+p.stderr)[-14000:]);raise SystemExit(p.returncode)
wasm=ROOT/'web/firmware.wasm'
manifest={'target':'wasm32-unknown-unknown','compiler':subprocess.check_output([CC,'--version'],text=True).splitlines()[0],'wasmBytes':wasm.stat().st_size,'sha256':hashlib.sha256(wasm.read_bytes()).hexdigest(),'sources':[str(p.relative_to(ROOT)) for p in sources],'functions':len(functions),'compileFlags':flags,'cpuInterpreter':False,'imports':[]}
(OUT/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(f'Built {wasm.stat().st_size:,} bytes, {len(sources)} C units, {len(functions)} instrumented functions.')
