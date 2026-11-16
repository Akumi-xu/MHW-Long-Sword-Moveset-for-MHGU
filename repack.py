from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib, json, subprocess, sys
root=Path(__file__).resolve().parent
manifest=json.loads((root/'RELEASE.json').read_text(encoding='utf8'))
if len(sys.argv)!=4: raise SystemExit('Usage: python repack.py <matching PC release.zip> <rebuilt.nro> <new output.zip>')
base,nro,out=map(Path,sys.argv[1:])
assert out.resolve() not in (base.resolve(),nro.resolve()) and not out.exists(), 'Use a new output filename'
assert hashlib.sha256(base.read_bytes()).hexdigest()==manifest['pc_sha256'], 'Wrong release baseline'
subprocess.run([sys.executable,str(root/'tools/check_relocations.py'),str(nro.resolve())],check=True)
blob=nro.read_bytes();assert blob[16:20]==b'NRO0', 'Not an NRO'
with ZipFile(base) as z:
 assert z.testzip() is None
 files={n:z.read(n) for n in z.namelist()}
for name,digest in manifest['payload_sha256'].items(): assert hashlib.sha256(files[name]).hexdigest()==digest
files['romfs/nativeNX/plugins/mhw_ls_rebuild.nro']=blob
with ZipFile(out,'x',ZIP_DEFLATED,compresslevel=7) as z:
 for name,data in files.items(): z.writestr(name,data)
with ZipFile(out) as z: assert z.testzip() is None and {n:z.read(n) for n in z.namelist()}==files
print('Created',out,'SHA256',hashlib.sha256(out.read_bytes()).hexdigest())
