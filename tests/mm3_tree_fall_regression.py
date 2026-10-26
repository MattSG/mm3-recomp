"""Exercise falling native trees without a graphics device.

Run inside `python scripts/devenv.py --shell`. The generated native retirement/ID chain
executes against the existing two-class city fixture. A queued-copy renderer
checks the production draw wrapper, grounded geometry, visible trajectories,
guest state restoration, streaming guards, and pause/resume clock behavior.
The real native renderer and both matrix-copy queues also run against a
billboard submission stub, including the former upright-copy negative control.
"""
import re
import struct
import subprocess
import tempfile
from pathlib import Path

import cc
import mm3_tree_contact_regression as contact

ROOT = contact.ROOT
MODEL, MATRIX = 0x874000, 0x880000
ENTRIES = (0x16CABD, 0x15FFE0, 0x15CA4B, 0x15B86B, 0x163C28, 0x163C6F,
           0x163BF4, 0x164960, 0x16492A)

C_TEST = r'''
static unsigned long long wall_ms=100000;
static unsigned long long test_clock(void){return wall_ms;}
#define MM3_TREE_CLOCK test_clock
#define MM3_TREE_LOG(...) ((void)0)
#define MM3_TREE_FLUSH() ((void)0)
#include "mm3_tree_diagnostics.c"
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);exit(2);}}while(0)
#define RAM_SIZE 0x1000000u
#define STACK 0x700000u
#define CAR 0x760000u
#define BODY 0x780000u
#define RECORD 0x790000u
#define CITY 0x800000u
#define CELLS 0x820000u
#define CELL 0x830000u
#define CHANGES 0x860000u
#define MODEL 0x874000u
#define MATRIX 0x880000u
static unsigned char *ram,*initial,*expected_ram;
static uint32_t id,primitive,mask_address,vector_address;
static unsigned type,draw_calls,last_drawing,nested_requested,nested_active;
static float queued[12];
static const float baseline[12]={0,0,-2, 0,1.5f,0, .75f,0,0, 120,4,-75};
static const float base_local[3]={1,2,-1},top_local[3]={1,12,-1};
typedef struct{
 uint32_t r[10];int f[3];uint16_t c[2];double fp[8];RecompXmm xmm[8];
} State;
static State state(void){
 State v;memset(&v,0,sizeof v);
 v.r[0]=g_eax;v.r[1]=g_ecx;v.r[2]=g_edx;v.r[3]=g_esp;v.r[4]=g_ebx;
 v.r[5]=g_esi;v.r[6]=g_edi;v.r[7]=g_ebp;v.r[8]=g_seh_ebp;v.r[9]=g_fs_base;
 v.f[0]=g_fp_top;v.f[1]=g_df;v.f[2]=g_fp_cmp;
 v.c[0]=g_fp_control_word;v.c[1]=g_fp_cc;memcpy(v.fp,g_fp_stack,sizeof v.fp);
 v.xmm[0]=g_xmm0;v.xmm[1]=g_xmm1;v.xmm[2]=g_xmm2;v.xmm[3]=g_xmm3;
 v.xmm[4]=g_xmm4;v.xmm[5]=g_xmm5;v.xmm[6]=g_xmm6;v.xmm[7]=g_xmm7;
 return v;
}
static void sentinels(void){
 g_eax=0xABCDEF01;g_ecx=0xBCDEF012;g_edx=0xCDEF0123;
 g_ebx=0x12345678;g_esi=0x23456789;g_edi=0x3456789A;
 g_ebp=0x456789AB;g_seh_ebp=0x56789ABC;g_fs_base=0x6789ABCD;
 g_fp_top=3;g_df=1;g_fp_cmp=-1;g_fp_control_word=0x27F;g_fp_cc=0x4100;
 for(unsigned i=0;i<8;i++)g_fp_stack[i]=i*1.25-3;
 memset(&g_xmm0,0xA0,sizeof g_xmm0);memset(&g_xmm1,0xA1,sizeof g_xmm1);
 memset(&g_xmm2,0xA2,sizeof g_xmm2);memset(&g_xmm3,0xA3,sizeof g_xmm3);
 memset(&g_xmm4,0xA4,sizeof g_xmm4);memset(&g_xmm5,0xA5,sizeof g_xmm5);
 memset(&g_xmm6,0xA6,sizeof g_xmm6);memset(&g_xmm7,0xA7,sizeof g_xmm7);
}
void sub_0016AEDB_original(void){
 ++draw_calls;CHECK(tree_read(MEM32(g_esp+24),queued,sizeof queued));
 last_drawing=mm3_tree_fall_drawing();
 if(nested_requested&&!nested_active){
  uint32_t outer_stack=g_esp;CHECK(last_drawing==1);nested_active=1;
  g_esp=STACK+0x100;memset((void *)XBOX_PTR(g_esp),0,40);
  MEM32(g_esp)=0xDEADBEEF;MEM32(g_esp+4)=MODEL;
  MEM32(g_esp+24)=MATRIX;MEM32(g_esp+36)=id;
  sub_0016AEDB();CHECK(last_drawing==0&&mm3_tree_fall_drawing()==1);
  nested_active=0;g_esp=outer_stack;last_drawing=mm3_tree_fall_drawing();
 }
 uint32_t result_stack=g_esp+40;sentinels();g_esp=result_stack;
}
void sub_000E1F49_original(void){CHECK(0);}
void sub_001D137B_original(void){CHECK(0);}
static int close_enough(float a,float b){return fabsf(a-b)<0.0002f;}
static void point(float out[3],const float matrix[12],const float in[3]){
 for(unsigned j=0;j<3;j++)out[j]=matrix[9+j]+matrix[j]*in[0]+matrix[3+j]*in[1]+matrix[6+j]*in[2];
}
static float dot(const float a[3],const float b[3]){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static unsigned fall_count(void){
 unsigned count=0;for(unsigned i=0;i<TREE_FALL_SLOTS;i++)count+=s_tree_falls[i].primitive!=0;return count;
}
static void reset(void){
 memcpy(ram,initial,RAM_SIZE);memset(s_tree_falls,0,sizeof s_tree_falls);
 s_tree_fall_city=s_tree_fall_cells=0;s_tree_fall_time=s_tree_fall_wall=s_tree_fall_deadline=0;
 s_tree_fall_replacement=0;wall_ms=100000;s_tree_break_pending=0;draw_calls=0;
 id=MEM32(RECORD+0x104);type=(id>>21)&3;
 primitive=0x840000u+((id>>5)&0xFFFFu)*(type?0xF8u:0xCCu);
 mask_address=primitive+(type?0xCu:8u);vector_address=0x850000u+(id>>23)*16;
 sentinels();g_esp=STACK;
}
static void retire(void){
 State before,after;s_tree_contact_car=CAR;before=state();
 CHECK(mm3_tree_use_solid_contact(RECORD,30));CHECK(tree_remove_native());after=state();
 CHECK(!memcmp(&before,&after,sizeof before));CHECK(fall_count()==1);
 CHECK(MEM32(mask_address)==(0xFFFFFFFFu^(1u<<(id&31u))));
 CHECK(MEM32(vector_address+8)==CHANGES+4&&MEM32(CHANGES)==id);
 /* The real native change-list/mask path must leave every other guest byte,
  * including nearby instances, shared geometry and the scratch stack intact. */
 memcpy(expected_ram,initial,RAM_SIZE);
 *(uint32_t *)(expected_ram+mask_address)=0xFFFFFFFFu^(1u<<(id&31u));
 *(uint32_t *)(expected_ram+vector_address+8)=CHANGES+4;
 *(uint32_t *)(expected_ram+CHANGES)=id;
 CHECK(!memcmp(expected_ram,ram,RAM_SIZE));
 uint32_t mask=MEM32(mask_address);
 CHECK(mm3_tree_render_mask(primitive,mask,type)==0xFFFFFFFFu);
 CHECK(MEM32(mask_address)==mask); /* Collision remains permanently retired. */
 CHECK(mm3_tree_render_mask(primitive+0x1000,mask,type)==mask);
 CHECK(mm3_tree_render_mask(primitive,mask,type^1u)==mask);
 /* A caller's additional missing bits never return, only the struck piece. */
 uint32_t extra=1u<<((id+1u)&31u);
 CHECK(mm3_tree_render_mask(primitive,mask&~extra,type)==(0xFFFFFFFFu&~extra));
}
static void advance(unsigned duration){
 while(duration){unsigned step=duration>100?100:duration;wall_ms+=step;tree_fall_clock();duration-=step;}
}
static void draw(uint32_t caller,uint32_t model,uint32_t draw_id){
 State direct,wrapped;
 memcpy((void *)XBOX_PTR(MATRIX),baseline,sizeof baseline);
 g_esp=STACK;memset((void *)XBOX_PTR(STACK),0,40);
 MEM32(STACK)=caller;MEM32(STACK+4)=model;MEM32(STACK+24)=MATRIX;MEM32(STACK+36)=draw_id;
 CHECK(!mm3_tree_fall_drawing());sub_0016AEDB_original();direct=state();
 sentinels();g_esp=STACK;memcpy(expected_ram,ram,RAM_SIZE);
 unsigned before=draw_calls;sub_0016AEDB();wrapped=state();
 CHECK(!mm3_tree_fall_drawing());
 CHECK(draw_calls==before+1&&!memcmp(&direct,&wrapped,sizeof direct));
 CHECK(!memcmp((const void *)XBOX_PTR(MATRIX),baseline,sizeof baseline));
 CHECK(!memcmp(expected_ram,ram,RAM_SIZE));
 /* Whole-draw rigidity, with nonuniform scale/yaw, catches transposed matrix
  * conventions and pivot fixes that move only the trunk or stretch its crown. */
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<3;j++)
  CHECK(close_enough(dot(queued+i*3,queued+j*3),dot(baseline+i*3,baseline+j*3)));
}
static void trajectory(void){
 float hinge[3],foot[3],crown[3],height[3],previous_height[3];
 float previous_slide=0;
 reset();retire();point(hinge,baseline,base_local);
 draw(type?0x15C05D:0x15E4D9,MODEL,id);
 for(unsigned i=0;i<12;i++)CHECK(close_enough(queued[i],baseline[i]));
 const unsigned ages[]={100,450,899,900,1250,1600,12000,13000,13900};
 unsigned age=0;
 for(unsigned a=0;a<sizeof ages/sizeof ages[0];a++){
  advance(ages[a]-age);age=ages[a];draw(type?0x15C05D:0x15E4D9,MODEL,id);
  CHECK(last_drawing==1);
  point(foot,queued,base_local);point(crown,queued,top_local);
  float travel[3]={foot[0]-hinge[0],0,foot[2]-hinge[2]};
  float slide=sqrtf(dot(travel,travel));
  CHECK(travel[0]>0&&travel[2]>0&&close_enough(travel[0]*4,travel[2]*3));
  CHECK(slide>=previous_slide-0.0002f&&slide<=0.2502f);previous_slide=slide;
  if(age>=1600)CHECK(close_enough(slide,0.25f));
  if(age<=12000)CHECK(close_enough(foot[1],hinge[1]));
  for(unsigned j=0;j<3;j++)height[j]=crown[j]-foot[j];
  CHECK(close_enough(dot(height,height),225));
  /* Impact (3,9,4) must tip horizontally along (3,0,4); its vertical
   * component cannot produce yaw or a fall backward into the vehicle. */
  CHECK(height[0]>0&&height[2]>0&&close_enough(height[0]*4,height[2]*3));
  if(age==100)CHECK(height[1]>14.9f);
  if(age==450)CHECK(height[1]>10&&height[1]<14&&height[0]>2);
  if(age==899)CHECK(height[1]>=0&&height[1]<0.1f);
  if(age==900||age==1600||age==12000||age>=13000){
   CHECK(close_enough(height[0],9)&&close_enough(height[1],0)&&close_enough(height[2],12));
  }
  if(age==1250)CHECK(height[1]>0.5f&&height[1]<1.5f); /* Gentle landing bounce. */
  if(age==13000)CHECK(close_enough(foot[1],hinge[1]-2));
  if(age==13900)CHECK(foot[1]<hinge[1]-3.9f&&foot[1]>hinge[1]-4.01f);
  if(age==899)memcpy(previous_height,height,sizeof height);
  if(age==900)CHECK(fabsf(height[1]-previous_height[1])<0.1f); /* No landing pop. */
 }
 advance(100);uint32_t mask=MEM32(mask_address);
 CHECK(mm3_tree_render_mask(primitive,mask,type)==mask&&fall_count()==0);
 draw(type?0x15C05D:0x15E4D9,MODEL,id);CHECK(!memcmp(queued,baseline,sizeof baseline));
}
static void impact_motion(void){
 float slow_height=0,slow_slide=0;
 const float velocities[][3]={{3,9,4},{21,-90,28},{120,400,160},{0,9,0},{NAN,9,0},{INFINITY,9,0}};
 for(unsigned v=0;v<sizeof velocities/sizeof velocities[0];v++){
  reset();memcpy((void *)XBOX_PTR(BODY+0x20),velocities[v],sizeof velocities[v]);
  s_tree_contact_car=CAR;CHECK(mm3_tree_use_solid_contact(RECORD,30)&&tree_remove_native());
  float anchor[3],base[3],tip[3],travel[3],height[3];point(anchor,baseline,base_local);
  advance(450);draw(type?0x15C05D:0x15E4D9,MODEL,id);
  point(base,queued,base_local);point(tip,queued,top_local);
  for(unsigned j=0;j<3;j++){travel[j]=base[j]-anchor[j];height[j]=tip[j]-base[j];}
  float slide=hypotf(travel[0],travel[2]);
  CHECK(close_enough(base[1],anchor[1])&&slide>0&&slide<1.25f);
  for(unsigned j=0;j<12;j++)CHECK(isfinite(queued[j]));
  if(v==0){slow_height=height[1];slow_slide=slide;CHECK(slow_height>12);}
  if(v==1||v==2){
   /* Strong impacts noticeably tip earlier and carry the trunk farther.
    * An excessive speed is bounded to the same visible response. */
   CHECK(height[1]<slow_height-5&&slide>slow_slide+0.5f);
   CHECK(close_enough(travel[0]*4,travel[2]*3));
  }
  if(v>=3){
   CHECK(close_enough(height[1],slow_height)&&close_enough(slide,slow_slide));
   CHECK(travel[0]>0&&close_enough(travel[2],0));
  }
  advance(1050);draw(type?0x15C05D:0x15E4D9,MODEL,id);
  point(base,queued,base_local);point(tip,queued,top_local);
  slide=hypotf(base[0]-anchor[0],base[2]-anchor[2]);
  CHECK(close_enough(base[1],anchor[1])&&close_enough(tip[1],base[1]));
  CHECK(close_enough(slide,v==1||v==2?1.25f:0.25f));
  if(v==1||v==2){
   /* Both sides of a fast landing stay continuous and grounded; the
    * crown subsequently rebounds once and settles flat. */
   reset();memcpy((void *)XBOX_PTR(BODY+0x20),velocities[v],sizeof velocities[v]);
   s_tree_contact_car=CAR;CHECK(mm3_tree_use_solid_contact(RECORD,30)&&tree_remove_native());
   advance(549);draw(type?0x15C05D:0x15E4D9,MODEL,id);
   point(base,queued,base_local);point(tip,queued,top_local);float before_height=tip[1]-base[1];
   CHECK(before_height>=0&&before_height<0.1f&&close_enough(base[1],anchor[1]));
   advance(1);draw(type?0x15C05D:0x15E4D9,MODEL,id);
   point(base,queued,base_local);point(tip,queued,top_local);
   CHECK(close_enough(tip[1],base[1])&&fabsf(tip[1]-base[1]-before_height)<0.1f);
   advance(275);draw(type?0x15C05D:0x15E4D9,MODEL,id);
   point(base,queued,base_local);point(tip,queued,top_local);
   CHECK(tip[1]-base[1]>1&&tip[1]-base[1]<2&&close_enough(base[1],anchor[1]));
   advance(276);draw(type?0x15C05D:0x15E4D9,MODEL,id);
   point(base,queued,base_local);point(tip,queued,top_local);
   CHECK(close_enough(tip[1],base[1])&&close_enough(base[1],anchor[1]));
  }
 }
}
static void caller_and_input_guards(void){
 float saved[12];reset();retire();advance(450);
 for(unsigned i=0;i<3;i++){
  uint32_t caller=i==0?0x15A8F4:i==1?0x167D94:0xDEADBEEF;
  draw(caller,MODEL,id);CHECK(!last_drawing&&!memcmp(queued,baseline,sizeof baseline));
 }
 draw(type?0x15C05D:0x15E4D9,MODEL,id^1);CHECK(!memcmp(queued,baseline,sizeof baseline));
 draw(type?0x15C05D:0x15E4D9,MODEL+0xF4,id);CHECK(!memcmp(queued,baseline,sizeof baseline));
 CHECK(fall_count()==1);
 CHECK(!tree_fall_transform(MODEL,id,0xFFFFFFFFu,saved));
 MEMF(MATRIX)=NAN;memcpy(expected_ram,ram,RAM_SIZE);
 CHECK(!tree_fall_transform(MODEL,id,MATRIX,saved)&&!memcmp(expected_ram,ram,RAM_SIZE));
 memcpy((void *)XBOX_PTR(MATRIX),baseline,sizeof baseline);MEMF(MODEL+0xC)=NAN;
 memcpy(expected_ram,ram,RAM_SIZE);
 CHECK(!tree_fall_transform(MODEL,id,MATRIX,saved)&&!memcmp(expected_ram,ram,RAM_SIZE));
 /* Native removal can proceed when a malformed/absent model cannot animate. */
 reset();MEMF(MODEL+0x18)=MEMF(MODEL+0xC)+0.1f;s_tree_contact_car=CAR;
 CHECK(mm3_tree_use_solid_contact(RECORD,30)&&tree_remove_native());
 CHECK(fall_count()==0&&!(MEM32(mask_address)&(1u<<(id&31u))));
}
static void prepare_variants(void){
 const uint32_t variants=0x890000u,indices=0x891000u;
 uint32_t owner=type?tree_word(primitive+4):tree_word(tree_word(primitive));
 MEM32(MODEL+0xF0)=1;MEM32(owner+0x1250)=variants;
 MEM32(variants+0x2Cu+0x20u)=indices;MEM32(variants+0x2Cu+0x24u)=indices+6;
 /* Keep the fixture owner's headers intact; slots 1/2 overlap them. The
  * three authored variants have their own geometry origin and tree height. */
 for(unsigned index=3;index<=6;index++){
  uint32_t model=MODEL+index*0xF4u;
  memcpy((void *)XBOX_PTR(model),(const void *)XBOX_PTR(MODEL),0xF4);
  MEM32(model+0xF0)=0xFFFFFFFFu;
  const float bounds[6]={2.0f+index,5,-4,6.0f+index,13,-2};
  memcpy((void *)XBOX_PTR(model+8),bounds,sizeof bounds);
 }
 MEM16(indices)=3;MEM16(indices+2)=4;MEM16(indices+4)=5;
}
static uint32_t native_variant(void){
 /* Execute the actual native ID constructor and selector used by 15D2DD.
  * Expected selection is not a second copy of the production hash. */
 g_esp=STACK;g_eax=primitive+0xC8u;MEM32(STACK)=0x15E438u;MEM32(STACK+4)=id&31u;
 sub_00163BF4();CHECK(g_eax==id&&g_esp==STACK+8);
 g_esp=STACK;g_eax=0;g_ecx=tree_word(tree_word(primitive))+0x11CCu;
 MEM32(STACK)=0x15E452u;MEM32(STACK+4)=id;
 sub_00164960();CHECK(g_esp==STACK+8);return g_eax;
}
static void model_variants(void){
 reset();retire();prepare_variants();advance(1600);
 if(type){
  /* Type1's native renderer reads the base model directly. A valid type0
   * variant table must not broaden that caller's model identity. */
  draw(0x15C05D,MODEL+3*0xF4u,id);CHECK(!memcmp(queued,baseline,sizeof baseline));
  return;
 }
 uint32_t actual=native_variant();CHECK(actual>=MODEL+3*0xF4u&&actual<=MODEL+5*0xF4u);
 draw(0x15E4D9,actual,id);CHECK(memcmp(queued,baseline,sizeof baseline));
 float local_foot[3],local_tip[3],anchor[3],foot[3],tip[3];
 local_foot[0]=(MEMF(actual+8)+MEMF(actual+0x14))*0.5f;
 local_foot[1]=MEMF(actual+0xC);local_foot[2]=(MEMF(actual+0x10)+MEMF(actual+0x1C))*0.5f;
 memcpy(local_tip,local_foot,sizeof local_tip);local_tip[1]=MEMF(actual+0x18);
 point(anchor,baseline,local_foot);point(foot,queued,local_foot);point(tip,queued,local_tip);
 CHECK(close_enough(foot[0]-anchor[0],0.15f)&&close_enough(foot[1],anchor[1])
  &&close_enough(foot[2]-anchor[2],0.20f));
 CHECK(close_enough(tip[0]-foot[0],7.2f)&&close_enough(tip[1]-foot[1],0)
  &&close_enough(tip[2]-foot[2],9.6f));
 CHECK(mm3_tree_render_mask(primitive,MEM32(mask_address),type)==0xFFFFFFFFu);
 /* Other valid variants and unrelated models sharing metadata do not gain
  * the selected instance's animation merely by belonging to its model pool. */
 for(unsigned index=3;index<=6;index++)if(MODEL+index*0xF4u!=actual){
  draw(0x15E4D9,MODEL+index*0xF4u,id);CHECK(!memcmp(queued,baseline,sizeof baseline));
 }
 for(unsigned invalid=0;invalid<10;invalid++){
  prepare_variants();uint32_t owner=tree_word(tree_word(primitive)),row=0x89002Cu;
  if(invalid==0)MEM32(owner+0x1250)=0;
  if(invalid==1)MEM32(MODEL+0xF0)=0xFFFFFFFFu;
  if(invalid==2)MEM32(MODEL+0xF0)=4096;
  if(invalid==3)MEM32(row+0x20)=0;
  if(invalid==4)MEM32(row+0x24)=0x890000u;
  if(invalid==5)MEM32(row+0x24)=0x891005u;
  if(invalid==6)MEM32(row+0x24)=0x891000u;
  if(invalid==7)MEM32(row+0x24)=0x891000u+4097u*2u;
  if(invalid==8)MEM32(owner+0x1250)=0xFFFFFFF0u;
  if(invalid==9)MEM32(row+0x20)=MEM32(row+0x24)=0xFFFFFFF0u;
  draw(0x15E4D9,actual,id);CHECK(!memcmp(queued,baseline,sizeof baseline));
  CHECK(fall_count()==1&&!(MEM32(mask_address)&(1u<<(id&31u))));
 }
 /* A changed variant list is resolved afresh. Only the current native
  * selection may animate, even when the old model is still readable. */
 prepare_variants();MEM16(0x891000)=MEM16(0x891002)=MEM16(0x891004)=6;
 uint32_t changed=native_variant();CHECK(changed==MODEL+6*0xF4u);
 draw(0x15E4D9,actual,id);CHECK(!memcmp(queued,baseline,sizeof baseline));
 draw(0x15E4D9,changed,id);CHECK(memcmp(queued,baseline,sizeof baseline));
}
static void streaming_guards(void){
 for(unsigned guard=0;guard<11;guard++){
  reset();retire();uint32_t mask=MEM32(mask_address);
  if(guard==0)MEM32(0x4291F0)=CITY+0x10000;
  if(guard==1)MEM32(CITY+0x15304)=CELLS+0x100;
  if(guard==2)MEM32(CELLS+(id>>23)*4)=CELL+0x100;
  if(guard==3)MEM32(primitive)=MEM32(primitive)+4;
  if(guard==4)MEM32(primitive+(type?0xF4u:0xC8u))^=0x20;
  if(guard==5)MEM32(MODEL+0xA0)=0;
  if(guard==6)MEM32(mask_address)|=1u<<(id&31u); /* Reload restored availability. */
  if(guard==7)MEM32(CELL+(type?0x10u:0x18u))+=4; /* Array shifted under identical ID. */
  if(guard==8)MEM32(CELL+(type?0x14u:0x1Cu))=primitive; /* Range no longer includes it. */
  if(guard==9)MEM32(primitive+4)^=0x10000; /* Same context, changed owner/instance key. */
  if(guard==10)MEM16(primitive+(type?8u:4u))++; /* Reused instance now names another descriptor. */
  if(guard==6)mask=MEM32(mask_address);
  memcpy(expected_ram,ram,RAM_SIZE);
  CHECK(mm3_tree_render_mask(primitive,mask,type)==mask&&fall_count()==0);
  CHECK(!memcmp(expected_ram,ram,RAM_SIZE));
  draw(type?0x15C05D:0x15E4D9,MODEL,id);CHECK(!memcmp(queued,baseline,sizeof baseline));
 }
}
static void direction_controls(void){
 const float velocities[][3]={{20,7,0},{-20,7,0},{0,7,20},{0,7,-20},{0,7,0},{NAN,7,0}};
 const float endpoints[][3]={{15,0,0},{-15,0,0},{0,0,15},{0,0,-15},{15,0,0},{15,0,0}};
 for(unsigned v=0;v<sizeof velocities/sizeof velocities[0];v++){
  reset();memcpy((void *)XBOX_PTR(BODY+0x20),velocities[v],sizeof velocities[v]);
  s_tree_contact_car=CAR;CHECK(mm3_tree_use_solid_contact(RECORD,30)&&tree_remove_native());
  CHECK(fall_count()==1);advance(1600);draw(type?0x15C05D:0x15E4D9,MODEL,id);
  float base[3],tip[3];point(base,queued,base_local);point(tip,queued,top_local);
  for(unsigned j=0;j<3;j++)CHECK(close_enough(tip[j]-base[j],endpoints[v][j]));
 }
}
static void bounded_instances(void){
 reset();unsigned stride=type?0xF8u:0xCCu,identity_offset=type?0xF4u:0xC8u;
 uint32_t piece=id&31u,id_prefix=id&~0x1FFFFFu;
 unsigned char template_instance[0xF8];memcpy(template_instance,(void *)XBOX_PTR(primitive),stride);
 /* Exercise more than the bounded table can hold through REAL native
  * retirement, with distinct adjacent instances sharing exactly one model. */
 MEM32(CELL+(type?0x14u:0x1Cu))=0x840000u+(TREE_FALL_SLOTS+1u)*stride;
 MEM32(vector_address+12)=CHANGES+512;
 for(unsigned index=0;index<=TREE_FALL_SLOTS;index++){
  uint32_t object=0x840000u+index*stride,object_id=id_prefix|(index<<5)|piece;
  memcpy((void *)XBOX_PTR(object),template_instance,stride);
  MEM32(object+identity_offset)=object_id&~31u;MEM32(RECORD+0x104)=object_id;
  s_tree_contact_car=CAR;State before=state();
  CHECK(mm3_tree_use_solid_contact(RECORD,30)&&tree_remove_native());State after=state();
  CHECK(!memcmp(&before,&after,sizeof before));
  CHECK(fall_count()==(index<TREE_FALL_SLOTS?index+1u:TREE_FALL_SLOTS));
 }
 CHECK(MEM32(vector_address+8)==CHANGES+(TREE_FALL_SLOTS+1u)*4u);
 for(unsigned index=0;index<=TREE_FALL_SLOTS;index++){
  uint32_t object=0x840000u+index*stride,mask=MEM32(object+(type?0xCu:8u));
  CHECK(mask==(0xFFFFFFFFu^(1u<<piece)));
  CHECK(mm3_tree_render_mask(object,mask,type)==(index?0xFFFFFFFFu:mask));
 }
 advance(14000);CHECK(!fall_count());
 CHECK(mm3_tree_render_mask(0x840000u,0xFFFFFFFFu^(1u<<piece),type)==(0xFFFFFFFFu^(1u<<piece)));
}
static void clocks(void){
 reset();retire();advance(400);uint64_t before=tree_fall_clock();CHECK(before==400);
 MEM8(0x429DB2)=1;wall_ms+=30000;CHECK(tree_fall_clock()==before);
 for(unsigned i=0;i<3;i++){wall_ms+=20;CHECK(tree_fall_clock()==before);}
 MEM8(0x429DB2)=0;wall_ms+=50;CHECK(tree_fall_clock()==before+50);
 before=tree_fall_clock();wall_ms+=60000;CHECK(tree_fall_clock()==before+100);
 /* Pause can also suppress rendering completely: first resumed draw cannot
  * consume the unseen wall-clock gap and skip the falling animation. */
 before=tree_fall_clock();MEM8(0x429DB2)=1;wall_ms+=60000;MEM8(0x429DB2)=0;
 CHECK(tree_fall_clock()==before+100);before=tree_fall_clock();
 wall_ms-=500;CHECK(tree_fall_clock()==before);wall_ms+=20;CHECK(tree_fall_clock()==before+20);
}
static void nested_draw_scope(void){
 reset();retire();advance(450);nested_requested=1;
 g_esp=STACK;memset((void *)XBOX_PTR(STACK),0,40);
 MEM32(STACK)=type?0x15C05D:0x15E4D9;MEM32(STACK+4)=MODEL;
 MEM32(STACK+24)=MATRIX;MEM32(STACK+36)=id;
 memcpy((void *)XBOX_PTR(MATRIX),baseline,sizeof baseline);
 unsigned before=draw_calls;sub_0016AEDB();nested_requested=0;
 CHECK(draw_calls==before+2&&last_drawing==1&&!mm3_tree_fall_drawing());
 CHECK(!memcmp((const void *)XBOX_PTR(MATRIX),baseline,sizeof baseline));
}
int main(int argc,char **argv){
 CHECK(argc==3);ram=VirtualAlloc(0,RAM_SIZE,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 initial=malloc(RAM_SIZE);expected_ram=malloc(RAM_SIZE);CHECK(ram&&initial&&expected_ram);
 FILE *input=fopen(argv[1],"rb");CHECK(input&&fread(initial,1,RAM_SIZE,input)==RAM_SIZE);fclose(input);
 g_xbox_mem_offset=(ptrdiff_t)ram;_putenv_s("MM3_TREE_TRACE",argv[2]);
 trajectory();impact_motion();caller_and_input_guards();model_variants();streaming_guards();direction_controls();bounded_instances();clocks();nested_draw_scope();
 printf("PASS native falling tree type=%u piece=%u tracing=%s: retirement, grounded geometry/rigidity, native variant selection, bounded speed-dependent impact/slide/rebound, landing/sink, queued-copy ABI, scene guards and pause clock\n",type,id&31u,argv[2]);
 free(expected_ram);free(initial);VirtualFree(ram,0,MEM_RELEASE);return 0;
}
'''


