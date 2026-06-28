#include "native_pad.h"
static NativePadState pads[NATIVE_PAD_MAX];
void native_pad_init(void){ for(int i=0;i<NATIVE_PAD_MAX;i++) pads[i]=(NativePadState){0}; }
void native_pad_update_xinput(int player,uint16_t buttons,int16_t lx,int16_t ly,int16_t rx,int16_t ry,uint8_t lt,uint8_t rt){ if(player<0||player>=NATIVE_PAD_MAX)return; NativePadState*p=&pads[player]; p->connected=true; p->frame_counter++; p->buttons=buttons; p->lx=lx; p->ly=ly; p->rx=rx; p->ry=ry; p->lt=lt; p->rt=rt; }
NativePadState native_pad_get(int player){ if(player<0||player>=NATIVE_PAD_MAX){NativePadState z={0};return z;} return pads[player]; }
