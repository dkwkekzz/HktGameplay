// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

/**
 * HktPhysics.h - HktCore Physics 모듈 통합 헤더
 *
 * HktCore의 Physics 기능을 사용하려면 이 헤더를 include하세요.
 *
 * 주요 구성:
 * - HktPhysicsTypes.h: 기본 타입, 상수, PropertyId
 * - HktPhysicsWorld.h: 물리 월드 클래스
 * - HktFlowBuilderPhysics.h: FlowBuilder 확장 헬퍼
 */

#include "Physics/HktPhysicsTypes.h"
#include "Physics/HktPhysicsWorld.h"

// Forward declarations
class IHktStashInterface;

/**
 * PhysicsWorld 팩토리 함수
 *
 * @param InStash - 엔티티 데이터 소스 (필수)
 * @return PhysicsWorld 인스턴스
 *
 * 사용 예:
 *   auto PhysicsWorld = CreatePhysicsWorld(MasterStash.Get());
 *
 *   // 매 프레임 충돌 감지 및 VM에 주입
 *   TArray<FHktCollisionPair> Collisions;
 *   PhysicsWorld->DetectWatchedCollisions(Collisions);
 *   for (auto& Pair : Collisions)
 *       VMProcessor->NotifyCollision(Pair.EntityA, Pair.EntityB);
 */
HKTCORE_API TUniquePtr<FHktPhysicsWorld> CreatePhysicsWorld(IHktStashInterface* InStash);
