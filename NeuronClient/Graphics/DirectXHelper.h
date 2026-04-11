#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "d3dx12.h"

#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "dxguid.lib")

#define IID_GRAPHICS_PPV_ARGS(ppType)       __uuidof(ppType), (ppType).put_void()

#define D3D12_CPU_VIRTUAL_ADDRESS_NULL      ((size_t)0)
#define D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN   ((size_t)-1)

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif

consteval uint32_t FIXED_MSG(const char* _msg)
{
  return static_cast<uint32_t>(_msg[0]) << 24 | static_cast<uint32_t>(_msg[1]) << 16 | static_cast<uint32_t>(_msg[2]) << 8 |
    static_cast<uint32_t>(_msg[3]);
}

#if defined _DEBUG
inline void SetName(ID3D12Object* _pObject, const wchar_t* _name)
{
  _pObject->SetName(_name);
}
#else
inline void SetName([[maybe_unused]] ID3D12Object* _pObject, [[maybe_unused]] const wchar_t* _name) {}
#endif
