#pragma once

#include "DirectXHelper.h"

#include "DescriptorHeap.h"

namespace Neuron::Graphics
{
  // Provides an interface for an application that owns Core to be notified of the device being lost or created.
  interface IDeviceNotify
  {
    virtual void OnDeviceLost() = 0;
    virtual void OnDeviceRestored() = 0;

    protected:
      ~IDeviceNotify() = default;
  };

  // Controls all the DirectX device resources.
  class Core
  {
  public:
    static constexpr unsigned int c_AllowTearing = 0x1;
    static constexpr unsigned int c_EnableHDR = 0x2;

    static void Startup(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT, UINT backBufferCount = 2,
      D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0, unsigned int flags = 0) noexcept(false);
    static void Shutdown() { WaitForGpu(); }

    static void CreateDeviceResources();
    static void CreateWindowSizeDependentResources();
    static void SetWindow(HWND window, int width, int height);
    static bool WindowSizeChanged(int width, int height);
    static void HandleDeviceLost();
    static void RegisterDeviceNotify(IDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }
    static void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_RENDER_TARGET);
    static void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);
    static void WaitForGpu() noexcept;

    // Device Accessors.
    static RECT GetOutputSize() noexcept { return m_outputSize; }

    // Direct3D Accessors.
    static auto GetD3DDevice() noexcept { return m_d3dDevice.get(); }
    static auto GetSwapChain() noexcept { return m_swapChain.get(); }
    static auto GetDXGIFactory() noexcept { return m_dxgiFactory.get(); }
    static HWND GetWindow() noexcept { return m_window; }
    static D3D_FEATURE_LEVEL GetDeviceFeatureLevel() noexcept { return m_d3dFeatureLevel; }
    static ID3D12Resource* GetRenderTarget() noexcept { return m_renderTargets[m_backBufferIndex].get(); }
    static ID3D12Resource* GetDepthStencil() noexcept { return m_depthStencil.get(); }
    static ID3D12CommandQueue* GetCommandQueue() noexcept { return m_commandQueue.get(); }
    static ID3D12CommandAllocator* GetCommandAllocator() noexcept { return m_commandAllocators[m_backBufferIndex].get(); }
    static auto GetCommandList() noexcept { return m_commandList.get(); }
    static DXGI_FORMAT GetBackBufferFormat() noexcept { return m_backBufferFormat; }
    static DXGI_FORMAT GetDepthBufferFormat() noexcept { return m_depthBufferFormat; }
    static D3D12_VIEWPORT GetScreenViewport() noexcept { return m_screenViewport; }
    static D3D12_RECT GetScissorRect() noexcept { return m_scissorRect; }
    static UINT GetCurrentFrameIndex() noexcept { return m_backBufferIndex; }
    static UINT GetBackBufferCount() noexcept { return m_backBufferCount; }
    static DXGI_COLOR_SPACE_TYPE GetColorSpace() noexcept { return m_colorSpace; }
    static unsigned int GetDeviceOptions() noexcept { return m_options; }

    static CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() noexcept
    {
      return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(m_backBufferIndex), m_rtvDescriptorSize);
    }

    static CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() noexcept { return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart()); }

    inline static DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_TYPE_DSV
    };
    static D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(const D3D12_DESCRIPTOR_HEAP_TYPE _type, const UINT _count = 1) { return g_DescriptorAllocator[_type].Allocate(_count); }

    static void UpdateColorSpace();

  private:
    static void MoveToNextFrame();
    static com_ptr<IDXGIAdapter1> GetAdapter();

    static constexpr size_t MAX_BACK_BUFFER_COUNT = 3;

    inline static UINT m_backBufferIndex;

    // Direct3D objects.
    inline static com_ptr<ID3D12Device> m_d3dDevice;
    inline static com_ptr<ID3D12GraphicsCommandList> m_commandList;
    inline static com_ptr<ID3D12CommandQueue> m_commandQueue;
    inline static com_ptr<ID3D12CommandAllocator> m_commandAllocators[MAX_BACK_BUFFER_COUNT];

    // Swap chain objects.
    inline static com_ptr<IDXGIFactory6> m_dxgiFactory;
    inline static com_ptr<IDXGISwapChain3> m_swapChain;
    inline static com_ptr<ID3D12Resource> m_renderTargets[MAX_BACK_BUFFER_COUNT];
    inline static com_ptr<ID3D12Resource> m_depthStencil;

    // Presentation fence objects.
    inline static com_ptr<ID3D12Fence> m_fence;
    inline static UINT64 m_fenceValues[MAX_BACK_BUFFER_COUNT];
    inline static handle m_fenceEvent;

    // Direct3D rendering objects.
    inline static com_ptr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
    inline static com_ptr<ID3D12DescriptorHeap> m_dsvDescriptorHeap;
    inline static UINT m_rtvDescriptorSize;
    inline static D3D12_VIEWPORT m_screenViewport;
    inline static D3D12_RECT m_scissorRect;

    // Direct3D properties.
    inline static DXGI_FORMAT m_backBufferFormat;
    inline static DXGI_FORMAT m_depthBufferFormat;
    inline static UINT m_backBufferCount;
    inline static D3D_FEATURE_LEVEL m_d3dMinFeatureLevel;

    // Cached device properties.
    inline static HWND m_window;
    inline static D3D_FEATURE_LEVEL m_d3dFeatureLevel{ D3D_FEATURE_LEVEL_12_0 };
    inline static DWORD m_dxgiFactoryFlags;
    inline static RECT m_outputSize{ 0, 0, 1, 1 };

    // HDR Support
    inline static DXGI_COLOR_SPACE_TYPE m_colorSpace{ DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 };

    // Core options (see flags above)
    inline static unsigned int m_options;

    // The IDeviceNotify can be held directly as it owns the Core.
    inline static IDeviceNotify* m_deviceNotify;
  };
}
