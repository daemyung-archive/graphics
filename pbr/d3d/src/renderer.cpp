//
// This file is part of the "graphics" project
// See "LICENSE" for license information.
//

#include "renderer.h"

#include <DirectXMath.h>

#include "d3dx12.h"
#include "GeometryGenerator.h"
#include "d3d_helper.h"
#include "stb_lib.h"

//----------------------------------------------------------------------------------------------------------------------

using namespace DirectX;

//----------------------------------------------------------------------------------------------------------------------

constexpr auto kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr auto kDepthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

//----------------------------------------------------------------------------------------------------------------------

Renderer::Renderer(HWND window) :
	window_(window),
	frame_count_(0) {
	RECT rect;
	GetClientRect(window_, &rect);

	width_ = rect.right - rect.left;
	height_ = rect.bottom - rect.top;

	InitFactory();
	InitDevice();
	InitCommandQueue();
	InitFence();
	InitSwapChain();
	InitRTVs();
	InitDepthTexture();
	InitDSV();
	InitCommandAllocators();
	InitCommandList();
	InitConstantBuffer();
	InitSphereAndTextures();
	InitSRVs();
	InitPipeline();

	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;
	viewport_.Width = width_;
	viewport_.Height = height_;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	scissor_rect_.left = 0;
	scissor_rect_.top = 0;
	scissor_rect_.right = width_;
	scissor_rect_.bottom = height_;
}

//----------------------------------------------------------------------------------------------------------------------

