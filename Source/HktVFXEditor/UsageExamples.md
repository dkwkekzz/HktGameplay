// ============================================================================
// 사용 예시: 프로그래머가 이 시스템을 어떻게 쓰는가
// ============================================================================

// ==========================================================================
// [1] 에디터에서: 커맨드렛이나 에디터 유틸리티 위젯으로 VFX 생성
// ==========================================================================

// 방법 A: 에디터 유틸리티 블루프린트 또는 Python에서 호출
// (에디터에서 한 번만 실행 → 에셋이 디스크에 저장됨)

/*
 * Editor Utility Widget에서:
 * 
 * 1. VFXGeneratorSubsystem 가져오기
 * 2. Intent 설정
 * 3. GenerateVFX 호출
 * 4. Content Browser에 Niagara 에셋 생성됨
 */

// 방법 B: C++ 코드로 직접 호출 (에디터 모듈에서)
void GenerateAllGameVFX()
{
    UVFXGeneratorSubsystem* Gen = GEditor->GetEditorSubsystem<UVFXGeneratorSubsystem>();
    if (!Gen) return;

    // --- 기본 전투 이펙트 ---

    // 화염 폭발 (약/중/강)
    {
        FVFXGenerationRequest Req;
        Req.Intent.EventType = EVFXEventType::Explosion;
        Req.Intent.Element = EVFXElement::Fire;
        Req.Intent.Intensity = 0.5f;
        Req.Intent.Radius = 300.f;
        Req.Intent.Duration = 1.5f;
        Req.Intent.SurfaceType = EVFXSurfaceType::Stone;
        Req.OutputDirectory = TEXT("/Game/VFX/Combat/Explosion");
        Gen->GenerateVFX(Req);
    }

    // 얼음 투사체 궤적
    {
        FVFXGenerationRequest Req;
        Req.Intent.EventType = EVFXEventType::ProjectileTrail;
        Req.Intent.Element = EVFXElement::Ice;
        Req.Intent.Intensity = 0.6f;
        Req.Intent.Radius = 50.f;
        Req.Intent.Duration = 0.f;  // 투사체 수명 동안 지속
        Req.OutputDirectory = TEXT("/Game/VFX/Combat/Projectile");
        Gen->GenerateVFX(Req);
    }

    // 아케인 힐
    {
        FVFXGenerationRequest Req;
        Req.Intent.EventType = EVFXEventType::Heal;
        Req.Intent.Element = EVFXElement::Arcane;
        Req.Intent.Intensity = 0.7f;
        Req.Intent.Radius = 100.f;
        Req.Intent.Duration = 2.0f;
        Req.Intent.StyleKeywords = { TEXT("mystical"), TEXT("runes"), TEXT("ascending") };
        Req.OutputDirectory = TEXT("/Game/VFX/Combat/Heal");
        Gen->GenerateVFX(Req);
    }

    // --- 퀵 생성 (자연어) ---
    Gen->QuickGenerate(TEXT("massive dark explosion with purple void energy"));
    Gen->QuickGenerate(TEXT("subtle holy buff aura with golden particles"));
    Gen->QuickGenerate(TEXT("lightning trail, fast, bright cyan sparks"));

    // --- 프리셋 뱅크 자동 생성 (조합 폭발!) ---
    // 5 이벤트 x 4 속성 x 3 강도 = 60개 VFX 자동 생성
    Gen->GeneratePresetBank(
        { EVFXEventType::Explosion, EVFXEventType::ProjectileHit,
          EVFXEventType::Buff, EVFXEventType::Heal, EVFXEventType::AreaEffect },
        { EVFXElement::Fire, EVFXElement::Ice, EVFXElement::Lightning, EVFXElement::Dark },
        { 0.3f, 0.6f, 1.0f }
    );
}


// ==========================================================================
// [2] 런타임: 게임플레이 코드에서 VFX 사용
// ==========================================================================

