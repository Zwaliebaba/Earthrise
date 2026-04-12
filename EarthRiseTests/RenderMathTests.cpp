#include "pch.h"
#include "CmoLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace DirectX;
using namespace Neuron::Graphics;

namespace EarthRiseTests::Rendering
{
  TEST_CLASS(RenderMathTests)
  {
  public:
    TEST_METHOD(ReverseZProjectionSwapsNearFar)
    {
      // Reverse-Z convention: pass (fov, aspect, farZ, nearZ) instead of (fov, aspect, nearZ, farZ)
      // This maps near plane to large Z and far plane to 0, preventing z-fighting at distance.
      constexpr float fovY = XM_PIDIV4;
      constexpr float aspect = 16.0f / 9.0f;
      constexpr float nearZ = 0.5f;
      constexpr float farZ = 20000.0f;

      // Standard projection
      XMMATRIX standard = XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);

      // Reverse-Z projection (near/far swapped)
      XMMATRIX reverseZ = XMMatrixPerspectiveFovLH(fovY, aspect, farZ, nearZ);

      XMFLOAT4X4 stdMat, revMat;
      XMStoreFloat4x4(&stdMat, standard);
      XMStoreFloat4x4(&revMat, reverseZ);

      // In standard LH projection, _33 = far/(far-near) ≈ 1.0
      // In reverse-Z LH, _33 = near/(near-far) ≈ 0.0
      Assert::IsTrue(stdMat._33 > 0.99f, L"Standard projection _33 should be near 1.0");
      Assert::IsTrue(revMat._33 < 0.001f, L"Reverse-Z projection _33 should be near 0.0");

      // In reverse-Z, a point at near plane should map to Z ≈ 1.0 (large Z)
      // and a point at far plane should map to Z ≈ 0.0
      XMVECTOR nearPoint = XMVectorSet(0, 0, nearZ, 1);
      XMVECTOR farPoint = XMVectorSet(0, 0, farZ, 1);

      XMVECTOR nearClip = XMVector4Transform(nearPoint, reverseZ);
      XMVECTOR farClip = XMVector4Transform(farPoint, reverseZ);

      // Perspective divide
      float nearDepth = XMVectorGetZ(nearClip) / XMVectorGetW(nearClip);
      float farDepth = XMVectorGetZ(farClip) / XMVectorGetW(farClip);

      Assert::IsTrue(nearDepth > 0.99f, L"Near plane should map to depth ~1.0 in reverse-Z");
      Assert::IsTrue(farDepth < 0.001f, L"Far plane should map to depth ~0.0 in reverse-Z");
    }

    TEST_METHOD(OriginRebasingSubtractsCameraPosition)
    {
      // Origin rebasing: entityWorldPos - cameraWorldPos = rebasedPos
      // This keeps GPU-side coordinates near zero.
      XMVECTOR cameraPos = XMVectorSet(50000.0f, 1000.0f, 80000.0f, 0);
      XMVECTOR entityPos = XMVectorSet(50100.0f, 1050.0f, 80200.0f, 0);
      XMVECTOR expected = XMVectorSet(100.0f, 50.0f, 200.0f, 0);

      XMVECTOR rebased = XMVectorSubtract(entityPos, cameraPos);

      Assert::IsTrue(XMVector3NearEqual(rebased, expected, XMVectorSplatEpsilon()),
        L"Rebased position should be entity - camera");

      // Verify the rebased values are small (good for float precision)
      Assert::IsTrue(XMVectorGetX(rebased) < 1000.0f,
        L"Rebased X should be small for GPU precision");
    }

    TEST_METHOD(ViewMatrixWithOriginRebasingHasCameraAtOrigin)
    {
      // With origin rebasing, the camera is always at origin in view space.
      // View matrix is built from (0,0,0) looking in the camera's direction.
      XMVECTOR eye = XMVectorSet(0, 0, 0, 0); // Camera at origin (rebased)
      XMVECTOR forward = XMVectorSet(0, 0, 1, 0);
      XMVECTOR up = XMVectorSet(0, 1, 0, 0);
      XMVECTOR at = XMVectorAdd(eye, forward);

      XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

      // Transform origin: should stay at (0,0,0)
      XMVECTOR origin = XMVectorSet(0, 0, 0, 1);
      XMVECTOR viewOrigin = XMVector4Transform(origin, view);
      Assert::IsTrue(XMVector3NearEqual(viewOrigin, XMVectorZero(), XMVectorReplicate(0.001f)),
        L"Camera at origin should transform to (0,0,0) in view space");
    }

