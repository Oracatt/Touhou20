#include "render_state.hpp"
#include <cstring>
namespace th20::source::sprite {
void apply_animation_render_state(Controller& c,Animation& a,IDirect3DDevice9& device) {
    auto flush=[&]{flush_textured_quads(c,device);};
    auto render=[&](unsigned state,DWORD value){device.SetRenderState(static_cast<D3DRENDERSTATETYPE>(state),value);};
    auto sampler=[&](unsigned state,DWORD value){device.SetSamplerState(0,static_cast<D3DSAMPLERSTATETYPE>(state),value);};
    const auto blend=static_cast<std::uint8_t>(a.base.flags[0]>>8);
    if(c.blend_mode!=blend){flush();c.blend_mode=blend;render(15,1);render(206,1);render(207,2);render(208,1);render(209,1);
        unsigned source=5,destination=6,operation=1;
        switch(blend){case 1:destination=2;break;case 2:destination=2;operation=3;break;case 3:render(15,0);source=2;destination=1;break;case 4:source=10;destination=4;break;case 5:source=9;destination=1;break;case 6:source=4;break;case 7:source=7;destination=8;break;case 8:destination=2;operation=4;break;case 9:destination=2;operation=5;break;default:break;}
        render(19,source);render(20,destination);render(171,operation);
    }
    const auto filter=static_cast<std::uint8_t>((a.base.flags[2]>>2)&3);
    if(c.field_e12!=filter){flush();c.field_e12=filter;sampler(5,filter==0?2:1);sampler(6,filter==0?2:1);}
    const auto u=static_cast<std::uint8_t>((a.base.flags[2]>>16)&3);
    if(c.field_e13!=u){flush();c.field_e13=u;if(u<3)sampler(1,u==0?1:(u==1?3:2));}
    const auto v=static_cast<std::uint8_t>((a.base.flags[2]>>13)&3);
    if(c.field_e14!=v){flush();c.field_e14=v;if(v<3)sampler(2,v==0?1:(v==1?3:2));}
    ++c.field_c0;
}
void select_texture_combine(Controller& c,IDirect3DDevice9& device,std::uint8_t mode) {
    if(c.field_00==mode)return;flush_textured_quads(c,device);
    const unsigned states[6]{5,6,2,3,4,1};unsigned values[6]{2,0,2,0,4,4};
    switch(mode){case 1:values[1]=values[3]=3;break;case 2:values[0]=values[1]=values[2]=values[3]=0;values[4]=values[5]=2;break;case 3:values[4]=3;break;default:break;}
    for(unsigned i=0;i<6;++i)device.SetTextureStageState(0,static_cast<D3DTEXTURESTAGESTATETYPE>(states[i]),values[i]);c.field_00=mode;
}
std::int32_t append_textured_quad(Controller& c,const Vertex28 (&vertices)[4]) noexcept {
    // Exact original upper bound leaves six vertices unused at the pool end.
    const auto limit=reinterpret_cast<std::uintptr_t>(&c)+0x7c00dcc;
    if(reinterpret_cast<std::uintptr_t>(c.textured_write)>=limit)return 1;
    auto* destination=c.textured_write;
    std::memcpy(destination,vertices,sizeof(Vertex28));std::memcpy(destination+3,vertices+1,sizeof(Vertex28));std::memcpy(destination+1,destination+3,sizeof(Vertex28));
    std::memcpy(destination+4,vertices+2,sizeof(Vertex28));std::memcpy(destination+2,destination+4,sizeof(Vertex28));std::memcpy(destination+5,vertices+3,sizeof(Vertex28));
    c.textured_write+=6;++c.quad_count;return 0;
}
}
