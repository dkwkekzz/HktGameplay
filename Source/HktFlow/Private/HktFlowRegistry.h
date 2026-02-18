#pragma once

#include "CoreMinimal.h"

/**
 * Flow 자가 등록 시스템
 * * 중앙 헤더 수정 없이 각 cpp 파일에서 스스로를 등록할 수 있게 합니다.
 */
class FHktFlowRegistry
{
public:
    using FFlowRegisterFunc = TFunction<void()>;

    /** 각 Flow 구현부에서 등록을 위해 호출 */
    static void AddFlowRegistration(FFlowRegisterFunc InitFunc);

    /** 게임 시작 시(예: GameInstance Init) 호출하여 모든 등록된 Flow 빌드 */
    static void InitializeAllFlows();

private:
    /** 정적 초기화 순서 문제 방지를 위해 함수 내 정적 변수 사용 (Meyers Singleton) */
    static TArray<FFlowRegisterFunc>& GetRegistry();
};

/**
 * 자가 등록 헬퍼 구조체
 * * 이 구조체의 정적 인스턴스를 생성하면 생성자에서 자동으로 레지스트리에 등록됩니다.
 */
struct FHktAutoRegisterFlow
{
    FHktAutoRegisterFlow(FHktFlowRegistry::FFlowRegisterFunc InitFunc)
    {
        FHktFlowRegistry::AddFlowRegistration(InitFunc);
    }
};

/**
 * 매크로: Flow 등록을 간편하게 만듭니다.
 * 사용법: 
 * HKT_REGISTER_FLOW_BODY()
 * {
 * // Flow 정의 로직
 * }
 */
#define HKT_REGISTER_FLOW_BODY() \
    static void HktFlowRegisterImpl(); \
    static FHktAutoRegisterFlow HktFlowAutoRegInstance(&HktFlowRegisterImpl); \
    void HktFlowRegisterImpl()