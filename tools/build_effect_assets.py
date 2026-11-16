"""Build the dependency-pruned private GU Shoryu/Slicing/Sakura ARC."""
from pathlib import Path
import struct,zlib,json,hashlib
ROOT=Path(__file__).resolve().parent
SHA=lambda b:hashlib.sha256(b).hexdigest()
UID=0x3f40;SAKURA_UID=0x3f41;PEL_PATH='effect/pel/pl/w07_f40'
GUN_PEL_PATH='effect/pel/pl/w04_f40'
GUN_SHELL_PATH='shell/pl/pl_w04_f40/pl_w04_f40'
def entries(raw):
 assert raw[:6]==b'ARC\0\x11\0';result={}
 for i in range(struct.unpack_from('<H',raw,6)[0]):
  a=12+80*i;name=raw[a:a+64].split(b'\0')[0].decode().replace('\\','/');kind,n,flags,at=struct.unpack_from('<4I',raw,a+64)
  data=zlib.decompress(raw[at:at+n]);assert len(data)==flags&0x1fffffff
  key=(name,kind);assert key not in result;result[key]=(flags,data)
 return result
def pel_groups(raw):
 assert raw[:8]==b'PEL\0\x0f\0\0\0';p=8;groups=[]
 for g in range(3):
  count=struct.unpack_from('<I',raw,p)[0];p+=4;rows=[]
  for _ in range(count):
   a=p;p+=1+raw[p];p+=1+raw[p];payload=p;p+=160;row=raw[a:p]
   rows.append(dict(raw=row,payload=payload-a,uid=struct.unpack_from('<H',row,payload-a+48)[0]))
  groups.append(rows)
 assert p==len(raw);return groups
def pack(data):
 b=bytearray(struct.pack('<4sHHI',b'ARC\0',17,len(data),0))+bytearray(80*len(data))
 for i,((name,kind),(flags,raw))in enumerate(data.items()):
  b.extend(bytes(-len(b)%16));p=len(b);compressed=zlib.compress(raw,9);b.extend(compressed)
  at=12+80*i;n=name.encode();assert len(n)<64;b[at:at+len(n)]=n;struct.pack_into('<4I',b,at+64,kind,len(compressed),flags,p)
 return bytes(b)