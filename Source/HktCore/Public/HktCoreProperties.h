// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FHktPropertyDef — 프로퍼티 정의 구조체
 *
 * 자동 부여된 ID + 이름 문자열.
 * operator uint16() 덕분에 기존 PropertyId enum 문법 그대로 사용 가능.
 */
struct FHktPropertyDef
{
    uint16 Id;
    const TCHAR* Name;

    FORCEINLINE operator uint16() const { return Id; }
    FORCEINLINE const TCHAR* ToString() const { return Name; }
};

// ============================================================================
// Detail — 프로퍼티 자동 ID 카운터 + 레지스트리
// ============================================================================

namespace HktProperty
{
    namespace Detail
    {
        inline uint16& Counter() { static uint16 C = 0; return C; }

        inline TArray<const FHktPropertyDef*>& Registry()
        {
            static TArray<const FHktPropertyDef*> R;
            return R;
        }
    }
}

// ============================================================================
// HKT_DEFINE_PROPERTY — 선언 순서대로 ID 자동 부여 + 레지스트리 등록
// ============================================================================

#define HKT_DEFINE_PROPERTY(PropName) \
    inline const FHktPropertyDef PropName = []() -> FHktPropertyDef { \
        FHktPropertyDef P{::HktProperty::Detail::Counter()++, TEXT(#PropName)}; \
        ::HktProperty::Detail::Registry().Add(&PropName); \
        return P; \
    }();

// ============================================================================
// HktProperty — 프로퍼티 정의 (선언 순서 = ID, 기존 enum 순서 유지)
// ============================================================================

namespace HktProperty
{
    // ===== Hot Properties (매 프레임 접근, O(1) 직접 인덱싱) =====

    // 위치/이동
    HKT_DEFINE_PROPERTY(PosX)               // 0
    HKT_DEFINE_PROPERTY(PosY)               // 1
    HKT_DEFINE_PROPERTY(PosZ)               // 2
    HKT_DEFINE_PROPERTY(RotYaw)             // 3
    HKT_DEFINE_PROPERTY(MoveTargetX)        // 4
    HKT_DEFINE_PROPERTY(MoveTargetY)        // 5
    HKT_DEFINE_PROPERTY(MoveTargetZ)        // 6
    HKT_DEFINE_PROPERTY(MoveForce)          // 7
    HKT_DEFINE_PROPERTY(IsMoving)           // 8
    HKT_DEFINE_PROPERTY(IsGrounded)         // 9
    HKT_DEFINE_PROPERTY(MaxSpeed)           // 10

    // 전투/상태
    HKT_DEFINE_PROPERTY(Health)             // 11
    HKT_DEFINE_PROPERTY(MaxHealth)          // 12
    HKT_DEFINE_PROPERTY(AttackPower)        // 13
    HKT_DEFINE_PROPERTY(Defense)            // 14
    HKT_DEFINE_PROPERTY(Team)               // 15
    HKT_DEFINE_PROPERTY(Mana)               // 16
    HKT_DEFINE_PROPERTY(MaxMana)            // 17

    // 소유
    HKT_DEFINE_PROPERTY(OwnerEntity)        // 18
    HKT_DEFINE_PROPERTY(EntitySpawnTag)     // 19

    // 스탠스
    HKT_DEFINE_PROPERTY(Stance)             // 20

    // 전투 (CP/공속)
    HKT_DEFINE_PROPERTY(CP)                 // 21
    HKT_DEFINE_PROPERTY(MaxCP)              // 22
    HKT_DEFINE_PROPERTY(AttackSpeed)        // 23
    HKT_DEFINE_PROPERTY(MotionPlayRate)     // 24
    HKT_DEFINE_PROPERTY(NextActionFrame)    // 25

    // 충돌
    HKT_DEFINE_PROPERTY(CollisionLayer)     // 26
    HKT_DEFINE_PROPERTY(CollisionMask)      // 27
    HKT_DEFINE_PROPERTY(CollisionRadius)    // 28
    HKT_DEFINE_PROPERTY(Mass)              // 29

    inline const uint16 HotMaxCount = Detail::Counter();  // = 30

    // ===== Cold Properties (공간 절약, 선형 탐색) =====

    // 이벤트 파라미터
    HKT_DEFINE_PROPERTY(TargetPosX)         // 30
    HKT_DEFINE_PROPERTY(TargetPosY)         // 31
    HKT_DEFINE_PROPERTY(TargetPosZ)         // 32
    HKT_DEFINE_PROPERTY(Param0)             // 33
    HKT_DEFINE_PROPERTY(Param1)             // 34
    HKT_DEFINE_PROPERTY(Param2)             // 35
    HKT_DEFINE_PROPERTY(Param3)             // 36

    // 애니메이션/비주얼
    HKT_DEFINE_PROPERTY(AnimState)          // 37
    HKT_DEFINE_PROPERTY(VisualState)        // 38
    HKT_DEFINE_PROPERTY(AnimStateUpper)     // 39

    // 물리
    HKT_DEFINE_PROPERTY(VelX)               // 40
    HKT_DEFINE_PROPERTY(VelY)               // 41
    HKT_DEFINE_PROPERTY(VelZ)               // 42

    // 아이템
    HKT_DEFINE_PROPERTY(ItemState)          // 43
    HKT_DEFINE_PROPERTY(ItemId)             // 44
    HKT_DEFINE_PROPERTY(EquipIndex)         // 45

    // 가방
    HKT_DEFINE_PROPERTY(BagCapacity)        // 46

    // NPC
    HKT_DEFINE_PROPERTY(IsNPC)              // 47
    HKT_DEFINE_PROPERTY(SpawnFlowTag)       // 48

    // 아이템 스킬
    HKT_DEFINE_PROPERTY(ItemSkillTag)       // 49
    HKT_DEFINE_PROPERTY(SkillCPCost)        // 50
    HKT_DEFINE_PROPERTY(RecoveryFrame)      // 51
    HKT_DEFINE_PROPERTY(SkillTargetRequired) // 52

    // 공격 사거리
    HKT_DEFINE_PROPERTY(AttackRange)        // 53

    // 장착 가능 여부
    HKT_DEFINE_PROPERTY(Equippable)         // 54

    // 캐릭터 장착 슬롯
    HKT_DEFINE_PROPERTY(EquipSlot0)         // 55
    HKT_DEFINE_PROPERTY(EquipSlot1)         // 56
    HKT_DEFINE_PROPERTY(EquipSlot2)         // 57
    HKT_DEFINE_PROPERTY(EquipSlot3)         // 58
    HKT_DEFINE_PROPERTY(EquipSlot4)         // 59
    HKT_DEFINE_PROPERTY(EquipSlot5)         // 60
    HKT_DEFINE_PROPERTY(EquipSlot6)         // 61
    HKT_DEFINE_PROPERTY(EquipSlot7)         // 62
    HKT_DEFINE_PROPERTY(EquipSlot8)         // 63

    // 복셀 스킨
    HKT_DEFINE_PROPERTY(VoxelSkinSet)       // 64
    HKT_DEFINE_PROPERTY(VoxelPalette)       // 65

    inline const uint16 MaxCount = Detail::Counter();  // = 66

    // ================================================================
    // 유틸리티 함수
    // ================================================================

    /** PropId → 이름 문자열 (O(1) 배열 인덱싱) */
    inline const TCHAR* GetPropertyName(uint16 PropId)
    {
        static const TCHAR** Table = []() {
            static const TCHAR* T[256]{};
            for (const auto* P : Detail::Registry())
                T[P->Id] = P->Name;
            return T;
        }();
        return PropId < MaxCount ? Table[PropId] : nullptr;
    }

    /** 이름 → FHktPropertyDef 검색 (JSON 파서용) */
    inline const FHktPropertyDef* FindByName(const FString& InName)
    {
        for (const auto* P : Detail::Registry())
        {
            if (InName == P->Name)
                return P;
        }
        return nullptr;
    }
}

// 하위 호환 — 기존 PropertyId::PosX 문법 유지
namespace PropertyId = HktProperty;