# Test double for the runtime's xbox_GuestRead (tree_read): the host-checked read over this fixture's RAM.
GUEST_READ_DOUBLE = r'''
extern ptrdiff_t g_xbox_mem_offset;
int xbox_GuestRead(uint32_t va, void *out, size_t size)
{
    SIZE_T copied = 0;
    uintptr_t host;
    if (va < 0x10000u || size > UINT32_MAX || va > UINT32_MAX - size) return 0;
    host = (uintptr_t)va + (uintptr_t)g_xbox_mem_offset;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)host, out, size, &copied) && copied == size;
}
'''


def native_source():
    previous = contact.common.ENTRIES
    try:
        contact.common.ENTRIES = ENTRIES
        code = contact.common.native_source().split('int main(int argc,char**argv){', 1)[0]
    finally:
        contact.common.ENTRIES = previous
    # Only vector allocation is modeled. All native cell/ID/piece-mask code
    # stays generated, and the production wrapper must restore these sentinels.
    code = re.sub(r'void sub_0002B969\(void\)\{.*?\}', r'''void sub_0002B969(void){
      uint32_t input=MEM32(g_esp+4),end=MEM32(g_eax+8);
      if(end+4>MEM32(g_eax+12))exit(99);
      MEM32(end)=MEM32(input);MEM32(g_eax+8)=end+4;g_esp+=8;
      g_eax=0xABCDEF00;g_ecx=0xABCD;g_edx=0xDEF;g_ebp=g_seh_ebp=0xDEADBEEF;
      g_fs_base=0;g_fp_top=5;g_fp_cmp=1;g_fp_cc=0x4000;g_fp_control_word=0x37F;g_df=0;
      for(unsigned i=0;i<8;i++)g_fp_stack[i]=-25.5-i;
      memset(&g_xmm0,0,sizeof g_xmm0);memset(&g_xmm7,0,sizeof g_xmm7);
    }''', code, count=1)
    assert 'MEM32(end)=MEM32(input)' in code
    return ('#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include "guest_state.c"\n#include "vec3_rotate.c"\n#include "guest_read.c"\n#include "fixed_step.c"\n'
            'RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7;\n'
            'RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;\n'
            + GUEST_READ_DOUBLE + code + C_TEST)


