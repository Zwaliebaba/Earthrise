#include "pch.h"
#include "SpaceObjectRenderer.h"

using namespace Neuron::Graphics;

void SpaceObjectRenderer::Initialize()
{
  m_pipeline.Initialize();
}

void SpaceObjectRenderer::BeginFrame(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc, const Camera& camera)
{
  m_pipeline.BindPipeline(cmdList);

  FrameConstants fc{};
  XMStoreFloat4x4(&fc.ViewProjection, XMMatrixTranspose(camera.GetViewProjectionMatrix()));
  XMStoreFloat3(&fc.CameraPosition, camera.GetPosition());
  XMStoreFloat3(&fc.LightDirection, camera.GetLightDirection());
  fc.AmbientIntensity = 0.3f;

  m_pipeline.SetFrameConstants(cmdList, cbAlloc, fc);
}

void XM_CALLCONV SpaceObjectRenderer::RenderObject(ID3D12GraphicsCommandList* cmdList,
  ConstantBufferAllocator& cbAlloc,
  const Mesh* mesh, FXMMATRIX world, FXMVECTOR color)
{
  if (!mesh || !mesh->IsValid())
    return;

  DrawConstants dc{};
  XMStoreFloat4x4(&dc.World, XMMatrixTranspose(world));
  XMStoreFloat4(&dc.Color, color);

  m_pipeline.SetDrawConstants(cmdList, cbAlloc, dc);

  auto vbView = mesh->GetVertexBufferView();
  auto ibView = mesh->GetIndexBufferView();
  cmdList->IASetVertexBuffers(0, 1, &vbView);
  cmdList->IASetIndexBuffer(&ibView);

  for (const auto& submesh : mesh->GetSubmeshes())
  {
    cmdList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndex, 0, 0);
  }
}
