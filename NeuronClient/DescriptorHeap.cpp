#include "pch.h"
#include "DescriptorHeap.h"

using namespace Graphics;

void DescriptorAllocator::DestroyAll(void) { sm_DescriptorHeapPool.clear(); }

ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
  std::lock_guard<std::mutex> LockGuard(sm_AllocationMutex);

  D3D12_DESCRIPTOR_HEAP_DESC Desc;
  Desc.Type = Type;
  Desc.NumDescriptors = NUM_DESCRIPTORS_PER_HEAP;
  Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  Desc.NodeMask = 1;

  com_ptr<ID3D12DescriptorHeap> pHeap;
  check_hresult(Core::GetD3DDevice()->CreateDescriptorHeap(&Desc, IID_GRAPHICS_PPV_ARGS(pHeap)));
  sm_DescriptorHeapPool.emplace_back(pHeap);
  return pHeap.get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t Count)
{
  if (m_CurrentHeap == nullptr || m_RemainingFreeHandles < Count)
  {
    m_CurrentHeap = RequestNewHeap(m_Type);
    m_CurrentHandle = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
    m_RemainingFreeHandles = NUM_DESCRIPTORS_PER_HEAP;

    if (m_DescriptorSize == 0)
      m_DescriptorSize = Core::GetD3DDevice()->GetDescriptorHandleIncrementSize(m_Type);
  }

  D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
  m_CurrentHandle.ptr += Count * m_DescriptorSize;
  m_RemainingFreeHandles -= Count;
  return ret;
}

//
// DescriptorHeap implementation
//

void DescriptorHeap::Create(const std::wstring& Name, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount)
{
  m_HeapDesc.Type = Type;
  m_HeapDesc.NumDescriptors = MaxCount;
  m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  m_HeapDesc.NodeMask = 1;

  check_hresult(Core::GetD3DDevice()->CreateDescriptorHeap(&m_HeapDesc, IID_GRAPHICS_PPV_ARGS(m_Heap)));

#ifdef RELEASE
  (void)Name;
#else
  m_Heap->SetName(Name.c_str());
#endif

  m_DescriptorSize = Core::GetD3DDevice()->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
  m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;
  m_FirstHandle = DescriptorHandle(m_Heap->GetCPUDescriptorHandleForHeapStart(), m_Heap->GetGPUDescriptorHandleForHeapStart());
  m_NextFreeHandle = m_FirstHandle;
}

DescriptorHandle DescriptorHeap::Alloc(uint32_t Count)
{
  DEBUG_ASSERT_TEXT(HasAvailableSpace(Count), "Descriptor Heap out of space.  Increase heap size.");
  DescriptorHandle ret = m_NextFreeHandle;
  m_NextFreeHandle += Count * m_DescriptorSize;
  m_NumFreeDescriptors -= Count;
  return ret;
}

bool DescriptorHeap::ValidateHandle(const DescriptorHandle& DHandle) const
{
  if (DHandle.GetCpuPtr() < m_FirstHandle.GetCpuPtr() || DHandle.GetCpuPtr() >= m_FirstHandle.GetCpuPtr() + m_HeapDesc.NumDescriptors * m_DescriptorSize)
    return false;

  if (DHandle.GetGpuPtr() - m_FirstHandle.GetGpuPtr() != DHandle.GetCpuPtr() - m_FirstHandle.GetCpuPtr())
    return false;

  return true;
}
