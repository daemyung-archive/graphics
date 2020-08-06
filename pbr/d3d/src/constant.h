//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#ifndef PBR_CONSTANT_H_
#define PBR_CONSTANT_H_

#include <DirectXMath.h>

using DirectX::XMFLOAT2;
using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;

struct Constant {
	XMFLOAT4X4 p;
	XMFLOAT4X4 v;
	XMFLOAT4X4 m;
	XMFLOAT3 light;
	float dummy1;
	XMFLOAT3 light_color;
	float dummy2;
	XMFLOAT3 camera;
	float dummy3;
};

#endif // PBR_CONSTANT_H_
