//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#ifndef PBR_MESH_H_
#define PBR_MESH_H_

#include <wrl.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

struct Mesh {
	ComPtr<ID3D12Resource> vertex_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	ComPtr<ID3D12Resource> index_buffer;
	D3D12_INDEX_BUFFER_VIEW index_buffer_view;
	UINT draw_count;
};

#endif // PBR_MESH_H_