def apply_render_seam(directory, body, owner):
    original, output = directory / 'before.c', directory / 'after.c'
    original.write_bytes(body.encode())
    script = directory / 'apply.cmake'
    script.write_text(f'''include("{(ROOT / 'scripts/mm3_tree_contact_seam.cmake').as_posix()}")
file(READ "{original.as_posix()}" content)
mm3_add_tree_render(content "{owner}")
file(WRITE "{output.as_posix()}" "${{content}}")
''')
    subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True, text=True)
    return output.read_bytes().decode()


def apply_mesh_seam(directory, body):
    original, output = directory / 'mesh-before.c', directory / 'mesh-after.c'
    original.write_bytes(body.encode())
    script = directory / 'mesh-apply.cmake'
    script.write_text(f'''include("{(ROOT / 'scripts/mm3_tree_contact_seam.cmake').as_posix()}")
file(READ "{original.as_posix()}" content)
mm3_add_tree_mesh(content)
file(WRITE "{output.as_posix()}" "${{content}}")
''')
    subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True, text=True)
    return output.read_bytes().decode()


def check_mesh_seam(directory):
    source = (ROOT / 'src/recomp/gen/recomp_0014.c').read_text()
    body = re.search(r'void sub_0016AEDB\(void\)\n\{.*?\n\}', source, re.S).group()
    expected = '    if (mm3_tree_fall_drawing()) goto loc_0016B072;'
    cases = 0
    for newline in ('\n', '\r\n'):
        retail = body.replace('\n', newline)
        patched = apply_mesh_seam(directory, retail)
        assert patched.count(expected) == 1
        recovered = patched[patched.index('void sub_0016AEDB'):].replace('\r\n', '\n')
        assert recovered.replace(expected + '\n', '') == body
        cases += 1
        for invalid in (
            retail.replace('loc_0016B04E: ;', 'loc_0016B04F: ;'),
            retail.replace('loc_0016B072: ;', 'loc_0016B073: ;'),
            retail + retail,
            patched,
        ):
            try:
                apply_mesh_seam(directory, invalid)
            except subprocess.CalledProcessError as error:
                assert 'MM3 tree mesh:' in error.stderr
            else:
                raise AssertionError('Changed/duplicate/reapplied mesh seam accepted')
            cases += 1
    return cases


