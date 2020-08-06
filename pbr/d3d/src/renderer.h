//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#ifndef PBR_RENDERER_H_
#define PBR_RENDERER_H_

#include <unordered_map>
#include <string>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#include "mesh.h"
#include "constant.h"

//----------------------------------------------------------------------------------------------------------------------

using Microsoft::WRL::ComPtr;

//----------------------------------------------------------------------------------------------------------------------

constexpr auto kNumBackBuffer = 3;

//----------------------------------------------------------------------------------------------------------------------

class Renderer {
public:
	Renderer(HWND window);

	~Renderer();

	void Update(double delta_time);

	void Render();

private:
	void InitFactory();

	void InitDevice();

	void InitCommandQueue();

	void InitFence();

	void InitSwapChain();

	void InitRTVs();

	void InitDepthTexture();

	void InitDSV();

	void InitCommandAllocators();

	void InitCommandList();

	void InitConstantBuffer();

	void InitSphereAndTextures();

	void InitSRVs();

	void InitPipeline();

	void WaitCommandQueueIdle();

	void WaitCommandQueueIdle(ID3D12CommandQueue* command_queue);

	UINT64 GetCurrentFenceValue();

	ID3D12Resource* GetCurrentBackBuffer();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV();

	ID3D12CommandAllocator* GetCurrentCommandAllocator();

private:
	HWND window_;
	UINT width_;
	UINT height_;
	ComPtr<IDXGIFactory4> factory_;
	ComPtr<ID3D12Device> device_;
	ComPtr<ID3D12CommandQueue> command_queue_;
	UINT64 frame_count_;
	UINT64 fence_values_[kNumBackBuffer];
	ComPtr<ID3D12Fence> fence_;
	ComPtr<IDXGISwapChain3> swap_chain_;
	ComPtr<ID3D12Resource> back_buffers_[kNumBackBuffer];
	ComPtr<ID3D12DescriptorHeap> rtv_heap_;
	UINT rtv_size_;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs_[kNumBackBuffer];
	ComPtr<ID3D12Resource> depth_texture_;
	ComPtr<ID3D12DescriptorHeap> dsv_heap_;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv_;
	ComPtr<ID3D12CommandAllocator> command_allocators_[kNumBackBuffer];
	ComPtr<ID3D12GraphicsCommandList> command_list_;
	Mesh sphere_;
	std::vector<ComPtr<ID3D12Resource>> textures_;
	ComPtr<ID3D12DescriptorHeap> srv_heap_;
	UINT srv_size_;
	ComPtr<ID3D12RootSignature> root_signature_;
	ComPtr<ID3D12PipelineState> pso_;	
	ComPtr<ID3D12Resource> constant_buffer_;
	D3D12_VIEWPORT viewport_;
	D3D12_RECT scissor_rect_;
};

//----------------------------------------------------------------------------------------------------------------------

#endif // PBR_RENDERER_H_