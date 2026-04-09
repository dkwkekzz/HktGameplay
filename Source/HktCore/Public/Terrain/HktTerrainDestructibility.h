// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Terrain/HktTerrainVoxelDef.h"

/**
 * HktTerrainDestructibility - terrain voxel 파괴 정책
 *
 * 이전에는 독립 테이블을 보유했으나, HktTerrainVoxelDef가
 * 단일 진실 소스로 통합된 후부터 해당 정의에 위임한다.
 *
 * 기존 호출부는 변경 없이 계속 사용 가능.
 *
 * pure C++ — UObject/UWorld 참조 없음.
 */
namespace HktTerrainDestructibility
{
    struct FPolicy
    {
        bool bDestructible;
        int32 Health;
    };

    /** TypeID별 파괴 정책 조회 — HktTerrainVoxelDef::GetDef() 에 위임 */
    inline FPolicy GetPolicy(uint16 TypeId)
    {
        const FHktVoxelDef& Def = HktTerrainVoxelDef::GetDef(TypeId);
        return { Def.bDestructible, Def.Health };
    }

    inline bool IsDestructible(uint16 TypeId)
    {
        return HktTerrainVoxelDef::GetDef(TypeId).bDestructible;
    }

    inline int32 GetHealth(uint16 TypeId)
    {
        return HktTerrainVoxelDef::GetDef(TypeId).Health;
    }
}
