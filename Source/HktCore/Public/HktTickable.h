// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IHktTickable - VM 틱 사이클에 참여하는 시스템 인터페이스
 * 
 * VMProcessor의 Tick에서 등록된 Tickable들을 순회하며 호출합니다.
 * Priority가 낮은 것부터 먼저 실행됩니다.
 * 
 * 사용 예:
 *   - PhysicsWorld: Priority=100 (VM Execute 후 충돌 감지)
 *   - 향후 Navigation, AI 등 확장 가능
 */
class HKTCORE_API IHktTickable
{
public:
    virtual ~IHktTickable() = default;
    
    /**
     * 매 프레임 호출
     * @param DeltaSeconds - 프레임 간 시간
     */
    virtual void Tick(float DeltaSeconds) = 0;
    
    /**
     * 틱 실행 우선순위 (낮을수록 먼저 실행)
     * 
     * 권장 값:
     *   0~99: VM 실행 전 시스템
     *   100~199: 물리/충돌
     *   200~299: 후처리
     */
    virtual int32 GetTickPriority() const = 0;
    
    /**
     * 틱 활성화 여부
     * false 반환 시 해당 프레임 스킵
     */
    virtual bool IsTickEnabled() const { return true; }
    
    /**
     * 디버그용 이름
     */
    virtual FName GetTickableName() const { return NAME_None; }
};

/**
 * 틱 우선순위 상수
 */
namespace HktTickPriority
{
    constexpr int32 PrePhysics = 50;
    constexpr int32 Physics = 100;
    constexpr int32 PostPhysics = 150;
    constexpr int32 Navigation = 200;
    constexpr int32 AI = 250;
    constexpr int32 Cleanup = 300;
}
