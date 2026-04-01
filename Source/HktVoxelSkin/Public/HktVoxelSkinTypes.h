// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ============================================================================
// 복셀 스킨 레이어 정의
//
// 캐릭터 복셀 메시는 7개 레이어로 구성된다.
// 각 레이어는 독립적으로 교체 가능하여 모듈러 외형을 지원한다.
// ============================================================================

namespace EHktVoxelSkinLayer
{
	enum Type : uint8
	{
		Body    = 0,   // 기본 몸체
		Head    = 1,   // 머리
		Armor   = 2,   // 갑옷/상의
		Boots   = 3,   // 부츠/하의
		Gloves  = 4,   // 장갑
		Cape    = 5,   // 망토/날개
		Weapon  = 6,   // 무기

		Count   = 7,
	};

	inline const TCHAR* ToString(Type Layer)
	{
		static const TCHAR* Names[] = {
			TEXT("Body"), TEXT("Head"), TEXT("Armor"), TEXT("Boots"),
			TEXT("Gloves"), TEXT("Cape"), TEXT("Weapon"),
		};
		return (Layer < Count) ? Names[Layer] : TEXT("Unknown");
	}
}

// ============================================================================
// FHktVoxelSkinID — 스킨 식별자
// ============================================================================

struct FHktVoxelSkinID
{
	uint16 SkinSetID = 0;       // 스킨 세트 (기본, 화염, 얼음 등)
	uint8  PaletteRow = 0;      // 팔레트 텍스처 행 번호 (0~255)
	uint8  Grade = 0;           // 스킨 등급 (파편 수에 영향)

	bool operator==(const FHktVoxelSkinID& Other) const
	{
		return SkinSetID == Other.SkinSetID && PaletteRow == Other.PaletteRow;
	}
};

// ============================================================================
// FHktVoxelSkinLayerData — 개별 레이어의 복셀 데이터 참조
// ============================================================================

struct FHktVoxelSkinLayerData
{
	EHktVoxelSkinLayer::Type Layer = EHktVoxelSkinLayer::Body;
	FHktVoxelSkinID SkinID;
	FIntVector Offset = FIntVector::ZeroValue;  // 레이어 오프셋 (장착 위치)
	bool bVisible = true;
};
