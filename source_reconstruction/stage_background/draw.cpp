#include "background.hpp"
#include "object_draw.hpp"
#include "../runtime_state/state.hpp"
#include "../platform_window/graphics_callbacks.hpp"
#include "../sprite_renderer/dispatch.hpp"
#include "../sprite_renderer/draw.hpp"
#include <bit>
namespace th20::source::background {
namespace pe=program_entry;namespace pw=platform_window;namespace sp=sprite;
namespace unrecovered {
void create_transition_effect(int mode,int duration,int red,int green,int blue,int priority); //4250c0 ->44-byte fade owner
}
namespace {
void reset_overlay(sp::Controller& c){c.field_7d40e90=0;c.field_7d40e8c=0x80808080;}
void copy_camera(Background& background){
    background.state.camera.points[0][0]=pe::graphics_state.viewports[3].points[2][0];
    background.state.camera.points[0][1]=pe::graphics_state.viewports[3].points[2][1];
    pe::graphics_state.viewports[3]=background.state.camera;select_background_viewport(pe::graphics_state,3);
}
void fog_settings(Background& background){auto& g=pe::graphics_state;
    pw::set_render_state(g,D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
    pw::set_render_state(g,D3DRS_FOGCOLOR,background.state.camera.final_state[6]);
    pw::set_render_state(g,D3DRS_FOGSTART,background.state.camera.final_state[0]);
    pw::set_render_state(g,D3DRS_FOGEND,background.state.camera.final_state[1]);
}
}
int Background::draw_geometry(){
    if(state_flags&8)return 1;auto& g=pe::graphics_state;auto& c=*pe::sprite_controller;
    if(!(state_flags&4)||fade_timer.current<60){
        sp::flush_textured_quads(c,*g.device);copy_camera(*this);sp::draw_environment::enable_depth_write();fog_settings(*this);
        const auto& viewport=g.viewports[3].viewport;
        D3DRECT rectangle{std::bit_cast<std::int32_t>(viewport.X),std::bit_cast<std::int32_t>(viewport.Y),std::bit_cast<std::int32_t>(viewport.X+viewport.Width),std::bit_cast<std::int32_t>(viewport.Y+viewport.Height)};
        const auto color=(!(state_flags&4)||frame_count>33)?state.camera.final_state[6]:0xff000000u;
        g.device->Clear(1,&rectangle,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,color,1.f,0);
    }
    if(state_flags&4){if(fade_timer.current<30){unrecovered::create_transition_effect(3,30,0,0,0,10);state_flags|=1;recovered::timer_set(fade_timer,1);}
        else{state.overlay_color&=0xffffff;state_flags&=~1u;}}
    if(state.overlay_color>>24){c.field_7d40e90=1;c.field_7d40e8c=state.overlay_color;state.overlay_color&=0xffffff;}
    rendered[0]=rendered[1]=rendered[2]=0;
    if(state_flags&1){sp::draw_environment::enable_fog();for(int layer=0;layer<8;++layer)draw_object_layer(*this,layer);sp::flush_textured_quads(c,*g.device);}
    reset_overlay(c);pw::disable_depth_write(g);pw::set_render_state(g,D3DRS_ZFUNC,D3DCMP_ALWAYS);return 1;
}
int Background::draw_foreground(){
    if(state_flags&8)return 1;auto& g=pe::graphics_state;auto& c=*pe::sprite_controller;
    if(!(state_flags&4)||fade_timer.current<60){
        sp::flush_textured_quads(c,*g.device);copy_camera(*this);pw::disable_fog(g);pw::disable_depth_write(g);
        pw::set_render_state(g,D3DRS_ZFUNC,D3DCMP_ALWAYS);sp::draw_animation_layer(c,39);
        pw::set_render_state(g,D3DRS_ZFUNC,D3DCMP_LESSEQUAL);sp::draw_animation_layer(c,40);fog_settings(*this);
    }
    if((state_flags&4)&&fade_timer.current>29)state.overlay_color&=0xffffff;
    if(state_flags&1){pw::disable_depth_write(g);sp::draw_environment::enable_fog();for(int layer=8;layer<12;++layer)draw_object_layer(*this,layer);sp::flush_textured_quads(c,*g.device);}
    reset_overlay(c);
    if(fade_timer.current>0){recovered::timer_add(fade_timer,-1.f,th20::source::state::timer_rate);
        if(fade_timer.current<=0){state.overlay_color|=0xff000000u;if(state_flags&2)state_flags|=8;state.overlay_color=0xffffff;state_flags&=~6u;}}
    pw::disable_depth_write(g);pw::set_render_state(g,D3DRS_ZFUNC,D3DCMP_ALWAYS);pw::disable_fog(g);return 1;
}
}
