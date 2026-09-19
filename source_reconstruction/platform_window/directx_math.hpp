#pragma once
#include <d3d9.h>
namespace th20::source::platform_window::directx {
struct Vector3 {float x,y,z;};
// These are normal DirectX SDK library calls used by the original program.
// They are not calls into the game's executable or retained game machine code.
void look_at(D3DMATRIX&,const Vector3& eye,const Vector3& target,const Vector3& up);
void perspective(D3DMATRIX&,float fov,float aspect,float near_plane,float far_plane);
HRESULT copy_surface(IDirect3DSurface9* destination,const RECT&,IDirect3DSurface9* source,const RECT&);
}
