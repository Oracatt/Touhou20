#include "platform_window.hpp"
#include <cstring>
#include <emmintrin.h>

namespace th20::source::platform_window {
namespace pe=program_entry;
namespace {
void assign_float(std::uint32_t& destination,float value) {std::memcpy(&destination,&value,4);}
float from_signed(int value) {return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(),value));}
}
void apply_camera(pe::ViewportState& viewport) { // 4da120
    auto* controller=pe::sprite_controller;
    auto& device=*pe::graphics_state.device;
    if(controller) sprite::flush_textured_quads(*controller,device);
    device.SetTransform(D3DTS_VIEW,&viewport.view);
    device.SetTransform(D3DTS_PROJECTION,&viewport.projection);
    if(controller) {
        assign_float(controller->fields_c8[0],viewport.points[0][0]);
        assign_float(controller->fields_c8[1],viewport.points[0][1]);
    }
}
void select_viewport(GraphicsStatePrefix& graphics,int index) { // 41dce0
    auto& viewport=graphics.viewports[index];
    graphics.current_viewport=&viewport;
    update_camera(viewport,viewport.viewport);apply_camera(viewport);
    graphics.device->SetViewport(&viewport.adjusted_viewport);
    auto& controller=*pe::sprite_controller;
    assign_float(controller.fields_c8[2],from_signed(viewport.offset_x));
    assign_float(controller.fields_c8[3],from_signed(viewport.offset_y));
    assign_float(controller.fields_c8[0],viewport.points[2][0]);
    assign_float(controller.fields_c8[1],viewport.points[2][1]);
    graphics.field_0b04=0;
}
}
namespace th20::source::program_entry::unrecovered {
void select_viewport(GraphicsStatePrefix& graphics,int index) {platform_window::select_viewport(graphics,index);}
}
