#include "native_io.h"
#include "pico/stdlib.h"
#if ENABLE_ADC_AIM
#include "hardware/adc.h"
#endif
static uint32_t recoil_until, muzzle_until; static bool go_blink;
static bool active_low_pressed(uint gpio){ return !gpio_get(gpio); }
void native_io_init(void){
#if ENABLE_ARCADE_IO
 const uint outs[]={RECOIL_GPIO,MUZZLE_GPIO,STATUS_LED_GPIO}; for(uint i=0;i<3;i++){gpio_init(outs[i]);gpio_set_dir(outs[i],GPIO_OUT);} gpio_put(RECOIL_GPIO,0); gpio_put(MUZZLE_GPIO,0); gpio_put(STATUS_LED_GPIO,1);
 const uint ins[]={SERVICE_GPIO,START_BUTTON_GPIO,COIN_BUTTON_GPIO}; for(uint i=0;i<3;i++){gpio_init(ins[i]);gpio_set_dir(ins[i],GPIO_IN);gpio_pull_up(ins[i]);}
#endif
#if ENABLE_ADC_AIM
 adc_init(); adc_gpio_init(ADC_PAN_GPIO); adc_gpio_init(ADC_TILT_GPIO);
#endif
}
void native_io_update(uint32_t now_ms){
#if ENABLE_ARCADE_IO
 gpio_put(RECOIL_GPIO, now_ms<recoil_until); gpio_put(MUZZLE_GPIO, now_ms<muzzle_until); if(go_blink) gpio_put(STATUS_LED_GPIO, ((now_ms/250)&1)?1:0);
#endif
}
void native_io_fire(void){ uint32_t n=to_ms_since_boot(get_absolute_time()); recoil_until=n+45; muzzle_until=n+70; }
void native_io_hit(void){ gpio_put(STATUS_LED_GPIO,0); }
void native_io_game_over(bool on){ go_blink=on; if(!on) gpio_put(STATUS_LED_GPIO,1); }
bool native_io_start_pressed(void){ return active_low_pressed(START_BUTTON_GPIO); } bool native_io_coin_pressed(void){ return active_low_pressed(COIN_BUTTON_GPIO); } bool native_io_service_pressed(void){ return active_low_pressed(SERVICE_GPIO); }
bool native_io_adc_aim(int*x,int*y){
#if ENABLE_ADC_AIM
 const int pan_min=200,pan_max=3900,tilt_min=200,tilt_max=3900; adc_select_input(0); uint16_t pan=adc_read(); adc_select_input(1); uint16_t tilt=adc_read(); if(pan<pan_min)pan=pan_min; if(pan>pan_max)pan=pan_max; if(tilt<tilt_min)tilt=tilt_min; if(tilt>tilt_max)tilt=tilt_max; *x=(pan-pan_min)*127/(pan_max-pan_min); *y=(tilt-tilt_min)*127/(tilt_max-tilt_min); return true;
#else
 (void)x;(void)y; return false;
#endif
}
