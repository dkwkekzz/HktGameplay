// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktPhysicsTypes.h"

/**
 * FFlowBuilder Physics Extensions
 * 
 * FlowBuilder에 추가할 Physics 관련 메서드들
 * 실제로는 HktVMProgram.h의 FFlowBuilder 클래스에 통합해야 합니다.
 * 
 * 이 파일은 참조용으로, 아래 메서드들을 FFlowBuilder에 추가하세요.
 */

/*
// ========================================================================
// Collider Setup (via Store Properties)
// ========================================================================

/// Sphere 충돌체 설정
FFlowBuilder& SetColliderSphere(RegisterIndex Entity, int32 RadiusCm,
                                 uint8 Layer = HktPhysics::Layer::Default,
                                 uint8 Mask = HktPhysics::Layer::All)
{
    // ColliderType = Sphere
    LoadConst(Reg::Temp, static_cast<int32>(EHktColliderType::Sphere));
    SaveEntityProperty(Entity, PropertyId::ColliderType, Reg::Temp);
    
    // Radius
    LoadConst(Reg::Temp, RadiusCm);
    SaveEntityProperty(Entity, PropertyId::ColliderRadius, Reg::Temp);
    
    // Layer
    LoadConst(Reg::Temp, static_cast<int32>(Layer));
    SaveEntityProperty(Entity, PropertyId::CollisionLayer, Reg::Temp);
    
    // Mask
    LoadConst(Reg::Temp, static_cast<int32>(Mask));
    SaveEntityProperty(Entity, PropertyId::CollisionMask, Reg::Temp);
    
    return *this;
}

/// Capsule 충돌체 설정
FFlowBuilder& SetColliderCapsule(RegisterIndex Entity, 
                                  int32 HalfHeightCm, int32 RadiusCm,
                                  uint8 Layer = HktPhysics::Layer::Default,
                                  uint8 Mask = HktPhysics::Layer::All)
{
    // ColliderType = Capsule
    LoadConst(Reg::Temp, static_cast<int32>(EHktColliderType::Capsule));
    SaveEntityProperty(Entity, PropertyId::ColliderType, Reg::Temp);
    
    // HalfHeight
    LoadConst(Reg::Temp, HalfHeightCm);
    SaveEntityProperty(Entity, PropertyId::ColliderHalfHeight, Reg::Temp);
    
    // Radius
    LoadConst(Reg::Temp, RadiusCm);
    SaveEntityProperty(Entity, PropertyId::ColliderRadius, Reg::Temp);
    
    // Layer
    LoadConst(Reg::Temp, static_cast<int32>(Layer));
    SaveEntityProperty(Entity, PropertyId::CollisionLayer, Reg::Temp);
    
    // Mask
    LoadConst(Reg::Temp, static_cast<int32>(Mask));
    SaveEntityProperty(Entity, PropertyId::CollisionMask, Reg::Temp);
    
    return *this;
}

/// 충돌 레이어만 설정
FFlowBuilder& SetCollisionLayer(RegisterIndex Entity, uint8 Layer)
{
    LoadConst(Reg::Temp, static_cast<int32>(Layer));
    SaveEntityProperty(Entity, PropertyId::CollisionLayer, Reg::Temp);
    return *this;
}

/// 충돌 마스크만 설정
FFlowBuilder& SetCollisionMask(RegisterIndex Entity, uint8 Mask)
{
    LoadConst(Reg::Temp, static_cast<int32>(Mask));
    SaveEntityProperty(Entity, PropertyId::CollisionMask, Reg::Temp);
    return *this;
}

/// 충돌체 비활성화
FFlowBuilder& DisableCollider(RegisterIndex Entity)
{
    LoadConst(Reg::Temp, static_cast<int32>(EHktColliderType::None));
    SaveEntityProperty(Entity, PropertyId::ColliderType, Reg::Temp);
    return *this;
}
*/

/**
 * Flow 정의 예제 (Physics 통합)
 */
namespace PhysicsFlowExamples
{
    /**
     * 파이어볼 스킬 (Physics 버전)
     * 
     * 기존 버전과 동일하나, 충돌체 설정이 추가됨
     */
    /*
    inline void RegisterFireball_Physics()
    {
        using namespace Reg;
        using namespace HktPhysics;
        
        Flow(TEXT("Ability.Skill.Fireball.Physics"))
            .Log(TEXT("Fireball: 시전"))
            .PlayAnim(Self, TEXT("Cast"))
            .WaitSeconds(0.5f)
            
            // 파이어볼 스폰
            .SpawnEntity(TEXT("/Game/BP_Fireball"))
            
            // === 충돌체 설정 (새로 추가) ===
            // Sphere(30cm), Projectile 레이어, Enemy만 충돌
            .SetColliderSphere(Spawned, 30, Layer::Projectile, Layer::Enemy)
            
            // 위치/이동 설정
            .GetPosition(R0, Self)
            .SetPosition(Spawned, R0)
            .MoveForward(Spawned, 1000)
            
            // 충돌 대기 (PhysicsWorld가 감지)
            .WaitCollision(Spawned)
            
            // 충돌 처리
            .GetPosition(R3, Spawned)
            .DestroyEntity(Spawned)
            .ApplyDamageConst(Hit, 100)
            
            // 범위 피해
            .ForEachInRadius(Hit, 300)
                .Move(Target, Iter)
                .ApplyDamageConst(Target, 50)
            .EndForEach()
            
            .Halt()
            .BuildAndRegister();
    }
    */
    
    /**
     * 캐릭터 스폰 (Physics 버전)
     * 
     * 캐릭터에 Capsule 충돌체 설정
     */
    /*
    inline void RegisterCharacterSpawn_Physics()
    {
        using namespace Reg;
        using namespace HktPhysics;
        
        Flow(TEXT("Event.Character.Spawn.Physics"))
            .Log(TEXT("CharacterSpawn: 캐릭터 생성"))
            
            // 캐릭터 스폰
            .SpawnEntity(TEXT("/Game/Characters/BP_PlayerCharacter"))
            .Move(Self, Spawned)
            
            // === 충돌체 설정 (새로 추가) ===
            // Capsule(HalfHeight=90cm, Radius=40cm), Player 레이어
            .SetColliderCapsule(Self, 90, 40, Layer::Player, Layer::All)
            
            // 스폰 위치 설정
            .LoadStore(R0, PropertyId::TargetPosX)
            .LoadStore(R1, PropertyId::TargetPosY)
            .LoadStore(R2, PropertyId::TargetPosZ)
            .SetPosition(Self, R0)
            
            // 스폰 이펙트
            .PlayVFXAttached(Self, TEXT("/Game/VFX/SpawnEffect"))
            .PlayAnim(Self, TEXT("Spawn"))
            .WaitSeconds(0.5f)
            
            .PlayAnim(Self, TEXT("Idle"))
            
            .Log(TEXT("CharacterSpawn: 완료"))
            .Halt()
            .BuildAndRegister();
    }
    */
}
