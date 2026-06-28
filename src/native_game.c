#include "native_game.h"
#include "native_pad.h"
#include "native_io.h"
#include "wire3d.h"
#include "gfx.h"
#include "input.h"
#include "audio.h"
#include <stdio.h>
#include <math.h>
#define MAX_ENEMIES 28
#define MAX_FX 48
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
typedef struct{bool alive;uint8_t mesh_id;float x,y,z,vz,scale;int hp,score_value,color;float phase;}Enemy; typedef struct{bool active;float x,y,life;int color;}Fx; enum{TITLE,PLAYING,PAUSED,GAMEOVER};
static Enemy en[MAX_ENEMIES]; static Fx fx[MAX_FX]; static int state,score,ammo,lives,wave,credits; static float aimx,aimy,spawn_t,scroll; static bool prev_fire,prev_reload,prev_start,prev_back,debug; static uint32_t frames,last_fps_ms; static float fps;
static float norm(int16_t v){float f=v<0?(float)v/32768.0f:(float)v/32767.0f; return fabsf(f)<0.18f?0:f;}
static void reset_game(void){for(int i=0;i<MAX_ENEMIES;i++)en[i].alive=false; for(int i=0;i<MAX_FX;i++)fx[i].active=false; score=0;ammo=8;lives=3;wave=1;aimx=64;aimy=64;spawn_t=.5f;scroll=0;native_io_game_over(false);} 
void native_game_init(void){state=TITLE;credits=0;debug=false;reset_game();}
int native_game_live_enemies(void){int n=0;for(int i=0;i<MAX_ENEMIES;i++)if(en[i].alive)n++;return n;}
static void add_fx(float x,float y,int c){for(int i=0;i<MAX_FX;i++)if(!fx[i].active){fx[i]=(Fx){true,x,y,.25f,c};return;}}
static void spawn_enemy(void){for(int i=0;i<MAX_ENEMIES;i++)if(!en[i].alive){int boss=(wave%5==0&&native_game_live_enemies()==0); en[i]=(Enemy){true,(uint8_t)(boss?3:(i+wave)%3),((i*37+wave*11)%80-40)/16.0f,((i*17+wave*3)%44-22)/18.0f,22.0f+(i%5),2.1f+wave*.12f,boss?1.4f:.75f,boss?5:1,boss?500:100,boss?10:12,0};return;}}
static void tone(int ch,float f,int ms,int w){audio_tone(ch,f,ms,w);} 
static void fire(void){ if(ammo<=0){tone(0,110,80,WAVE_SQUARE);return;} ammo--; native_io_fire(); tone(0,280,45,WAVE_SQUARE); int best=-1; float bz=999; for(int i=0;i<MAX_ENEMIES;i++) if(en[i].alive){float sx,sy; if(wire3d_project((Vec3){en[i].x,en[i].y,en[i].z},&sx,&sy)){float r=700.0f/en[i].z*en[i].scale; float dx=sx-aimx,dy=sy-aimy; if(dx*dx+dy*dy<r*r && en[i].z<bz){best=i;bz=en[i].z;}}} if(best>=0){float sx,sy;wire3d_project((Vec3){en[best].x,en[best].y,en[best].z},&sx,&sy); add_fx(sx,sy,10); native_io_hit(); tone(1,760,70,WAVE_TRIANGLE); if(--en[best].hp<=0){score+=en[best].score_value; en[best].alive=false; if(score/1000+1>wave)wave=score/1000+1;}}}
static void reload(void){ammo=8;tone(2,440,80,WAVE_TRIANGLE);}
static void draw_cross(void){int x=(int)aimx,y=(int)aimy; gfx_circ(x,y,5,8); gfx_line(x-9,y,x-3,y,7);gfx_line(x+3,y,x+9,y,7);gfx_line(x,y-9,x,y-3,7);gfx_line(x,y+3,x,y+9,7);} 
void native_game_frame(float dt,uint32_t now){frames++; if(now-last_fps_ms>=1000){fps=frames*1000.0f/(now-last_fps_ms);frames=0;last_fps_ms=now;} NativePadState p=native_pad_get(0); bool raw=p.connected; uint16_t b=p.buttons; bool fire_now=(raw&&p.rt>50)||(raw&&(b&XINPUT_GAMEPAD_A))||input_btn(4,0); bool reload_now=(raw&&p.lt>50)||(raw&&(b&XINPUT_GAMEPAD_B))||input_btn(5,0); bool start_now=(raw&&(b&XINPUT_GAMEPAD_START))||input_btn(6,0)||native_io_start_pressed(); bool back_now=(raw&&(b&XINPUT_GAMEPAD_BACK))||native_io_service_pressed(); if(native_io_coin_pressed())credits++;
 if(start_now&&!prev_start){ if(state==TITLE||state==GAMEOVER){reset_game();state=PLAYING;} else state=(state==PLAYING)?PAUSED:PLAYING; } if(back_now&&!prev_back)debug=!debug; if(reload_now&&!prev_reload)reload(); if(fire_now&&!prev_fire&&state==PLAYING)fire(); prev_fire=fire_now;prev_reload=reload_now;prev_start=start_now;prev_back=back_now;
 int ax,ay; if(native_io_adc_aim(&ax,&ay)){aimx=ax;aimy=ay;} float nx=raw?norm(p.lx):0, ny=raw?norm(p.ly):0; if(!raw){ if(input_btn(0,0))nx-=1; if(input_btn(1,0))nx+=1; if(input_btn(2,0))ny+=1; if(input_btn(3,0))ny-=1;} aimx+=nx*90*dt; aimy-=ny*90*dt; if(aimx<0)aimx=0;if(aimx>127)aimx=127;if(aimy<0)aimy=0;if(aimy>127)aimy=127;
 if(state==PLAYING){scroll+=dt*5; spawn_t-=dt; if(spawn_t<=0){spawn_enemy(); spawn_t=1.1f-fminf(.65f,wave*.04f);} for(int i=0;i<MAX_ENEMIES;i++)if(en[i].alive){en[i].z-=en[i].vz*dt; en[i].phase+=dt; if(en[i].z<1.1f){en[i].alive=false; lives--; tone(3,90,180,WAVE_SAW); if(lives<=0){state=GAMEOVER; native_io_game_over(true);}}} for(int i=0;i<MAX_FX;i++)if(fx[i].active&&(fx[i].life-=dt)<=0)fx[i].active=false; }
 gfx_cls(0); wire3d_draw_corridor(scroll); for(int pass=0;pass<MAX_ENEMIES;pass++){int best=-1;float bz=-1;for(int i=0;i<MAX_ENEMIES;i++)if(en[i].alive&&en[i].z>bz){bz=en[i].z;best=i;} if(best<0)break; wire3d_draw_mesh(en[best].mesh_id,en[best].x,en[best].y,en[best].z,en[best].scale,en[best].color); en[best].z=-en[best].z; } for(int i=0;i<MAX_ENEMIES;i++)if(en[i].alive&&en[i].z<0)en[i].z=-en[i].z; for(int i=0;i<MAX_FX;i++)if(fx[i].active)gfx_circ((int)fx[i].x,(int)fx[i].y,(int)(2+20*fx[i].life),fx[i].color); draw_cross(); char buf[64]; snprintf(buf,sizeof buf,"sc%05d am%d lv%d w%d",score,ammo,lives,wave); gfx_print(buf,1,1,7); if(state==TITLE){gfx_print("FRUIT JAM",33,42,10);gfx_print("RAILSHOOTER",27,52,12);gfx_print("START TO PLAY",25,72,7);} if(state==PAUSED)gfx_print("PAUSED",49,61,10); if(state==GAMEOVER)gfx_print("GAME OVER",39,58,8); if(debug){snprintf(buf,sizeof buf,"lx%d ly%d lt%d rt%d",p.lx,p.ly,p.lt,p.rt);gfx_print(buf,1,112,11);snprintf(buf,sizeof buf,"b%04x fps%02d e%d %s",p.buttons,(int)fps,native_game_live_enemies(),raw?"xinput":"digital");gfx_print(buf,1,120,11);} gfx_flip(); native_io_update(now); }