Renderer::~Renderer() {
	WaitCommandQueueIdle();
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::Update(double delta_time) {
	auto fence_value = GetCurrentFenceValue();

	if (fence_->GetCompletedValue() < fence_value) {
		HANDLE event = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		THROW_IF_FAILED(
			fence_->SetEventOnCompletion(fence_value, event)
		);

		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	void* data;
	THROW_IF_FAILED(
		constant_buffer_->Map(0, nullptr, &data)
	);

	auto offset = AlignPow2(sizeof(Constant), 256) * swap_chain_->GetCurrentBackBufferIndex();
	Constant* constant = reinterpret_cast<Constant*>(static_cast<UINT8*>(data) + offset);

	auto p = XMMatrixPerspectiveFovLH(90.0f, 1.0f, 0.001f, 100.0f);
	XMStoreFloat4x4(&constant->p, p);

	auto v = XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, -1.5f, 1.0f), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMStoreFloat4x4(&constant->v, v);

	auto m = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	XMStoreFloat4x4(&constant->m, m);

	static float theta = 0.0f;
	static float theta_acc = 0.0001f;
	auto light = XMVector3Normalize(XMVectorSet(-cos(theta), 0.0f, -sin(theta), 0.0f));
	XMStoreFloat3(&constant->light, light);
	theta += theta_acc;

	if (theta < 0.0f || XM_PI < theta)
		theta_acc *= -1.0f;

	constant->light_color = XMFLOAT3(300.0f, 300.0f, 300.0f);

	constant->camera = XMFLOAT3(0.0f, 0.0f, -1.5f);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::Render() {
	auto command_allocator = GetCurrentCommandAllocator();

	THROW_IF_FAILED(
		command_allocator->Reset()
	);

	THROW_IF_FAILED(
		command_list_->Reset(command_allocator, pso_.Get())
	);

	auto back_buffer = GetCurrentBackBuffer();

	command_list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer,
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	constexpr FLOAT kClearColor[4] = { 0.2f, 0.2f, 0.2f, 1.0 };

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentRTV();
	command_list_->ClearRenderTargetView(rtv, kClearColor, 0, nullptr);
	command_list_->ClearDepthStencilView(dsv_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	command_list_->OMSetRenderTargets(1, &rtv, true, &dsv_);
	command_list_->RSSetViewports(1, &viewport_);
	command_list_->RSSetScissorRects(1, &scissor_rect_);
	command_list_->SetGraphicsRootSignature(root_signature_.Get());

	auto back_buffer_index = swap_chain_->GetCurrentBackBufferIndex();
	auto offset = AlignPow2(sizeof(Constant), 256) * back_buffer_index;
	command_list_->SetGraphicsRootConstantBufferView(0,
		constant_buffer_->GetGPUVirtualAddress() + offset);

	ID3D12DescriptorHeap* heaps[] = { srv_heap_.Get() };
	command_list_->SetDescriptorHeaps(_countof(heaps), heaps);
	command_list_->SetGraphicsRootDescriptorTable(1, srv_heap_->GetGPUDescriptorHandleForHeapStart());

	command_list_->IASetVertexBuffers(0, 1, &sphere_.vertex_buffer_view);
	command_list_->IASetIndexBuffer(&sphere_.index_buffer_view);
	command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list_->DrawIndexedInstanced(sphere_.draw_count, 1, 0, 0, 0);

	command_list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	
	THROW_IF_FAILED(
		command_list_->Close()
	);

	ID3D12CommandList* command_lists[] = { command_list_.Get() };
	command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

	THROW_IF_FAILED(
		swap_chain_->Present(0, 0)
	);

	fence_values_[back_buffer_index] = ++frame_count_;

	THROW_IF_FAILED(
		command_queue_->Signal(fence_.Get(), frame_count_)
	);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitFactory() {
	UINT flags = 0;

#ifdef _DEBUG
	ComPtr<ID3D12Debug> debug;

	THROW_IF_FAILED(
		D3D12GetDebugInterface(IID_PPV_ARGS(&debug))
	);

	flags |= DXGI_CREATE_FACTORY_DEBUG;
	debug->EnableDebugLayer();
#endif // _DEBUG

	THROW_IF_FAILED(
		CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_))
	);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitDevice() {
	THROW_IF_FAILED(
		D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))
	);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitCommandQueue() {
	D3D12_COMMAND_QUEUE_DESC desc = {};

	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	THROW_IF_FAILED(
		device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue_))
	);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitFence() {
	for (auto i = 0; i != kNumBackBuffer; ++i) {
		fence_values_[i] = 0;
	}

	THROW_IF_FAILED(
		device_->CreateFence(frame_count_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))
	);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitSwapChain() {
	DXGI_SWAP_CHAIN_DESC1 desc = {};

	desc.Format = kBackBufferFormat;
	desc.Width = width_;
	desc.Height = height_;
	desc.SampleDesc = { 1, 0 };
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = kNumBackBuffer;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> swap_chain;

	THROW_IF_FAILED(
		factory_->CreateSwapChainForHwnd(command_queue_.Get(), window_, &desc, nullptr, nullptr, &swap_chain)
	);

	THROW_IF_FAILED(
		swap_chain.As(&swap_chain_)
	);

	for (auto i = 0; i != kNumBackBuffer; ++i) {
		THROW_IF_FAILED(
			swap_chain_->GetBuffer(i, IID_PPV_ARGS(&back_buffers_[i]))
		);
	}
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitRTVs() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};

	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NumDescriptors = kNumBackBuffer;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	THROW_IF_FAILED(
		device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtv_heap_))
	);

	rtv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor(rtv_heap_->GetCPUDescriptorHandleForHeapStart());
	for (auto i = 0; i != kNumBackBuffer; ++i) {
		device_->CreateRenderTargetView(back_buffers_[i].Get(), nullptr, descriptor);

		rtvs_[i] = descriptor;
		descriptor.Offset(rtv_size_);
	}
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitDepthTexture() {
	D3D12_CLEAR_VALUE clear_value;

	clear_value.Format = kDepthBufferFormat;
	clear_value.DepthStencil.Depth = 1.0f;

	THROW_IF_FAILED(
		device_->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(kDepthBufferFormat, width_, height_, 1, 0, 1, 0,
				D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL), D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value,
			IID_PPV_ARGS(&depth_texture_))
	); 
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitDSV() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};

	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	THROW_IF_FAILED(
		device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsv_heap_))
	);

	dsv_ = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
	device_->CreateDepthStencilView(depth_texture_.Get(), nullptr, dsv_);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitCommandAllocators() {
	for (auto i = 0; i != kNumBackBuffer; ++i) {
		auto& command_allocator = command_allocators_[i];

		THROW_IF_FAILED(
			device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator))
		);
	}
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitCommandList() {
	auto& command_allocator = command_allocators_[0];

	THROW_IF_FAILED(
		device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(), nullptr,
			IID_PPV_ARGS(&command_list_))
	);

	THROW_IF_FAILED(
		command_list_->Close()
	);
}

