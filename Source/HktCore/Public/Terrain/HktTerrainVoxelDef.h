// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Terrain/HktTerrainVoxel.h"

/**
 * EHktVoxelPhase - 복셀의 물리적 상태
 *
 * Movement / Collision system이 이 값을 보고
 * 콜리전 차단 / 수영 / 통과 여부를 결정한다.
 */
enum class EHktVoxelPhase : uint8
{
    Gas   = 0,  // Air       — 무저항 통과 (콜리전 없음)
    Fluid = 1,  // Water     — 유체 통과 (수영, 저항)
    Solid = 2,  // Stone 등  — 이동 완전 차단
};

/**
 * EHktVoxelMoveModifier - 이 복셀 위/안에 있을 때 이동 보정
 *
 * HktSimulationSystems(C++)의 Movement system이 매 틱 발판 복셀을 조회하여
 * 엔티티의 속도 / 마찰을 조정한다.
 *
 * 실제 속도 계산 예:
 *   최종속도 = (MoveSpeedPct / 100) * 기본속도
 *   Slippery: 속도 110% 지만 가속/감속 반응이 느려짐
 */
enum class EHktVoxelMoveModifier : uint8
{
    Normal   = 0,  // 변화 없음   (100%)
    Slippery = 1,  // Ice         — 마찰 감소, 고속 슬라이딩 (110% + 제어 감소)
    Slow     = 2,  // Snow/Clay   — 이동 감속 (80%)
    Swim     = 3,  // Water       — 수영 모드 (50%, 상하 이동 허용)
};

/**
 * EHktVoxelDestroyEffect - 복셀 파괴 시 연출 유형
 *
 * Op_FindTerrainInRadius 가 voxel을 제거한 뒤
 * 이 값에 맞는 lifecycle 이벤트 태그를 디스패치한다.
 *
 * 이벤트 태그 매핑 (HktVMInterpreterActions.cpp 참조):
 *   None    → (없음)
 *   Debris  → Story.Flow.Debris.Lifecycle
 *   Shatter → Story.Flow.Voxel.Shatter
 *   Crumble → Story.Flow.Voxel.Crumble
 *   Crack   → Story.Flow.Voxel.Crack
 */
enum class EHktVoxelDestroyEffect : uint8
{
    None    = 0,  // 효과 없음         (Air, Water, Bedrock)
    Debris  = 1,  // 일반 잔해 엔티티   (Stone, Grass, Dirt …)
    Shatter = 2,  // 산산조각           (Glass — 작은 파편 다수)
    Crumble = 3,  // 부스러짐           (Sand, Gravel — 중력 낙하)
    Crack   = 4,  // 균열 / 얼음 덩어리  (Ice)
};

/**
 * FHktVoxelDef - 복셀 타입의 모든 상호작용 속성
 *
 * HktCore 내 복셀 처리의 단일 진실 소스(Single Source of Truth).
 *
 * 각 시스템이 이 구조체를 참조한다:
 *   TerrainState::SetVoxel       — AutoFlags 자동 설정
 *   VM::FindTerrainInRadius      — bDestructible 체크 + DestroyEffect 기반 이벤트 디스패치
 *   HktTerrainDestructibility    — bDestructible / Health 위임 (하위 호환)
 *   HktSimulationSystems         — Phase / MoveModifier / bGravity / bFlowable 참조
 *
 * 순수 C++ — UObject/UWorld 참조 없음.
 */
struct FHktVoxelDef
{
    EHktVoxelPhase          Phase;          // 물리 상태 (Gas/Fluid/Solid)
    EHktVoxelMoveModifier   MoveModifier;   // 이동 보정 (Normal/Slippery/Slow/Swim)
    EHktVoxelDestroyEffect  DestroyEffect;  // 파괴 연출 (None/Debris/Shatter/Crumble/Crack)
    bool                    bDestructible;  // 파괴 가능 여부
    int32                   Health;         // 체력 (파괴 가능할 때만 의미)
    bool                    bGravity;       // 지지 없을 때 낙하 (Sand, Gravel)
    bool                    bFlowable;      // 인접 빈 공간으로 전파 (Water)
    int32                   MoveSpeedPct;   // 위/안에 있을 때 이동 속도 % (100 = 기본)
    uint8                   AutoFlags;      // FHktTerrainVoxel::Flags 자동 할당값
};

