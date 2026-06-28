#include "wire3d.h"
#include "gfx.h"
#define NEAR_Z 0.8f
static const Vec3 cube_v[]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
static const unsigned char cube_e[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
static const Vec3 pyr_v[]={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},{0,0,1.5f}}; static const unsigned char pyr_e[][2]={{0,1},{1,2},{2,3},{3,0},{0,4},{1,4},{2,4},{3,4}};
static const Vec3 bug_v[]={{-1,0,0},{1,0,0},{0,0,1},{0,0,-1},{-.7f,.7f,0},{.7f,.7f,0},{-.7f,-.7f,0},{.7f,-.7f,0}}; static const unsigned char bug_e[][2]={{0,1},{2,3},{0,4},{4,2},{1,5},{5,2},{0,6},{6,3},{1,7},{7,3}};
bool wire3d_project(Vec3 v,float*sx,float*sy){ if(v.z<=NEAR_Z)return false; float f=58.0f/v.z; *sx=64.0f+v.x*f; *sy=64.0f-v.y*f; return true; }
void wire3d_line(Vec3 a,Vec3 b,int c){ float x0,y0,x1,y1; if(!wire3d_project(a,&x0,&y0)||!wire3d_project(b,&x1,&y1))return; gfx_line((int)x0,(int)y0,(int)x1,(int)y1,c); }
void wire3d_draw_mesh(int id,float x,float y,float z,float s,int c){ const Vec3*v=cube_v; const unsigned char(*e)[2]=cube_e; int n=12; if(id==1){v=pyr_v;e=pyr_e;n=8;} else if(id==2){v=bug_v;e=bug_e;n=10;} for(int i=0;i<n;i++){ Vec3 a={x+v[e[i][0]].x*s,y+v[e[i][0]].y*s,z+v[e[i][0]].z*s}; Vec3 b={x+v[e[i][1]].x*s,y+v[e[i][1]].y*s,z+v[e[i][1]].z*s}; wire3d_line(a,b,c);} if(id==3){wire3d_draw_mesh(0,x,y,z,s*1.4f,c);wire3d_draw_mesh(1,x,y,z+s*1.8f,s,c);} }
void wire3d_draw_corridor(float scroll){ for(int i=0;i<14;i++){ float z=1.5f+i*2.0f-(scroll-(int)(scroll/2)*2); wire3d_line((Vec3){-4,-2,z},(Vec3){4,-2,z},5); wire3d_line((Vec3){-4,2,z},(Vec3){4,2,z},1);} for(int x=-4;x<=4;x+=2){wire3d_line((Vec3){(float)x,-2,1},(Vec3){(float)x,-2,30},13);wire3d_line((Vec3){(float)x,2,1},(Vec3){(float)x,2,30},1);} }
