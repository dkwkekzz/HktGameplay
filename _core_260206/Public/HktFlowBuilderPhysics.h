// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "Physics/HktPhysicsTypes.h"

/**
 * FlowBuilder Physics 확장 매크로/헬퍼
 * 
 * FFlowBuilder에 직접 메서드를 추가하는 대신,
 * 확장 함수 형태로 제공하여 기존 코드 수정 최소화
 * 
 * 사용 예:
 *   Flow(TEXT("Ability.Skill.Fireball"))
 *       .SpawnEntity(TEXT("/Game/BP_Fireball"))
 *       << SetColliderSphere(Spawned, 30, Layer::Projectile, Layer::Enemy)
 *       .WaitCollision(Spawned)
 *       ...
 */

// ============================================================================
// FlowBuilder 확장용 헬퍼 (인라인 명령어 시퀀스)
// ============================================================================

/**
 * Sphere 충돌체 설정을 위한 명령어 시퀀스 생성
 * 
 * @param Builder - FlowBuilder 참조
 * @param Entity - 대상 엔티티 레지스터
 * @param RadiusCm - 반경 (cm)
 * @param Layer - 자신의 레이어
 * @param Mask - 충돌 대상 마스크
 */
template<typename TFlowBuilder>
TFlowBuilder& SetColliderSphere(TFlowBuilder& Builder, uint8 Entity, int32 RadiusCm,
    uint8 Layer = HktPhysics::Layer::Default, 
    uint8 Mask = HktPhysics::Layer::All)
{
    // ColliderType = Sphere
    Builder.LoadConst(Reg::Temp, static_cast<int32>(EHktColliderType::Sphere));
    Builder.SaveEntityProperty(Entity, PropertyId::ColliderType, Reg::Temp);
    
    // ColliderRadius
    Builder.LoadConst(Reg::Temp, RadiusCm);
    Builder.SaveEntityProperty(Entity, PropertyId::ColliderRadius, Reg::Temp);
    
    // CollisionLayer
    Builder.LoadConst(Reg::Temp, static_cast<int32>(Layer));
    Builder.SaveEntityProperty(Entity, PropertyId::CollisionLayer, Reg::Temp);
    
    // CollisionMask
    Builder.LoadConst(Reg::Temp, static_cast<int32>(Mask));
    Builder.SaveEntityProperty(Entity, PropertyId::CollisionMask, Reg::Temp);
    
    return Builder;
}

/**
 * Capsule 충돌체 설정을 위한 명령어 시퀀스 생성
 */
template<typename TFlowBuilder>
TFlowBuilder& SetColliderCapsule(TFlowBuilder& Builder, uint8 Entity, 
    int32 HalfHeightCm, int32 RadiusCm,
    uint8 Layer = HktPhysics::Layer::Default,
    uint8 Mask = HktPhysics::Layer::All)
{
    // ColliderType = Capsule
    Builder.LoadConst(Reg::Temp, static_cast<int32>(EHktColliderType::Capsule));
    Builder.SaveEntityProperty(Entity, PropertyId::ColliderType, Reg::Temp);
    
    // ColliderHalfHeight
    Builder.LoadConst(Reg::Temp, HalfHeightCm);
    Builder.SaveEntityProperty(Entity, PropertyId::ColliderHalfHeight, Reg::Temp);
    
    // ColliderRadius
    Builder.LoadConst(Reg::Temp, RadiusCm);
    Builder.SaveEntityProperty(Entity, PropertyId::ColliderRadius, Reg::Temp);
    
    // CollisionLayer
    Builder.LoadConst(Reg::Temp, static_cast<int32>(Layer));
    Builder.SaveEntityProperty(Entity, PropertyId::CollisionLayer, Reg::Temp);
    
    // CollisionMask
    Builder.LoadConst(Reg::Temp, static_cast<int32>(Mask));
    Builder.SaveEntityProperty(Entity, PropertyId::CollisionMask, Reg::Temp);
    
    return Builder;
}

/**
 * 충돌 레이어만 설정
 */
template<typename TFlowBuilder>
TFlowBuilder& SetCollisionLayer(TFlowBuilder& Builder, uint8 Entity, uint8 Layer)
{
    Builder.LoadConst(Reg::Temp, static_cast<int32>(Layer));
    Builder.SaveEntityProperty(Entity, PropertyId::CollisionLayer, Reg::Temp);
    return Builder;
}

/**
 * 충돌 마스크만 설정
 */
template<typename TFlowBuilder>
TFlowBuilder& SetCollisionMask(TFlowBuilder& Builder, uint8 Entity, uint8 Mask)
{
    Builder.LoadConst(Reg::Temp, static_cast<int32>(Mask));
    Builder.SaveEntityProperty(Entity, PropertyId::CollisionMask, Reg::Temp);
    return Builder;
}

/**
 * 충돌체 비활성화 (ColliderType = None)
 */
template<typename TFlowBuilder>
TFlowBuilder& DisableCollider(TFlowBuilder& Builder, uint8 Entity)
{
    Builder.LoadConst(Reg::Temp, static_cast<int32>(EHktColliderType::None));
    Builder.SaveEntityProperty(Entity, PropertyId::ColliderType, Reg::Temp);
    return Builder;
}

// ============================================================================
// 레지스터 별칭 (Physics 관련)
// ============================================================================

namespace Reg
{
    // 기존 레지스터와 동일, 명시적 문서화 목적
    // Spawned = 12: SpawnEntity 결과, 충돌체 설정 대상
    // Hit = 13: WaitCollision 결과, 충돌 상대 엔티티
}
