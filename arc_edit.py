from pathlib import Path
import json, sys
from tools.build_effect_assets import entries, pack
if len(sys.argv)!=4 or sys.argv[1] not in ('unpack','pack'): raise SystemExit('Usage: python arc_edit.py unpack <input.arc> <new folder> OR pack <folder> <new output.arc>')
mode,a,b=sys.argv[1:]
if mode=='unpack':
 data=entries(Path(a).read_bytes());folder=Path(b);folder.mkdir(exist_ok=False);rows=[]
 for i,((name,kind),(flags,raw)) in enumerate(data.items()):
  file=f'{i:04d}_{kind:08x}.bin';(folder/file).write_bytes(raw);rows.append(dict(name=name,kind=kind,flags=flags,file=file))
 (folder/'entries.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
else:
 folder=Path(a);rows=json.loads((folder/'entries.json').read_text(encoding='utf8'));data={}
 for row in rows:
  name=row['file'];assert Path(name).name==name and '/' not in name and '\\' not in name
  raw=(folder/name).read_bytes();key=(row['name'],row['kind']);assert key not in data
  data[key]=(row['flags']&0xe0000000|len(raw),raw)
 result=pack(data);assert entries(result)==data
 with Path(b).open('xb') as f: f.write(result)
print('PASS',mode,len(data),'typed resources')