/**
 * HktTerrainVoxelDef — TypeID별 FHktVoxelDef 조회 네임스페이스
 *
 * 새 타입 추가 시 GetDef() 테이블만 수정하면 된다.
 * HktTerrainType 상수 (HktVoxelTerrainTypes.h):
 *   Air=0, Grass=1, Dirt=2, Stone=3, Sand=4, Water=5,
 *   Snow=6, Ice=7, Gravel=8, Clay=9, Bedrock=10, Glass=11
 */
namespace HktTerrainVoxelDef
{
    static constexpr int32 MaxTypeId = 16;

    /**
     * TypeID별 복셀 정의 조회.
     *
     * 범위 밖 TypeId는 Solid/Normal/None/불가파괴 기본값 반환.
     */
    inline const FHktVoxelDef& GetDef(uint16 TypeId)
    {
        // 편의 별칭
        using F = FHktTerrainVoxel;
        using P = EHktVoxelPhase;
        using M = EHktVoxelMoveModifier;
        using D = EHktVoxelDestroyEffect;

        // Phase / MoveModifier / DestroyEffect / Destructible / HP
        // / Gravity / Flowable / MoveSpeedPct / AutoFlags
        static const FHktVoxelDef Table[MaxTypeId] = {
            /* 0  Air     */ {P::Gas,   M::Normal,   D::None,    false, 0, false, false, 100, 0},
            /* 1  Grass   */ {P::Solid, M::Normal,   D::Debris,  true,  1, false, false, 100, F::FLAG_DESTRUCTIBLE},
            /* 2  Dirt    */ {P::Solid, M::Normal,   D::Debris,  true,  1, false, false, 100, F::FLAG_DESTRUCTIBLE},
            /* 3  Stone   */ {P::Solid, M::Normal,   D::Debris,  true,  3, false, false, 100, F::FLAG_DESTRUCTIBLE},
            /* 4  Sand    */ {P::Solid, M::Slow,     D::Crumble, true,  1, true,  false,  90, F::FLAG_DESTRUCTIBLE},
            /* 5  Water   */ {P::Fluid, M::Swim,     D::None,    false, 0, false, true,   50, F::FLAG_TRANSLUCENT},
            /* 6  Snow    */ {P::Solid, M::Slow,     D::Debris,  true,  1, false, false,  80, F::FLAG_DESTRUCTIBLE},
            /* 7  Ice     */ {P::Solid, M::Slippery, D::Crack,   true,  2, false, false, 110, F::FLAG_TRANSLUCENT | F::FLAG_DESTRUCTIBLE},
            /* 8  Gravel  */ {P::Solid, M::Normal,   D::Crumble, true,  1, true,  false,  95, F::FLAG_DESTRUCTIBLE},
            /* 9  Clay    */ {P::Solid, M::Slow,     D::Debris,  true,  1, false, false,  80, F::FLAG_DESTRUCTIBLE},
            /* 10 Bedrock */ {P::Solid, M::Normal,   D::None,    false, 0, false, false, 100, 0},
            /* 11 Glass   */ {P::Solid, M::Normal,   D::Shatter, true,  1, false, false, 100, F::FLAG_TRANSLUCENT | F::FLAG_DESTRUCTIBLE},
            /* 12 reserved */ {P::Solid, M::Normal,  D::None,    false, 0, false, false, 100, 0},
            /* 13 reserved */ {P::Solid, M::Normal,  D::None,    false, 0, false, false, 100, 0},
            /* 14 reserved */ {P::Solid, M::Normal,  D::None,    false, 0, false, false, 100, 0},
            /* 15 reserved */ {P::Solid, M::Normal,  D::None,    false, 0, false, false, 100, 0},
        };

        if (TypeId < MaxTypeId)
            return Table[TypeId];

        static const FHktVoxelDef Default = {P::Solid, M::Normal, D::None, false, 0, false, false, 100, 0};
        return Default;
    }

    /**
     * TypeID에서 올바른 Flags가 설정된 FHktTerrainVoxel 생성.
     * Op_SetVoxel / 지형 생성기에서 사용.
     */
    inline FHktTerrainVoxel MakeVoxel(uint16 TypeId, uint8 PaletteIndex = 0)
    {
        FHktTerrainVoxel V;
        V.TypeID       = TypeId;
        V.PaletteIndex = PaletteIndex;
        V.Flags        = GetDef(TypeId).AutoFlags;
        return V;
    }
}