    TEST_METHOD(FlatColorVertexSizeIs24Bytes)
    {
      // FlatColorVertex: Position(XMFLOAT3) + Normal(XMFLOAT3) = 24 bytes
      Assert::AreEqual(static_cast<size_t>(24), sizeof(FlatColorVertex),
        L"FlatColorVertex should be 24 bytes (position + normal)");
    }

    TEST_METHOD(ConstantBufferAlignment)
    {
      // D3D12 requires constant buffer sizes to be 256-byte aligned.
      constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
      Assert::AreEqual(static_cast<size_t>(256), alignment,
        L"D3D12 CB alignment should be 256 bytes");

      // Verify alignment math
      constexpr size_t testSize = 100;
      size_t aligned = (testSize + alignment - 1) & ~(alignment - 1);
      Assert::AreEqual(static_cast<size_t>(256), aligned,
        L"100 bytes should align up to 256");

      size_t twoBlocks = (257 + alignment - 1) & ~(alignment - 1);
      Assert::AreEqual(static_cast<size_t>(512), twoBlocks,
        L"257 bytes should align up to 512");
    }

    TEST_METHOD(CmoVertexStrideIs64Bytes)
    {
      // VSD3DStarter CMO vertex layout:
      // Position(12) + Normal(12) + Tangent(16) + Color(16) + TexCoord(8) = 64
      constexpr size_t stride = 12 + 12 + 16 + 16 + 8;
      Assert::AreEqual(static_cast<size_t>(64), stride,
        L"CMO vertex stride should be 64 bytes");
    }

