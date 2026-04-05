// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Rendering/HktVoxelChunkComponent.h"
#include "Rendering/HktVoxelChunkProxy.h"
#include "Data/HktVoxelRenderCache.h"
#include "Data/HktVoxelTypes.h"
#include "Meshing/HktVoxelVertex.h"
#include "HktVoxelCoreLog.h"
#include "Materials/Material.h"

UHktVoxelChunkComponent::UHktVoxelChunkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CastShadow = true;

	// 선택용 Visibility 트레이스 응답 — 물리 충돌은 VM이 처리
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 기본 머티리얼 — 프로덕션에서는 팔레트 기반 커스텀 머티리얼로 교체
	SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
}

void UHktVoxelChunkComponent::SetVoxelMaterial(UMaterialInterface* InMaterial)
{
	if (InMaterial)
	{
		SetMaterial(0, InMaterial);
		MarkRenderStateDirty();
	}
}

void UHktVoxelChunkComponent::Initialize(FHktVoxelRenderCache* Cache, const FIntVector& InChunkCoord)
{
	RenderCache = Cache;
	ChunkCoord = InChunkCoord;

	// 청크 좌표에 따른 상대 위치 설정
	// 엔티티 복셀(ChunkCoord=0,0,0)은 원점이므로 Actor에 붙어서 이동
	// 월드 복셀은 청크 좌표에 맞는 오프셋 적용
	static constexpr float VoxelSize = FHktVoxelChunk::VOXEL_SIZE;
	static constexpr float ChunkWorldSize = FHktVoxelChunk::SIZE * VoxelSize;

	SetRelativeLocation(FVector(
		ChunkCoord.X * ChunkWorldSize,
		ChunkCoord.Y * ChunkWorldSize,
		ChunkCoord.Z * ChunkWorldSize));
}

void UHktVoxelChunkComponent::OnMeshReady()
{
	if (!RenderCache)
	{
		return;
	}

	const FHktVoxelChunk* Chunk = RenderCache->GetChunk(ChunkCoord);
	if (!Chunk)
	{
		return;
	}

	// 불투명 + 반투명 메시 데이터를 합쳐서 복사
	// (프로덕션에서는 별도 렌더 패스로 분리하지만, 현재는 단일 패스)
	TArray<FHktVoxelVertex> VerticesCopy;
	TArray<uint32> IndicesCopy;

	VerticesCopy.Append(Chunk->OpaqueVertices);
	IndicesCopy.Append(Chunk->OpaqueIndices);

	// 반투명 인덱스는 오프셋 적용
	const uint32 OpaqueVertCount = Chunk->OpaqueVertices.Num();
	for (uint32 Idx : Chunk->TranslucentIndices)
	{
		IndicesCopy.Add(Idx + OpaqueVertCount);
	}
	VerticesCopy.Append(Chunk->TranslucentVertices);

	if (!SceneProxy)
	{
		MarkRenderStateDirty();
		return;
	}

	// SceneProxy를 안전하게 캡처 — 렌더 커맨드 실행 시 유효성 확인용으로
	// GetScene()에서 FPrimitiveComponentId를 통해 안전하게 접근
	FPrimitiveSceneProxy* CapturedProxy = SceneProxy;

	// GPU 버퍼 업로드는 반드시 Render Thread에서
	ENQUEUE_RENDER_COMMAND(HktVoxelUpdateMesh)(
		[CapturedProxy, Verts = MoveTemp(VerticesCopy), Idxs = MoveTemp(IndicesCopy)](FRHICommandListImmediate& RHICmdList)
		{
			static_cast<FHktVoxelChunkProxy*>(CapturedProxy)->UpdateMeshData_RenderThread(Verts, Idxs);
		}
	);
}

FPrimitiveSceneProxy* UHktVoxelChunkComponent::CreateSceneProxy()
{
	return new FHktVoxelChunkProxy(this);
}

FBoxSphereBounds UHktVoxelChunkComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	static constexpr float VoxelSize = FHktVoxelChunk::VOXEL_SIZE;
	static constexpr float ChunkWorldSize = FHktVoxelChunk::SIZE * VoxelSize;

	// 복셀은 로컬 (0,0,0)~(ChunkWorldSize,ChunkWorldSize,ChunkWorldSize) 범위에 배치됨
	const FBox Box(FVector::ZeroVector, FVector(ChunkWorldSize, ChunkWorldSize, ChunkWorldSize));
	return FBoxSphereBounds(Box).TransformBy(LocalToWorld);
}
