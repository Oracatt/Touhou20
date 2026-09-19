#include "platform_window.hpp"
#include "data_constants.hpp"
#include <cstring>

namespace th20::source::platform_window {
void initialize_render_state() {
    // Preserve the order of the original COM writes; return HRESULTs are ignored.
    auto device=[] {return program_entry::graphics_state.device;};
    const struct {unsigned state,value;} states[]={
        {7,1},{0x89,0},{0x16,1},{0x1b,1},{0xce,1},{9,2},{0x13,5},{0x14,6},
        {0x17,8},{0xab,1},{0xcf,2},{0xd0,1},{0xd1,1},{0xf,1},{0x18,1},
        {0x19,7},{0x1c,1},{0x26,0x3f800000},{0x23,0},{0x8c,3},
        {0x22,0xffa0a0a0},{0x24,0x447a0000},{0x25,0x459c4000},{0xa1,0}};
    for(auto entry:states) device()->SetRenderState(static_cast<D3DRENDERSTATETYPE>(entry.state),entry.value);
    const struct {unsigned state,value;} texture[]={{4,4},{5,2},{6,3},{1,4},{2,2},{3,3},{0x18,2},{0xb,0}};
    for(auto entry:texture) device()->SetTextureStageState(0,static_cast<D3DTEXTURESTAGESTATETYPE>(entry.state),entry.value);
    const struct {unsigned state,value;} sampler[]={{7,0},{5,2},{6,2},{3,3},{1,1},{2,1}};
    for(auto entry:sampler) device()->SetSamplerState(0,static_cast<D3DSAMPLERSTATETYPE>(entry.state),entry.value);
    if(program_entry::sprite_controller) {
        // Five real field setters: 41db40/60/80/a0/c0. The renderer owns the
        // allocation and preceding 96 MiB; no guessed allocation is made here.
        auto& sprite=*program_entry::sprite_controller;
        sprite.blend_mode=0x0b;sprite.unknown_cached_e0d=0xff;sprite.unknown_cached_e0e=0xff;
        sprite.cached_texture=0xffffffff;sprite.unknown_cached_e10=0xff;
    }
}
}
namespace th20::source::program_entry::unrecovered {
void fn_0041a2c0() {platform_window::initialize_render_state();}
}
