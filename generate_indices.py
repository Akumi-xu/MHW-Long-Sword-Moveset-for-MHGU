from pathlib import Path
import re,struct,collections,json
lab=Path(__file__).parent;c=lab/'01_plugins/mhw_ls_rebuild/c'
f32=lambda v:struct.unpack('<f',struct.pack('<f',float(v)))[0]
rows=re.findall(r'\{"([^"]+)",(\d+),\d+,\d+,\d+,\{([^}]+)\}\}',(c/'world_particle_dynamics.h').read_text())
assert len(rows)==20
rates=[];indices=[]
for name,index,values in rows:
 values=[f32(x.rstrip('f'))for x in values.split(',')];row=[]
 for k in(9,14,15,16):
  v=values[k]
  if v==1:row.append(0);continue
  assert 0<v<1
  if v not in rates:rates.append(v)
  row.append(rates.index(v)+1)
 indices.append(row)
text='#pragma once\n/* Generated from the existing float32 particle tracks. */\n'
text+='enum { WORLD_SERIES_LIMIT=512 };\nstatic const float world_series_rates[]={'+','.join(format(v,'.9g')+'f'for v in rates)+'};\n'
text+='static const unsigned char world_particle_series_indices[][4]={\n'+''.join('{'+','.join(map(str,r))+'},\n'for r in indices)+'};\n'
(c/'world_particle_series.h').write_text(text)
s=(c/'world_audio_events.h').read_text();section=s[s.index('static const WAudioCue'):];section=section[:section.index('};')]
cues=re.findall(r'\{(\d+),([^,{}]+),(\d+),(\d+)\}',section);assert len(cues)==298
groups=collections.defaultdict(list)
for i,row in enumerate(cues):groups[int(row[0])].append(i)
order=[];ranges=[]
for source,group in sorted(groups.items()):ranges.append((source,len(order),len(group)));order.extend(group)
assert sorted(order)==list(range(len(cues)))
text='#pragma once\n/* Stable original cue indices; seen bits and equal-frame order stay unchanged. */\ntypedef struct {unsigned short source,first,count;} WAudioCueRange;\n'
text+='static const unsigned short world_audio_cue_indices[]={\n'+''.join(','.join(map(str,order[i:i+16]))+',\n'for i in range(0,len(order),16))+'};\n'
text+='static const WAudioCueRange world_audio_cue_ranges[]={\n'+''.join('{'+','.join(map(str,r))+'},\n'for r in ranges)+'};\n'
text+='static const WAudioCueRange *world_audio_cue_range(unsigned source)\n{\n    unsigned lo=0,hi=sizeof(world_audio_cue_ranges)/sizeof(*world_audio_cue_ranges);\n    while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(world_audio_cue_ranges[mid].source<source)lo=mid+1;else hi=mid;}\n    return lo<sizeof(world_audio_cue_ranges)/sizeof(*world_audio_cue_ranges)&&world_audio_cue_ranges[lo].source==source?world_audio_cue_ranges+lo:0;\n}\n'
(c/'world_audio_index.h').write_text(text)
report=dict(float32_series_rates=rates,series_table_bytes=len(rates)*513*4,audio_cues=len(cues),audio_sources=len(groups),max_cues_per_source=max(map(len,groups.values())),average_cues_per_source=len(cues)/len(groups))
(lab/'index_generation.json').write_text(json.dumps(report,indent=2));print(report)