REAL_RENDER_TEST = r'''
#define SCENE 0x8A0000u
#define CAMERA 0x8C0000u
#define VIEW 0x8D0000u
#define POOL 0x8E0000u
#define ZERO_VECTOR 0x8F0000u
static void native_draw(uint32_t draw_id){
 sentinels();g_df=0;g_esp=STACK;memset((void *)XBOX_PTR(STACK),0,40);
 MEM32(STACK)=type?0x15C05D:0x15E4D9;MEM32(STACK+4)=MODEL;
 MEM32(STACK+8)=CAMERA;MEM32(STACK+12)=VIEW;MEM32(STACK+24)=MATRIX;
 MEM32(STACK+36)=draw_id;memcpy((void *)XBOX_PTR(MATRIX),baseline,sizeof baseline);
 billboard_calls=mesh_calls=0;memset((void *)XBOX_PTR(POOL),0,0x80);
 CHECK(!mm3_tree_fall_drawing());sub_0016AEDB();
 CHECK(!mm3_tree_fall_drawing()&&g_esp==STACK+40);
 CHECK(!memcmp((const void *)XBOX_PTR(MATRIX),baseline,sizeof baseline));
}
int main(int argc,char **argv){
 CHECK(argc==4);ram=VirtualAlloc(0,RAM_SIZE,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 initial=malloc(RAM_SIZE);expected_ram=malloc(RAM_SIZE);CHECK(ram&&initial&&expected_ram);
 FILE *input=fopen(argv[1],"rb");CHECK(input&&fread(initial,1,RAM_SIZE,input)==RAM_SIZE);fclose(input);
 g_xbox_mem_offset=(ptrdiff_t)ram;_putenv_s("MM3_TREE_TRACE",argv[2]);
 int patched=atoi(argv[3]);reset();retire();advance(450);
 uint32_t owner=type?tree_word(primitive+4):tree_word(tree_word(primitive));
 MEM32(MODEL)=owner;MEM8(owner+0xA0)=1;MEM32(owner+0xA4)=SCENE;
 MEM32(SCENE+0x1521C)=0;MEM32(MODEL+0xC8)=0;MEM8(owner+0xA2)=0;
 const unsigned model_flags[]={0,0x0442};
 for(unsigned flag=0;flag<2;flag++)for(unsigned accepted=0;accepted<2;accepted++){
  MEM32(MODEL+0xC8)=model_flags[flag];
  billboard_result=accepted;
  /* Native billboard submission can return zero after drawing its sprite,
   * so the original path can submit an upright copy AND a falling mesh. */
  native_draw(id);
  CHECK(billboard_calls==(patched?0u:1u));
  CHECK(mesh_calls==(patched||!accepted?1u:0u));
  if(mesh_calls){
   memcpy(queued,(const void *)XBOX_PTR(POOL+8),sizeof queued);
   CHECK(memcmp(queued,baseline,sizeof baseline));
   float anchor[3],foot[3];point(anchor,baseline,base_local);point(foot,queued,base_local);
   CHECK(close_enough(foot[1],anchor[1])&&foot[0]>anchor[0]&&foot[2]>anchor[2]);
   CHECK(close_enough((foot[0]-anchor[0])*4,(foot[2]-anchor[2])*3));
   CHECK(hypotf(foot[0]-anchor[0],foot[2]-anchor[2])<0.25f);
  }
  /* An adjacent instance sharing the same model keeps its native billboard
   * and mesh fallback decision, with an unchanged orientation. */
  native_draw(id^1u);CHECK(billboard_calls==1&&mesh_calls==(!accepted?1u:0u));
  if(mesh_calls)CHECK(!memcmp((const void *)XBOX_PTR(POOL+8),baseline,sizeof baseline));
 }
 printf("PASS real native tree renderer type=%u tracing=%s patched=%d: billboard submission/return paths, native mesh matrix copy, nearby LOD and scoped draw flag\n",type,argv[2],patched);
 free(expected_ram);free(initial);VirtualFree(ram,0,MEM_RELEASE);return 0;
}
'''


