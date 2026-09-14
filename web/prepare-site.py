from pathlib import Path
import shutil
root=Path(__file__).resolve().parent
public=root/'public'
if public.exists():shutil.rmtree(public)
public.mkdir()
for name in ['firmware.wasm','runtime-worker.js','futures-worker.js','physics.js','physics-defaults.js','README.md']:
 shutil.copy2(root/name,public/name)
for name in ['assets','fixtures','source']:
 shutil.copytree(root/name,public/name,dirs_exist_ok=True)
(public/'generated').mkdir()
for name in ['build','functions','baseline','verification','audio-verification']:
 source=root/'generated'/(name+'.json')
 if source.exists():shutil.copy2(source,public/'generated'/source.name)
(public/'vendor').mkdir()
for name in ['cannon-es.js','CANNON-LICENSE']:
 shutil.copy2(root/'vendor'/name,public/'vendor'/name)
