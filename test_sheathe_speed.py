from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parent
code=(root/'01_plugins/mhw_ls_rebuild/c/world_adapter.inc').read_text()
function=re.search(r'static float world_motion_speed\(void \*a,const WEvent \*e\)\n\{.*?\n\}\n',code,re.S).group()
stub='#include <assert.h>\n#include <math.h>\n#include <stdio.h>\nenum {W_FLOAT_SPEED=0,FRAME=1,W_SHEATHE=6};\ntypedef struct {unsigned common2;} WEvent;\nstatic struct {unsigned source,move;} world;\nstatic float source_speed=1,frame;\nstatic float real(void*a,unsigned offset){(void)a;assert(offset==FRAME);return frame;}\nstatic float w_float(unsigned source,unsigned field,float time){assert(source==306&&field==W_FLOAT_SPEED&&time==frame);return source_speed;}\n'
checks='int main(void){\n world.source=306;world.move=W_SHEATHE;frame=20;\n WEvent start={.common2=0x80000},hold={.common2=0};\n assert(fabsf(world_motion_speed(0,&start)-1.16f)<1e-6f);\n frame=90;assert(world_motion_speed(0,&hold)==1);\n world.move=4;assert(world_motion_speed(0,&start)==1);\n world.move=W_SHEATHE;source_speed=.75f;frame=40;\n assert(fabsf(world_motion_speed(0,&start)-.87f)<1e-6f);\n assert(world_motion_speed(0,&hold)==.75f);\n puts("PASS default QS3 1.16x; source speed preserved; hold and other moves unchanged; no skill lookup or stacking");\n}\n'
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(stub+function+checks)
 subprocess.run(['gcc',str(p/'test.c'),'-lm','-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