def real_renderer_source(directory, patched):
    previous = contact.common.ENTRIES
    try:
        contact.common.ENTRIES = ENTRIES + (0x16AEDB, 0x169AD3, 0x169A15)
        code = contact.common.native_source().split('int main(int argc,char**argv){', 1)[0]
    finally:
        contact.common.ENTRIES = previous
    if patched:
        code = apply_mesh_seam(directory, code).replace('\r\n', '\n')
    code = code.replace('void sub_0016AEDB(void)\n{', 'void sub_0016AEDB_original(void)\n{', 1)
    # Execute the real renderer decision and both native matrix-copy queues.
    # Isolate visibility, effects, allocation and arithmetic leaves only.
    stubs = {
        'sub_0002B969': 'uint32_t input=MEM32(g_esp+4),end=MEM32(g_eax+8);if(end+4>MEM32(g_eax+12))exit(99);MEM32(end)=MEM32(input);MEM32(g_eax+8)=end+4;g_esp+=8;',
        'sub_001C7D01': 'g_esp+=12;',
        'sub_000139C8': 'for(unsigned i=0;i<3;i++)MEM32(g_eax+i*4)=MEM32(g_edx+i*4);g_esp+=4;',
        'sub_001C7B15': 'g_eax=0;g_esp+=8;',
        'sub_0016C29E': 'g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=1.0;g_esp+=4;',
        'sub_0023BB76': 'g_eax=0;g_esp+=20;',
        'sub_00164801': 'g_eax=0;g_esp+=4;',
        'sub_0016A909': 'mesh_calls++;g_eax=0x8E0000u;g_esp+=4;',
        'sub_00168BDD': 'mesh_calls++;g_eax=0x8E0000u;g_esp+=4;',
        'sub_0016946B': 'billboard_calls++;g_eax=billboard_result;g_esp+=8;',
        'sub_000129CD': 'g_eax=0;g_esp+=8;',
        'sub_00019D3A': 'g_eax=0x8F0000u;g_esp+=4;',
    }
    for name, replacement in stubs.items():
        code, count = re.subn(r'void ' + name + r'\(void\)\{.*?\}',
                             lambda match: f'void {name}(void){{{replacement}}}', code, count=1)
        assert count == 1, name
    support = C_TEST.split('void sub_0016AEDB_original(void){', 1)[0]
    for name in ('point', 'close_enough', 'fall_count', 'reset', 'retire', 'advance'):
        support += '\n' + re.search(r'static [^\n]+ ' + name + r'\([^\n]*\)\{.*?(?=\n(?:static |int main|void ))', C_TEST, re.S).group()
    return ('#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include "guest_state.c"\n#include "vec3_rotate.c"\n#include "guest_read.c"\n#include "fixed_step.c"\n'
            'RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7;\n'
            'RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;\n'
            'static unsigned billboard_calls,mesh_calls,billboard_result;\n'
            + GUEST_READ_DOUBLE + code + support + '\nvoid sub_000E1F49_original(void){CHECK(0);}\n'
            'void sub_001D137B_original(void){CHECK(0);}\n' + REAL_RENDER_TEST)


