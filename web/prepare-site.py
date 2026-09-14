from pathlib import Path
import shutil
root=Path(__file__).resolve().parent
public=root/'public';public.mkdir(exist_ok=True)
for name in ['firmware.wasm','runtime-worker.js','futures-worker.js','physics.js','README.md']:
 if (root/name).exists():shutil.copy2(root/name,public/name)
for name in ['assets','fixtures','generated','source']:
 shutil.copytree(root/name,public/name,dirs_exist_ok=True)
