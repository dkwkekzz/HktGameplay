// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * HktTerrainDestructibility - terrain voxel 파괴 정책
 *
 * TypeID별 파괴 가능 여부와 내구도(Health)를 정의한다.
 * pure C++ — UObject/UWorld 참조 없음.
 *
 * HktTerrainType 상수:
 *   Air=0, Grass=1, Dirt=2, Stone=3, Sand=4,
 *   Water=5, Snow=6, Ice=7, Gravel=8, Clay=9, Bedrock=10
 */
namespace HktTerrainDestructibility
{
    struct FPolicy
    {
        bool bDestructible;
        int32 Health;
    };

    static constexpr int32 MaxTypeId = 16;

    /**
     * TypeID별 파괴 정책 테이블.
     *
     * Air(0)     = 불가 (빈 공간)
     * Grass(1)   = HP 1
     * Dirt(2)    = HP 1
     * Stone(3)   = HP 3
     * Sand(4)    = HP 1
     * Water(5)   = 불가 (유체)
     * Snow(6)    = HP 1
     * Ice(7)     = HP 2
     * Gravel(8)  = HP 1
     * Clay(9)    = HP 1
     * Bedrock(10)= 불가 (기반암)
     */
    inline FPolicy GetPolicy(uint16 TypeId)
    {
        static const FPolicy Table[MaxTypeId] = {
            /* 0  Air     */ { false, 0 },
            /* 1  Grass   */ { true,  1 },
            /* 2  Dirt    */ { true,  1 },
            /* 3  Stone   */ { true,  3 },
            /* 4  Sand    */ { true,  1 },
            /* 5  Water   */ { false, 0 },
            /* 6  Snow    */ { true,  1 },
            /* 7  Ice     */ { true,  2 },
            /* 8  Gravel  */ { true,  1 },
            /* 9  Clay    */ { true,  1 },
            /* 10 Bedrock */ { false, 0 },
            /* 11~15 reserved */ { false, 0 }, { false, 0 }, { false, 0 }, { false, 0 }, { false, 0 },
        };

        if (TypeId < MaxTypeId)
            return Table[TypeId];
        return { false, 0 };
    }

    inline bool IsDestructible(uint16 TypeId)
    {
        return GetPolicy(TypeId).bDestructible;
    }

    inline int32 GetHealth(uint16 TypeId)
    {
        return GetPolicy(TypeId).Health;
    }
}