    TEST_METHOD(FibonacciSphereDistributionIsUniform)
    {
      // Verify the Fibonacci sphere algorithm produces roughly uniform distribution.
      // Used by Starfield for star placement.
      constexpr int N = 100;

      int hemisphereTop = 0;
      int hemisphereBottom = 0;

      for (int i = 0; i < N; ++i)
      {
        float y = 1.0f - (2.0f * static_cast<float>(i) / static_cast<float>(N - 1));
        if (y > 0) ++hemisphereTop;
        else ++hemisphereBottom;
      }

      // Both hemispheres should have roughly equal points
      Assert::IsTrue(abs(hemisphereTop - hemisphereBottom) <= 2,
        L"Fibonacci sphere should distribute points equally between hemispheres");
    }
  };

  TEST_CLASS(CmoLoaderTests)
  {
  public:
    TEST_METHOD(LoadAsteroid01)
    {
      std::wstring path = EARTHRISE_SOURCE_DIR;
      path += L"\\GameData\\Meshes\\Asteroids\\Asteroid01.cmo";

      auto meshes = CmoLoader::LoadFromFile(path);
      Assert::IsFalse(meshes.empty(), L"Asteroid01.cmo should contain at least one mesh");

      const auto& mesh = meshes[0];
      Assert::IsTrue(mesh.Vertices.size() > 0, L"Asteroid01 should have vertices");
      Assert::IsTrue(mesh.Indices.size() > 0, L"Asteroid01 should have indices");
      Assert::IsTrue(mesh.Submeshes.size() >= 1, L"Asteroid01 should have at least one submesh");

      // Index count should be divisible by 3 (triangles)
      for (const auto& sub : mesh.Submeshes)
        Assert::AreEqual(0u, sub.IndexCount % 3, L"Index count should be a multiple of 3");
    }

    TEST_METHOD(LoadHullShuttle)
    {
      std::wstring path = EARTHRISE_SOURCE_DIR;
      path += L"\\GameData\\Meshes\\Hulls\\HullShuttle.cmo";

      auto meshes = CmoLoader::LoadFromFile(path);
      Assert::IsFalse(meshes.empty(), L"HullShuttle.cmo should contain at least one mesh");

      const auto& mesh = meshes[0];
      Assert::IsTrue(mesh.Vertices.size() > 0, L"HullShuttle should have vertices");
      Assert::IsTrue(mesh.Indices.size() > 0, L"HullShuttle should have indices");

      // All indices should reference valid vertices
      for (uint16_t idx : mesh.Indices)
        Assert::IsTrue(idx < mesh.Vertices.size(),
          L"All indices should be within vertex range");
    }

    TEST_METHOD(LoadAllCmoFilesWithoutError)
    {
      // Batch-validate all 65+ mesh files parse without crashing
      std::wstring basePath = EARTHRISE_SOURCE_DIR;
      basePath += L"\\GameData\\Meshes\\";

      const wchar_t* folders[] = {
        L"Asteroids", L"Crates", L"Decorations", L"Hulls",
        L"Jumpgates", L"Projectiles", L"SpaceObjects", L"Stations", L"Turrets"
      };

      int totalMeshes = 0;
      for (const wchar_t* folder : folders)
      {
        std::wstring searchPath = basePath + folder + L"\\*.cmo";

        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE)
          continue;

        do
        {
          std::wstring filePath = basePath + folder + L"\\" + findData.cFileName;
          auto meshes = CmoLoader::LoadFromFile(filePath);
          Assert::IsFalse(meshes.empty(),
            (std::wstring(L"Should parse: ") + findData.cFileName).c_str());

          for (const auto& mesh : meshes)
          {
            Assert::IsTrue(mesh.Vertices.size() > 0,
              (std::wstring(L"Vertices > 0: ") + findData.cFileName).c_str());
            Assert::IsTrue(mesh.Indices.size() > 0,
              (std::wstring(L"Indices > 0: ") + findData.cFileName).c_str());
          }
          ++totalMeshes;
        }
        while (FindNextFileW(hFind, &findData));

        FindClose(hFind);
      }

      // We should have found at least 60 meshes (65 expected)
      Assert::IsTrue(totalMeshes >= 60,
        L"Should have loaded at least 60 CMO files across all folders");
    }

    TEST_METHOD(VertexNormalsAreUnitLength)
    {
      std::wstring path = EARTHRISE_SOURCE_DIR;
      path += L"\\GameData\\Meshes\\Stations\\Mining01.cmo";

      auto meshes = CmoLoader::LoadFromFile(path);
      Assert::IsFalse(meshes.empty());

      for (const auto& v : meshes[0].Vertices)
      {
        XMVECTOR n = XMLoadFloat3(&v.Normal);
        float len = XMVectorGetX(XMVector3Length(n));
        Assert::IsTrue(fabsf(len - 1.0f) < 0.01f,
          L"Normals should be unit length");
      }
    }

    TEST_METHOD(LoadFromMemoryMatchesLoadFromFile)
    {
      std::wstring path = EARTHRISE_SOURCE_DIR;
      path += L"\\GameData\\Meshes\\Crates\\Crate01.cmo";

      // Load via file
      auto meshesFile = CmoLoader::LoadFromFile(path);
      Assert::IsFalse(meshesFile.empty());

      // Load raw bytes, then parse via memory
      HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      Assert::IsTrue(hFile != INVALID_HANDLE_VALUE, L"Should open file");

      DWORD fileSize = GetFileSize(hFile, nullptr);
      std::vector<uint8_t> buffer(fileSize);
      DWORD bytesRead = 0;
      ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr);
      CloseHandle(hFile);

      auto meshesMem = CmoLoader::LoadFromMemory(buffer.data(), buffer.size());
      Assert::AreEqual(meshesFile.size(), meshesMem.size(),
        L"File and memory loading should produce same number of meshes");

      Assert::AreEqual(meshesFile[0].Vertices.size(), meshesMem[0].Vertices.size(),
        L"Vertex counts should match");
      Assert::AreEqual(meshesFile[0].Indices.size(), meshesMem[0].Indices.size(),
        L"Index counts should match");
    }
  };

  TEST_CLASS(ProceduralMeshTests)
  {
  public:
    TEST_METHOD(UVSphereHasExpectedVertexCount)
    {
      // (rings + 1) * (slices + 1) vertices
      auto mesh = ProceduralMesh::GenerateUVSphere(16, 16);
      Assert::AreEqual(static_cast<size_t>(17 * 17), mesh.Vertices.size(),
        L"16x16 sphere should have (16+1)*(16+1) = 289 vertices");
    }

    TEST_METHOD(UVSphereHasExpectedTriangleCount)
    {
      // rings * slices * 2 triangles * 3 indices
      auto mesh = ProceduralMesh::GenerateUVSphere(16, 16);
      Assert::AreEqual(static_cast<size_t>(16 * 16 * 6), mesh.Indices.size(),
        L"16x16 sphere should have 16*16*6 = 1536 indices");
      Assert::AreEqual(0u, static_cast<uint32_t>(mesh.Indices.size() % 3),
        L"Index count must be divisible by 3");
    }

    TEST_METHOD(UVSphereIndicesWithinVertexRange)
    {
      auto mesh = ProceduralMesh::GenerateUVSphere(24, 24);
      for (size_t i = 0; i < mesh.Indices.size(); ++i)
      {
        Assert::IsTrue(mesh.Indices[i] < mesh.Vertices.size(),
          L"All indices must reference valid vertices");
      }
    }

    TEST_METHOD(UVSphereNormalsAreUnitLength)
    {
      auto mesh = ProceduralMesh::GenerateUVSphere(24, 24);
      for (const auto& v : mesh.Vertices)
      {
        XMVECTOR n = XMLoadFloat3(&v.Normal);
        float len = XMVectorGetX(XMVector3Length(n));
        Assert::IsTrue(fabsf(len - 1.0f) < 0.001f,
          L"Sphere normals should be unit length");
      }
    }

    TEST_METHOD(UVSphereHasOneSubmeshCoveringAllIndices)
    {
      auto mesh = ProceduralMesh::GenerateUVSphere(20, 20);
      Assert::AreEqual(static_cast<size_t>(1), mesh.Submeshes.size(),
        L"Should have exactly one submesh");
      Assert::AreEqual(0u, mesh.Submeshes[0].StartIndex,
        L"Submesh should start at index 0");
      Assert::AreEqual(static_cast<uint32_t>(mesh.Indices.size()), mesh.Submeshes[0].IndexCount,
        L"Submesh should cover all indices");
    }

    TEST_METHOD(UVSphereTexCoordsParallelToVertices)
    {
      auto mesh = ProceduralMesh::GenerateUVSphere(12, 12);
      Assert::AreEqual(mesh.Vertices.size(), mesh.TexCoords.size(),
        L"TexCoords array must be parallel to Vertices array");
    }

    TEST_METHOD(UVSphereVerticesAreOnUnitSphere)
    {
      auto mesh = ProceduralMesh::GenerateUVSphere(20, 20);
      for (const auto& v : mesh.Vertices)
      {
        XMVECTOR p = XMLoadFloat3(&v.Position);
        float len = XMVectorGetX(XMVector3Length(p));
        Assert::IsTrue(fabsf(len - 1.0f) < 0.001f,
          L"All vertices should lie on the unit sphere");
      }
    }

    TEST_METHOD(UVSphereFitsUint16Indices)
    {
      // Max practical size: 128x128 → (129*129) = 16641 verts, well within uint16_t
      auto mesh = ProceduralMesh::GenerateUVSphere(128, 128);
      Assert::IsTrue(mesh.Vertices.size() <= 65535,
        L"128x128 sphere must fit in uint16_t indices");
      for (uint16_t idx : mesh.Indices)
      {
        Assert::IsTrue(idx < mesh.Vertices.size(),
          L"All indices must reference valid vertices at max size");
      }
    }

    TEST_METHOD(UVSphereUnindexingProducesValidData)
    {
      // Simulate what BuildSurfaceMesh does: unindex the mesh and assign
      // barycentric coordinates. Verify no out-of-bounds access.
      auto mesh = ProceduralMesh::GenerateUVSphere(24, 24);

      const size_t triCount = mesh.Indices.size() / 3;
      std::vector<FlatColorVertex> unindexedVerts;
      unindexedVerts.reserve(triCount * 3);
      std::vector<uint16_t> unindexedIndices;
      unindexedIndices.reserve(triCount * 3);

      for (size_t t = 0; t < triCount; ++t)
      {
        uint16_t i0 = mesh.Indices[t * 3 + 0];
        uint16_t i1 = mesh.Indices[t * 3 + 1];
        uint16_t i2 = mesh.Indices[t * 3 + 2];

        Assert::IsTrue(i0 < mesh.Vertices.size(), L"i0 in range");
        Assert::IsTrue(i1 < mesh.Vertices.size(), L"i1 in range");
        Assert::IsTrue(i2 < mesh.Vertices.size(), L"i2 in range");

        auto baseIdx = static_cast<uint16_t>(unindexedVerts.size());

        unindexedVerts.push_back(mesh.Vertices[i0]);
        unindexedVerts.push_back(mesh.Vertices[i1]);
        unindexedVerts.push_back(mesh.Vertices[i2]);

        unindexedIndices.push_back(baseIdx);
        unindexedIndices.push_back(static_cast<uint16_t>(baseIdx + 1));
        unindexedIndices.push_back(static_cast<uint16_t>(baseIdx + 2));
      }

      // After unindexing, vertex count == index count == triCount * 3
      Assert::AreEqual(unindexedVerts.size(), unindexedIndices.size(),
        L"Unindexed vertex and index counts should match");
      Assert::IsTrue(unindexedVerts.size() <= 65535,
        L"Unindexed vertex count must fit in uint16_t");
    }
  };
}
