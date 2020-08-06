//
// This file is part of the "dxview" project
// See "LICENSE" for license information.
//

#ifndef DXVIEW_DX_HELPER_H_
#define DXVIEW_DX_HELPER_H_

#include <cassert>
#include <stdexcept>
#include <filesystem>
#include <winrt/base.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <spdlog/spdlog.h>

//----------------------------------------------------------------------------------------------------------------------

inline auto ToString(HRESULT result) {
	switch (result)
	{
	case E_INVALIDARG:
		return "E_INVALIDARG";
	case DXGI_ERROR_INVALID_CALL:
		return "DXGI_ERROR_INVALID_CALL";
	default:
		throw std::runtime_error("Fail");
	}
}

//----------------------------------------------------------------------------------------------------------------------

#define THROW_IF_FAILED(function) { \
	auto result = function; \
	\
	if (FAILED(result)) { \
		spdlog::error(fmt::format("{} {} {} {}", ToString(result), #function, __FILE__, __LINE__)); \
		throw winrt::hresult_error(result);\
	} \
}

//----------------------------------------------------------------------------------------------------------------------

inline HRESULT CreateIntermediateBuffer(ID3D12Device* device, UINT64 size, ID3D12Resource** buffer) {
	return device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(buffer));
}

//----------------------------------------------------------------------------------------------------------------------

inline HRESULT CreateStaticBuffer(ID3D12Device* device, UINT64 size, ID3D12Resource** buffer) {
	return device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size), D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(buffer));
}

//----------------------------------------------------------------------------------------------------------------------

inline HRESULT CreateTexture(ID3D12Device* device, DXGI_FORMAT format, UINT64 width, UINT height,
	ID3D12Resource** texture) {
	return device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1), D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(texture));
}

//----------------------------------------------------------------------------------------------------------------------

inline void CompileShader(LPCWSTR file_path, LPCSTR entrypoint, LPCSTR target, ID3DBlob** code) {
	auto flags = 0u;

#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // _DEBUG

	ComPtr<ID3DBlob> error;
	auto result = D3DCompileFromFile(file_path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint, target,
		flags, 0, code, &error);

	if (error) {
		spdlog::error("{}", static_cast<char*>(error->GetBufferPointer()));
	}

	assert(SUCCEEDED(result));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename T>
inline T AlignPow2(T value, UINT64 alignment) {
	return ((value + static_cast<T>(alignment) - 1) & ~(static_cast<T>(alignment) - 1));
}

#endif // DXVIEW_DX_HELPER_H_