// 예시: 스킬 시스템에서 VFX 발동
UCLASS()
class AMyCharacter : public ACharacter
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    UVFXRuntimeResolver* VFXResolver;

    AMyCharacter()
    {
        VFXResolver = CreateDefaultSubobject<UVFXRuntimeResolver>(TEXT("VFXResolver"));
    }

    // --- 스킬 사용 시 ---
    void CastFireball(FVector TargetLocation)
    {
        // 1. 시전 이펙트 (캐릭터에 붙음)
        FVFXIntent CastIntent;
        CastIntent.EventType = EVFXEventType::Channel;
        CastIntent.Element = EVFXElement::Fire;
        CastIntent.Intensity = 0.4f;
        CastIntent.Radius = 50.f;
        CastIntent.Duration = 0.5f;
        CastIntent.Location = GetActorLocation();

        VFXResolver->PlayVFXAttached(CastIntent, GetMesh(), TEXT("RightHand"));

        // 2. 파이어볼 스폰 (별도 액터)
        // ... 투사체 로직 ...
    }

    // 파이어볼이 적중했을 때 호출
    void OnFireballHit(FVector HitLocation, FVector HitNormal,
                       float Damage, float MaxDamage, EPhysicalSurface Surface)
    {
        FVFXIntent HitIntent;
        HitIntent.EventType = EVFXEventType::Explosion;
        HitIntent.Element = EVFXElement::Fire;
        HitIntent.Location = HitLocation;
        HitIntent.Direction = -HitNormal;
        HitIntent.SurfaceNormal = HitNormal;

        // 시뮬레이션 결과를 VFX 파라미터로 변환!
        HitIntent.Intensity = FMath::Clamp(Damage / MaxDamage, 0.1f, 1.0f);
        HitIntent.Radius = FMath::Lerp(100.f, 500.f, HitIntent.Intensity);
        HitIntent.Duration = FMath::Lerp(0.8f, 2.0f, HitIntent.Intensity);

        // 표면 타입 변환
        switch (Surface)
        {
        case SurfaceType1: HitIntent.SurfaceType = EVFXSurfaceType::Stone; break;
        case SurfaceType2: HitIntent.SurfaceType = EVFXSurfaceType::Metal; break;
        case SurfaceType3: HitIntent.SurfaceType = EVFXSurfaceType::Water; break;
        default:           HitIntent.SurfaceType = EVFXSurfaceType::None;  break;
        }

        // 캐릭터 레벨 반영
        HitIntent.SourcePower = FMath::Clamp(CharacterLevel / 50.f, 0.f, 1.f);

        VFXResolver->PlayVFX(HitIntent);
    }

    // --- 버프/디버프 시 ---
    void ApplyBuff(EVFXElement Element, float Potency)
    {
        FVFXIntent BuffIntent;
        BuffIntent.EventType = EVFXEventType::Buff;
        BuffIntent.Element = Element;
        BuffIntent.Intensity = Potency;
        BuffIntent.Radius = 80.f;
        BuffIntent.Duration = 5.0f;

        VFXResolver->PlayVFXAttached(BuffIntent, GetMesh());
    }

    // --- 죽음 시 ---
    void OnDeath(EVFXElement KillingElement)
    {
        FVFXIntent DeathIntent;
        DeathIntent.EventType = EVFXEventType::Death;
        DeathIntent.Element = KillingElement;  // 어떤 속성으로 죽었는지
        DeathIntent.Intensity = 0.8f;
        DeathIntent.Location = GetActorLocation();

        VFXResolver->PlayVFX(DeathIntent);
    }

    int32 CharacterLevel = 1;
};


// ==========================================================================
// [3] HktSimulation VM 연동 예시
// ==========================================================================

/*
 * HktSimulation의 OpCode 실행 결과에서 VFX Intent를 추출하는 패턴
 *
 * VM에서 스킬이 실행될 때:
 *   cast → fireball creation → forward movement → collision → explosion
 *
 * 각 단계에서 VFX Intent를 emit:
 *   cast     → Channel + Fire
 *   creation → Summon + Fire  
 *   movement → ProjectileTrail + Fire
 *   collision→ ProjectileHit + Fire
 *   explosion→ Explosion + Fire + (시뮬레이션 데미지 기반 Intensity)
 */

// VM의 OpCode 핸들러에서:
struct FSkillVFXBridge
{
    UVFXRuntimeResolver* Resolver;

    // VM에서 호출: 이벤트 발생 시 VFX 매핑
    void OnSimulationEvent(
        const FString& EventName,        // "explosion", "hit" 등
        const FString& ElementName,      // "fire", "ice" 등
        FVector Location,
        FVector Direction,
        float NormalizedDamage,          // 0~1
        float EffectRadius)
    {
        FVFXIntent Intent;

        // EventName → EVFXEventType 변환
        static TMap<FString, EVFXEventType> EventMap = {
            {TEXT("explosion"),  EVFXEventType::Explosion},
            {TEXT("hit"),        EVFXEventType::ProjectileHit},
            {TEXT("trail"),      EVFXEventType::ProjectileTrail},
            {TEXT("cast"),       EVFXEventType::Channel},
            {TEXT("summon"),     EVFXEventType::Summon},
            {TEXT("area"),       EVFXEventType::AreaEffect},
            {TEXT("buff"),       EVFXEventType::Buff},
            {TEXT("debuff"),     EVFXEventType::Debuff},
            {TEXT("heal"),       EVFXEventType::Heal},
        };

        static TMap<FString, EVFXElement> ElementMap = {
            {TEXT("fire"),      EVFXElement::Fire},
            {TEXT("ice"),       EVFXElement::Ice},
            {TEXT("lightning"), EVFXElement::Lightning},
            {TEXT("dark"),      EVFXElement::Dark},
            {TEXT("holy"),      EVFXElement::Holy},
            {TEXT("poison"),    EVFXElement::Poison},
            {TEXT("arcane"),    EVFXElement::Arcane},
        };

        Intent.EventType = EventMap.FindRef(EventName);
        Intent.Element = ElementMap.FindRef(ElementName);
        Intent.Location = Location;
        Intent.Direction = Direction;
        Intent.Intensity = NormalizedDamage;
        Intent.Radius = EffectRadius;

        Resolver->PlayVFX(Intent);
    }
};