def check_render_seams(directory):
    source = (ROOT / 'src/recomp/gen/recomp_0014.c').read_text()
    cases = 0
    for owner, expected, changed in (
        ('sub_0015BF5A', 'mm3_tree_render_mask(edi, MEM32(edi + 0xC), 1u)',
         'eax = MEM32(ebp + -16);'),
        ('sub_0015D2DD', 'mm3_tree_render_mask(eax, MEM32(eax + 8), 0u)',
         'ecx = MEM32(ebp + -20);'),
    ):
        body = re.search(r'void ' + owner + r'\(void\)\n\{.*?\n\}', source, re.S).group()
        for newline in ('\n', '\r\n'):
            retail = body.replace('\n', newline)
            patched = apply_render_seam(directory, retail, owner)
            assert patched.count(expected) == 1
            # Only the mask expression/declaration changes: loop flags and
            # native calls, including transform construction, remain intact.
            recovered = patched[patched.index('void ' + owner):]
            if owner.endswith('BF5A'):
                recovered = recovered.replace(expected, '(uint32_t)(MEM32(edi + 0xC)) & 0xFFFFFFFFu')
            else:
                recovered = recovered.replace(expected, '(uint32_t)(MEM32(eax + 8)) & 0xFFFFFFFFu')
            assert recovered.replace('\r\n', '\n') == body
            cases += 1
            for invalid, invalid_owner in (
                (retail.replace(changed, changed.replace('MEM32', 'MEM16')), owner),
                (retail + retail, owner),
                (patched, owner),
                (retail, 'sub_0015BCBB'),
            ):
                try:
                    apply_render_seam(directory, invalid, invalid_owner)
                except subprocess.CalledProcessError as error:
                    assert 'MM3 tree render:' in error.stderr
                else:
                    raise AssertionError('Changed/duplicate/reapplied/unknown render seam accepted')
                cases += 1
    for owner in ('sub_0015BCBB', 'sub_0015D03A'):
        body = re.search(r'void ' + owner + r'\(void\)\n\{.*?\n\}', source, re.S).group()
        assert 'mm3_tree_render_mask' not in body  # Collision producers stay native.
    return cases


