// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

// HktRuntime Private only — 외부 노출 금지

#if WITH_HKT_INSIGHTS

#include "HktInsightsRuntimeTypes.h"
#include "HktWorldState.h"
#include "HktPropertyIds.h"

namespace HktWorldStateInsights
{
    /** PropertyId → 짧은 표시 이름 */
    inline FString PropIdToName(uint16 PropId)
    {
        switch (PropId)
        {
        case PropertyId::PosX:            return TEXT("PosX");
        case PropertyId::PosY:            return TEXT("PosY");
        case PropertyId::PosZ:            return TEXT("PosZ");
        case PropertyId::RotYaw:          return TEXT("RotYaw");
        case PropertyId::MoveTargetX:     return TEXT("MoveTargX");
        case PropertyId::MoveTargetY:     return TEXT("MoveTargY");
        case PropertyId::MoveTargetZ:     return TEXT("MoveTargZ");
        case PropertyId::MoveSpeed:       return TEXT("MoveSpeed");
        case PropertyId::IsMoving:        return TEXT("IsMoving");
        case PropertyId::Health:          return TEXT("Health");
        case PropertyId::MaxHealth:       return TEXT("MaxHealth");
        case PropertyId::AttackPower:     return TEXT("AtkPow");
        case PropertyId::Defense:         return TEXT("Defense");
        case PropertyId::Team:            return TEXT("Team");
        case PropertyId::Mana:            return TEXT("Mana");
        case PropertyId::MaxMana:         return TEXT("MaxMana");
        case PropertyId::OwnerEntity:     return TEXT("OwnerEnt");
        case PropertyId::EntityType:      return TEXT("EntType");
        case PropertyId::TargetPosX:      return TEXT("TargPosX");
        case PropertyId::TargetPosY:      return TEXT("TargPosY");
        case PropertyId::TargetPosZ:      return TEXT("TargPosZ");
        case PropertyId::Param0:          return TEXT("Param0");
        case PropertyId::Param1:          return TEXT("Param1");
        case PropertyId::Param2:          return TEXT("Param2");
        case PropertyId::Param3:          return TEXT("Param3");
        case PropertyId::AnimState:       return TEXT("AnimState");
        case PropertyId::VisualState:     return TEXT("VisState");
        case PropertyId::OwnerPlayerHash: return TEXT("Owner");
        default:                          return FString::Printf(TEXT("P%d"), PropId);
        }
    }

    /** TypeId → 표시 이름 */
    inline FString TypeIdToName(FHktTypeId TypeId)
    {
        switch (TypeId)
        {
        case HktType::Unit:       return TEXT("Unit");
        case HktType::Projectile: return TEXT("Projectile");
        case HktType::Equipment:  return TEXT("Equipment");
        case HktType::Building:   return TEXT("Building");
        default:                  return FString::Printf(TEXT("Type%d"), TypeId);
        }
    }

    /**
     * FHktWorldState → FHktWorldStateSnapshot 변환
     *
     * 모든 활성 엔티티의 타입별 Pool을 순회하여
     * 각 엔티티의 모든 프로퍼티를 FHktWorldEntityRow에 담습니다.
     *
     * @param WS         읽기 전용 WorldState
     * @param SourceName "Server[0]", "Client" 등 소스 식별자
     */
    inline FHktWorldStateSnapshot BuildSnapshot(const FHktWorldState& WS, const FString& SourceName)
    {
        FHktWorldStateSnapshot Snapshot;
        Snapshot.SourceName   = SourceName;
        Snapshot.FrameNumber  = WS.FrameNumber;
        Snapshot.CaptureTime  = FPlatformTime::Seconds();
        Snapshot.EntityCount  = 0;

        for (int32 T = 1; T < HktType::MaxTypes; ++T)
        {
            const FHktEntityPool& Pool = WS.GetPool(static_cast<FHktTypeId>(T));
            if (Pool.ActiveCount == 0 || !Pool.Schema)
            {
                continue;
            }

            Snapshot.EntityCount += Pool.ActiveCount;

            Pool.ForEachEntity([&](FHktEntityId EntityId, int32 Slot)
            {
                FHktWorldEntityRow Row;
                Row.EntityId = EntityId;
                Row.TypeName = TypeIdToName(static_cast<FHktTypeId>(T));

                const int32 Stride = Pool.Schema->GetStride();
                Row.PropNames.Reserve(Stride);
                Row.PropValues.Reserve(Stride);

                for (int8 LocalIdx = 0; LocalIdx < Stride; ++LocalIdx)
                {
                    const uint16 PropId = Pool.Schema->PropertyIds[LocalIdx];
                    const int32  Value  = Pool.Get(Slot, LocalIdx);
                    Row.PropNames.Add(PropIdToName(PropId));
                    Row.PropValues.Add(Value);
                }

                Snapshot.Entities.Add(MoveTemp(Row));
            });
        }

        return Snapshot;
    }

} // namespace HktWorldStateInsights

#endif // WITH_HKT_INSIGHTS
