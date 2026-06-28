#include <cstdio>
#include <cstdarg>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pio_usb.h"
#include "tusb.h"
#include "usb-host/fwUSBHost.h"
extern "C" {
#include "gfx.h"
#include "dvi.h"
#include "input.h"
#include "audio.h"
#include "native_pad.h"
#include "native_io.h"
#include "native_game.h"
}
extern fwUSBHost obUSBHost;
extern "C" int cdc_debug_printf(const char* fmt, ...){ va_list a; va_start(a,fmt); int r=vprintf(fmt,a); va_end(a); return r; }
extern "C" int tusb_debug_buffered_printf(const char* fmt, ...){ va_list a; va_start(a,fmt); int r=vprintf(fmt,a); va_end(a); return r; }
int main(){
    vreg_set_voltage(VREG_VOLTAGE_1_30); sleep_ms(10); set_sys_clock_pll(1260000000,5,1);
    gpio_init(11); gpio_set_dir(11,GPIO_OUT); gpio_put(11,1);
    pio_usb_configuration_t pio_cfg=PIO_USB_DEFAULT_CONFIG; pio_cfg.pin_dp=1; pio_cfg.tx_ch=9; tuh_configure(1,TUH_CFGID_RPI_PIO_USB_CONFIGURATION,&pio_cfg);
    stdio_init_all(); gfx_init(); dvi_init(gfx_get_dvi_buffer());
    bool audio_ok=audio_init(); if(audio_ok) audio_volume(5);
    native_pad_init(); native_io_init(); native_game_init();
    obUSBHost.m_obHID.getKeyboard().setKeyCallback(input_key_callback);
    input_set_modifier_poll([]()->uint8_t{return obUSBHost.m_obHID.getKeyboard().getModifiers();});
    input_set_mouse_poll([](){auto&m=obUSBHost.m_obHID.getMouse(); if(m.isAnyMounted()){int32_t dx,dy,w; m.getDeltas(&dx,&dy,&w); input_mouse_update(dx,dy,w,m.getButtons());}});
    obUSBHost.m_obHID.getController().setReportCallback([](uint8_t dev,uint8_t inst,uint8_t const*rep,uint16_t len){auto&c=obUSBHost.m_obHID.getController(); int p=c.getPlayerForDevice(dev,inst); if(p>1)p=1; uint16_t vid=0,pid=0; tuh_vid_pid_get(dev,&vid,&pid); if(vid==SONY_VID) input_dualsense_report(rep,len,p,pid); else input_gamepad_report(rep,len,p);});
    obUSBHost.m_obXInput.setReportCallback([](uint8_t dev,uint8_t inst,xinput_gamepad_t const*pad){int p=obUSBHost.m_obXInput.getPlayerForDevice(dev,inst); if(p>1)p=1; input_xinput_update(pad->wButtons,pad->sThumbLX,pad->sThumbLY,p); native_pad_update_xinput(p,pad->wButtons,pad->sThumbLX,pad->sThumbLY,pad->sThumbRX,pad->sThumbRY,pad->bLeftTrigger,pad->bRightTrigger);});
    printf("fruitjam_railshooter native @ %u MHz\n",(unsigned)(clock_get_hz(clk_sys)/1000000));
    absolute_time_t scan_end=make_timeout_time_ms(3000); while(!time_reached(scan_end)){tuh_task(); input_update(); sleep_ms(1);} 
    absolute_time_t next=get_absolute_time(); uint32_t last=to_ms_since_boot(next);
    while(true){ tuh_task(); input_update(); uint32_t now=to_ms_since_boot(get_absolute_time()); float dt=(now-last)/1000.0f; if(dt<0.001f)dt=0.001f; if(dt>0.05f)dt=0.05f; last=now; native_game_frame(dt,now); next=delayed_by_us(next,16667); sleep_until(next); if(absolute_time_diff_us(get_absolute_time(),next)<-20000) next=get_absolute_time(); }
}