def main():
    cc.skip_unless_windows('the generated fixtures include windows.h')
    cc.require_compiler()
    image = contact.common.common.Image(str(ROOT / 'game_files/default.xbe'))
    with tempfile.TemporaryDirectory(prefix='mm3-tree-fall-') as temporary:
        directory = Path(temporary)
        seams = check_render_seams(directory) + check_mesh_seam(directory)
        code, executable = directory / 'fall.c', directory / ('fall' + cc.exe_suffix())
        code.write_text(native_source())
        subprocess.run(cc.compile_command([code], executable,
                                          includes=[ROOT / 'src', ROOT / 'tools/xboxrecomp/include', ROOT / 'tools/xboxrecomp/src', ROOT / 'tools/xboxrecomp/src/kernel'],
                                          defines=['_CRT_SECURE_NO_WARNINGS'], std='c11', optimize='none',
                                          dynamic_crt=True, obj=directory / 'fall.obj'), check=True, cwd=directory)
        renderer_executables = []
        for patched in (False, True):
            name = 'native-mesh' if patched else 'native-billboard-control'
            renderer_source, renderer_executable = directory / (name + '.c'), directory / (name + cc.exe_suffix())
            renderer_source.write_text(real_renderer_source(directory, patched))
            subprocess.run(cc.compile_command([renderer_source], renderer_executable,
                                              includes=[ROOT / 'src', ROOT / 'tools/xboxrecomp/include', ROOT / 'tools/xboxrecomp/src', ROOT / 'tools/xboxrecomp/src/kernel'],
                                              defines=['_CRT_SECURE_NO_WARNINGS'], std='c11', optimize='none',
                                              dynamic_crt=True, obj=directory / (name + '.obj')),
                           check=True, cwd=directory)
            renderer_executables.append(renderer_executable)
        cases = renderer_cases = 0
        for kind in (0, 1):
            for piece in (0, 3, 31):
                ram = contact.fixture(image, 2, 30)
                contact.break_fixture(ram, kind=kind, piece=piece)
                # Offset local geometry and a deliberately larger culling box
                # make a wrong bounds choice observably move the hinge.
                struct.pack_into('<6f', ram, MODEL + 8, -1, 2, -3, 3, 12, 1)
                struct.pack_into('<6f', ram, MODEL + 0x20, -10, -8, -11, 13, 18, 9)
                struct.pack_into('<i', ram, MODEL + 0xF0, -1)
                struct.pack_into('<3f', ram, contact.BODY + 0x20, 3, 9, 4)
                ram[0x429DB2] = 0
                struct.pack_into('<12f', ram, MATRIX, 0, 0, -2, 0, 1.5, 0, .75, 0, 0, 120, 4, -75)
                fixture = directory / 'fixture.bin'
                fixture.write_bytes(ram)
                for tracing in ('0', '1', 'contacts'):
                    subprocess.run([str(executable), str(fixture), tracing], check=True)
                    cases += 1
                    if piece == 3:
                        subprocess.run([str(renderer_executables[1]), str(fixture), tracing, '1'], check=True)
                        renderer_cases += 1
                if piece == 3:
                    subprocess.run([str(renderer_executables[0]), str(fixture), '0', '0'], check=True)
                    renderer_cases += 1
        print(f'PASS: {cases} falling-tree native fixture runs; {renderer_cases} real native renderer/negative-control runs; {seams} real generated render-seam/fail-closed cases; no graphics device required.')


if __name__ == '__main__':
    main()
