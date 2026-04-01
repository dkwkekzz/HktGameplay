// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVoxelSkinAssembler.h"
#include "Data/HktVoxelTypes.h"
#include "HktVoxelSkinLog.h"

void FHktVoxelSkinAssembler::SetLayer(EHktVoxelSkinLayer::Type Layer, const FHktVoxelSkinLayerData& Data)
{
	if (Layer < EHktVoxelSkinLayer::Count)
	{
		Layers[Layer] = Data;
		bLayerActive[Layer] = true;
	}
}

void FHktVoxelSkinAssembler::RemoveLayer(EHktVoxelSkinLayer::Type Layer)
{
	if (Layer < EHktVoxelSkinLayer::Count)
	{
		bLayerActive[Layer] = false;
	}
}

void FHktVoxelSkinAssembler::SetLayerVisible(EHktVoxelSkinLayer::Type Layer, bool bVisible)
{
	if (Layer < EHktVoxelSkinLayer::Count && bLayerActive[Layer])
	{
		Layers[Layer].bVisible = bVisible;
	}
}

const FHktVoxelSkinLayerData* FHktVoxelSkinAssembler::GetLayer(EHktVoxelSkinLayer::Type Layer) const
{
	if (Layer < EHktVoxelSkinLayer::Count && bLayerActive[Layer])
	{
		return &Layers[Layer];
	}
	return nullptr;
}

void FHktVoxelSkinAssembler::Assemble(FHktVoxelChunk& OutChunk) const
{
	// 청크 초기화 — 모든 복셀을 빈 공간으로
	FMemory::Memzero(OutChunk.Data, sizeof(OutChunk.Data));
	OutChunk.bMeshDirty = true;
	OutChunk.bMeshReady = false;

	// 레이어를 낮은 우선순위부터 순회 — 높은 레이어가 덮어쓴다
	for (int32 i = 0; i < EHktVoxelSkinLayer::Count; i++)
	{
		if (!bLayerActive[i] || !Layers[i].bVisible)
		{
			continue;
		}

		// TODO: Phase 2 — 레이어별 복셀 데이터를 로드하여 OutChunk에 합성
		// 현재는 레이어 활성화 플래그만 관리
		UE_LOG(LogHktVoxelSkin, Verbose, TEXT("Assembling layer: %s (SkinSet=%d, Palette=%d)"),
			EHktVoxelSkinLayer::ToString(static_cast<EHktVoxelSkinLayer::Type>(i)),
			Layers[i].SkinID.SkinSetID,
			Layers[i].SkinID.PaletteRow);
	}
}

void FHktVoxelSkinAssembler::ChangeSkinPalette(EHktVoxelSkinLayer::Type Layer, uint8 NewPaletteRow)
{
	if (Layer < EHktVoxelSkinLayer::Count && bLayerActive[Layer])
	{
		Layers[Layer].SkinID.PaletteRow = NewPaletteRow;
		// 팔레트 변경은 재메싱 불필요 — GPU에서 팔레트 텍스처 룩업으로 처리
	}
}

void FHktVoxelSkinAssembler::Clear()
{
	FMemory::Memzero(bLayerActive, sizeof(bLayerActive));
}
