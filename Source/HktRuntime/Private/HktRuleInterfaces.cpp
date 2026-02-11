#include "HktRuleInterfaces.h"
#include "HktRuleSubsystem.h"

namespace HktRule
{
    /** IHktServerRule 인스턴스 생성 (HktRuntime 내부 구현) */
    TSharedPtr<IHktServerRule> GetServerRule(UWorld* InWorld)
    {
        UHktRuleSubsystem* RuleSubsystem = UHktRuleSubsystem::Get(InWorld);
        if (!RuleSubsystem)
        {
            return nullptr;
        }
    
        return RuleSubsystem->GetServerRule();
    }
    
    /** IHktClientRule 인스턴스 생성 (HktRuntime 내부 구현) */
    TSharedPtr<IHktClientRule> GetClientRule(UWorld* InWorld)
    {
        UHktRuleSubsystem* RuleSubsystem = UHktRuleSubsystem::Get(InWorld);
        if (!RuleSubsystem)
        {
            return nullptr;
        }
    
        return RuleSubsystem->GetClientRule();
    }
}