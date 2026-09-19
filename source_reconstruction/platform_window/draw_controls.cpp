#include "graphics_callbacks.hpp"
#include "../sprite_renderer/quad.hpp"
namespace th20::source::platform_window {
namespace {
void flush_draws(){auto& g=program_entry::graphics_state;sprite::flush_textured_quads(*program_entry::sprite_controller,*g.device);}
}
HRESULT set_render_state(GraphicsStatePrefix& g,D3DRENDERSTATETYPE state,DWORD value){ //4d9db0
    flush_draws();return g.device->SetRenderState(state,value);
}
HRESULT disable_depth_write(GraphicsStatePrefix& g){ //4ddf80
    if(!g.field_0dbc)return D3D_OK;
    flush_draws();g.field_0dbc=0;return g.device->SetRenderState(D3DRS_ZWRITEENABLE,0);
}
HRESULT disable_fog(GraphicsStatePrefix& g){ //4dda60
    if(!g.render_value)return D3D_OK;
    // This is the same4455c0 source previously reached through reset_sprite_queue.
    flush_draws();g.render_value=0;return g.device->SetRenderState(D3DRS_FOGENABLE,FALSE);
}
}