void Renderer::InitConstantBuffer() {
	THROW_IF_FAILED(
		CreateIntermediateBuffer(device_.Get(), AlignPow2(sizeof(Constant), 256) * kNumBackBuffer, &constant_buffer_)
	);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitSphereAndTextures() {
	auto mesh_data = GeometryGenerator().CreateSphere(1.0f, 32, 32);

	D3D12_COMMAND_QUEUE_DESC desc = {};

	desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ComPtr<ID3D12CommandQueue> command_queue;

	THROW_IF_FAILED(
		device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue))
	);

	ComPtr<ID3D12CommandAllocator> command_allocator;

	THROW_IF_FAILED(
		device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&command_allocator))
	);

	ComPtr<ID3D12GraphicsCommandList> command_list;

	THROW_IF_FAILED(
		device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, command_allocator.Get(), nullptr,
			IID_PPV_ARGS(&command_list))
	);

	std::vector<ComPtr<ID3D12Resource>> intermediates;

	UINT64 size;
	ComPtr<ID3D12Resource> src;
	ComPtr<ID3D12Resource> dst;
	D3D12_SUBRESOURCE_DATA subresource_data;

	// Create a vertex buffer.
	size = sizeof(GeometryGenerator::Vertex) * mesh_data.Vertices.size();
	assert(size);

	THROW_IF_FAILED(
		CreateIntermediateBuffer(device_.Get(), size, &src)
	);

	THROW_IF_FAILED(
		CreateStaticBuffer(device_.Get(), size, &dst);
	);

	subresource_data.pData = mesh_data.Vertices.data();
	subresource_data.RowPitch = size;
	subresource_data.SlicePitch = subresource_data.RowPitch;

	UpdateSubresources<1>(command_list.Get(), dst.Get(), src.Get(), 0, 0, 1, &subresource_data);

	sphere_.vertex_buffer = src;
	sphere_.vertex_buffer_view.BufferLocation = sphere_.vertex_buffer->GetGPUVirtualAddress();
	sphere_.vertex_buffer_view.SizeInBytes = size;
	sphere_.vertex_buffer_view.StrideInBytes = sizeof(GeometryGenerator::Vertex);
	intermediates.push_back(dst);

	// Create an index buffer.
	size = sizeof(GeometryGenerator::uint32) * mesh_data.Indices32.size();
	assert(size);

	THROW_IF_FAILED(
		CreateIntermediateBuffer(device_.Get(), size, &src)
	);

	THROW_IF_FAILED(
		CreateStaticBuffer(device_.Get(), size, &dst);
	);

	subresource_data.pData = mesh_data.Indices32.data();
	subresource_data.RowPitch = size;
	subresource_data.SlicePitch = subresource_data.RowPitch;

	UpdateSubresources<1>(command_list.Get(), dst.Get(), src.Get(), 0, 0, 1, &subresource_data);

	sphere_.index_buffer = src;
	sphere_.index_buffer_view.BufferLocation = sphere_.index_buffer->GetGPUVirtualAddress();
	sphere_.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
	sphere_.index_buffer_view.SizeInBytes = size;
	intermediates.push_back(dst);

	// Set a draw count.
	sphere_.draw_count = mesh_data.Indices32.size();

	int x, y, c;
	stbi_uc* image;

	image = stbi_load("../asset/albedo.png", &x, &y, &c, STBI_rgb_alpha);

	size = x * y * 4;
	assert(size);

	THROW_IF_FAILED(
		CreateIntermediateBuffer(device_.Get(), size, &src)
	);

	THROW_IF_FAILED(
		CreateTexture(device_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, x, y, &dst);
	);

	subresource_data.pData = image;
	subresource_data.RowPitch = x * 4;
	subresource_data.SlicePitch = subresource_data.RowPitch * y;

	UpdateSubresources<1>(command_list.Get(), dst.Get(), src.Get(), 0, 0, 1, &subresource_data);

	stbi_image_free(image);

	textures_.push_back(dst);
	intermediates.push_back(src);

	image = stbi_load("../asset/metallic.png", &x, &y, &c, STBI_rgb_alpha);

	size = x * y * 4;
	assert(size);

	THROW_IF_FAILED(
		CreateIntermediateBuffer(device_.Get(), size, &src)
	);

	THROW_IF_FAILED(
		CreateTexture(device_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, x, y, &dst);
	);

	subresource_data.pData = image;
	subresource_data.RowPitch = x * 4;
	subresource_data.SlicePitch = subresource_data.RowPitch * y;

	UpdateSubresources<1>(command_list.Get(), dst.Get(), src.Get(), 0, 0, 1, &subresource_data);

	stbi_image_free(image);

	textures_.push_back(dst);
	intermediates.push_back(src);

	image = stbi_load("../asset/roughness.png", &x, &y, &c, STBI_rgb_alpha);

	size = x * y * 4;
	assert(size);

	THROW_IF_FAILED(
		CreateIntermediateBuffer(device_.Get(), size, &src)
	);

	THROW_IF_FAILED(
		CreateTexture(device_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, x, y, &dst);
	);

	subresource_data.pData = image;
	subresource_data.RowPitch = x * 4;
	subresource_data.SlicePitch = subresource_data.RowPitch * y;

	UpdateSubresources<1>(command_list.Get(), dst.Get(), src.Get(), 0, 0, 1, &subresource_data);

	stbi_image_free(image);

	textures_.push_back(dst);
	intermediates.push_back(src);

	image = stbi_load("../asset/ao.png", &x, &y, &c, STBI_rgb_alpha);

	size = x * y * 4;
	assert(size);

	THROW_IF_FAILED(
		CreateIntermediateBuffer(device_.Get(), size, &src)
	);

	THROW_IF_FAILED(
		CreateTexture(device_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, x, y, &dst);
	);

	subresource_data.pData = image;
	subresource_data.RowPitch = x * 4;
	subresource_data.SlicePitch = subresource_data.RowPitch * y;

	UpdateSubresources<1>(command_list.Get(), dst.Get(), src.Get(), 0, 0, 1, &subresource_data);

	stbi_image_free(image);

	textures_.push_back(dst);
	intermediates.push_back(src);

	THROW_IF_FAILED(
		command_list->Close()
	);

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

	++frame_count_;

	THROW_IF_FAILED(
		command_queue->Signal(fence_.Get(), frame_count_)
	);

	WaitCommandQueueIdle(command_queue.Get());
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitSRVs() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};

	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 4;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	THROW_IF_FAILED(
		device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srv_heap_))
	);

	srv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE srv(srv_heap_->GetCPUDescriptorHandleForHeapStart());
	for (auto texture : textures_) {
		device_->CreateShaderResourceView(texture.Get(), nullptr, srv);

		srv.Offset(srv_size_);
	}

	ComPtr<ID3D12CommandAllocator> command_allocator;

	THROW_IF_FAILED(
		device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator))
	);

	command_list_->Reset(command_allocator.Get(), nullptr);

	std::vector<D3D12_RESOURCE_BARRIER> barriers;

	for (auto texture : textures_) {
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	command_list_->ResourceBarrier(barriers.size(), barriers.data());

	THROW_IF_FAILED(
		command_list_->Close()
	);

	ID3D12CommandList* command_lists[] = { command_list_.Get() };
	command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

	++frame_count_;

	THROW_IF_FAILED(
		command_queue_->Signal(fence_.Get(), frame_count_)
	);

	WaitCommandQueueIdle();
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::InitPipeline() {
	D3D12_INPUT_ELEMENT_DESC input_elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	CD3DX12_DESCRIPTOR_RANGE srvs;
	srvs.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

	CD3DX12_ROOT_PARAMETER root_parameters[2];

	root_parameters[0].InitAsConstantBufferView(0);
	root_parameters[1].InitAsDescriptorTable(1, &srvs);

	CD3DX12_STATIC_SAMPLER_DESC sampler_desc(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc(_countof(root_parameters), root_parameters, 1, &sampler_desc,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serialized_root_signature;
	ComPtr<ID3DBlob> error;
	auto result = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1,
		&serialized_root_signature, nullptr);

	if (error) {
		spdlog::error("{}", static_cast<char*>(error->GetBufferPointer()));
	}

	assert(SUCCEEDED(result));

	THROW_IF_FAILED(
		device_->CreateRootSignature(0, serialized_root_signature->GetBufferPointer(),
			serialized_root_signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_));
	);

	ComPtr<ID3DBlob> vs_code;
	CompileShader(L"../src/pbr.hlsl", "vs_main", "vs_5_0", &vs_code);

	ComPtr<ID3DBlob> ps_code;
	CompileShader(L"../src/pbr.hlsl", "ps_main", "ps_5_0", &ps_code);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

	desc.InputLayout = { input_elements, _countof(input_elements) };
	desc.pRootSignature = root_signature_.Get();
	desc.VS = { vs_code->GetBufferPointer(), vs_code->GetBufferSize() };
	desc.PS = { ps_code->GetBufferPointer(), ps_code->GetBufferSize() };
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = kBackBufferFormat;
	desc.DSVFormat = kDepthBufferFormat;
	desc.SampleDesc = { 1, 0 };

	THROW_IF_FAILED(
		device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso_))
	);
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::WaitCommandQueueIdle() {
	WaitCommandQueueIdle(command_queue_.Get());
}

//----------------------------------------------------------------------------------------------------------------------

void Renderer::WaitCommandQueueIdle(ID3D12CommandQueue* command_queue) {
	if (fence_->GetCompletedValue() < frame_count_) {
		HANDLE event = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		THROW_IF_FAILED(
			fence_->SetEventOnCompletion(frame_count_, event)
		);

		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
}

//----------------------------------------------------------------------------------------------------------------------

UINT64 Renderer::GetCurrentFenceValue() {
	return fence_values_[swap_chain_->GetCurrentBackBufferIndex()];
}

//----------------------------------------------------------------------------------------------------------------------

ID3D12Resource* Renderer::GetCurrentBackBuffer() {
	return back_buffers_[swap_chain_->GetCurrentBackBufferIndex()].Get();
}

//----------------------------------------------------------------------------------------------------------------------

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::GetCurrentRTV() {
	return rtvs_[swap_chain_->GetCurrentBackBufferIndex()];
}

//----------------------------------------------------------------------------------------------------------------------

ID3D12CommandAllocator* Renderer::GetCurrentCommandAllocator() {
	return command_allocators_[swap_chain_->GetCurrentBackBufferIndex()].Get();
}

//----------------------------------------------------------------------------------------------------------------------