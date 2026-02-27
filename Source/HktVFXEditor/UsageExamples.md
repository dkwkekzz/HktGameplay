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
 * 1. HktVFXGeneratorSubsystem 가져오기
 * 2. Intent 설정
 * 3. GenerateVFX 호출
 * 4. Content Browser에 Niagara 에셋 생성됨
 */

// 방법 B: C++ 코드로 직접 호출 (에디터 모듈에서)
void GenerateAllGameVFX()
{
    UHktVFXGeneratorSubsystem* Gen = GEditor->GetEditorSubsystem<UHktVFXGeneratorSubsystem>();
    if (!Gen) return;

    // --- 기본 전투 이펙트 ---

    // 화염 폭발 (약/중/강)
    {
        FHktVFXGenerationRequest Req;
        Req.Intent.EventType = EHktVFXEventType::Explosion;
        Req.Intent.Element = EHktVFXElement::Fire;
        Req.Intent.Intensity = 0.5f;
        Req.Intent.Radius = 300.f;
        Req.Intent.Duration = 1.5f;
        Req.Intent.SurfaceType = EHktVFXSurfaceType::Stone;
        Req.OutputDirectory = TEXT("/Game/VFX/Combat/Explosion");
        Gen->GenerateVFX(Req);
    }

    // 얼음 투사체 궤적
    {
        FHktVFXGenerationRequest Req;
        Req.Intent.EventType = EHktVFXEventType::ProjectileTrail;
        Req.Intent.Element = EHktVFXElement::Ice;
        Req.Intent.Intensity = 0.6f;
        Req.Intent.Radius = 50.f;
        Req.Intent.Duration = 0.f;  // 투사체 수명 동안 지속
        Req.OutputDirectory = TEXT("/Game/VFX/Combat/Projectile");
        Gen->GenerateVFX(Req);
    }

    // 아케인 힐
    {
        FHktVFXGenerationRequest Req;
        Req.Intent.EventType = EHktVFXEventType::Heal;
        Req.Intent.Element = EHktVFXElement::Arcane;
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
        { EHktVFXEventType::Explosion, EHktVFXEventType::ProjectileHit,
          EHktVFXEventType::Buff, EHktVFXEventType::Heal, EHktVFXEventType::AreaEffect },
        { EHktVFXElement::Fire, EHktVFXElement::Ice, EHktVFXElement::Lightning, EHktVFXElement::Dark },
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
    UHktVFXRuntimeResolver* VFXResolver;

    AMyCharacter()
    {
        VFXResolver = CreateDefaultSubobject<UHktVFXRuntimeResolver>(TEXT("VFXResolver"));
    }

    // --- 스킬 사용 시 ---
    void CastFireball(FVector TargetLocation)
    {
        // 1. 시전 이펙트 (캐릭터에 붙음)
        FHktVFXIntent CastIntent;
        CastIntent.EventType = EHktVFXEventType::Channel;
        CastIntent.Element = EHktVFXElement::Fire;
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
        FHktVFXIntent HitIntent;
        HitIntent.EventType = EHktVFXEventType::Explosion;
        HitIntent.Element = EHktVFXElement::Fire;
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
        case SurfaceType1: HitIntent.SurfaceType = EHktVFXSurfaceType::Stone; break;
        case SurfaceType2: HitIntent.SurfaceType = EHktVFXSurfaceType::Metal; break;
        case SurfaceType3: HitIntent.SurfaceType = EHktVFXSurfaceType::Water; break;
        default:           HitIntent.SurfaceType = EHktVFXSurfaceType::None;  break;
        }

        // 캐릭터 레벨 반영
        HitIntent.SourcePower = FMath::Clamp(CharacterLevel / 50.f, 0.f, 1.f);

        VFXResolver->PlayVFX(HitIntent);
    }

    // --- 버프/디버프 시 ---
    void ApplyBuff(EHktVFXElement Element, float Potency)
    {
        FHktVFXIntent BuffIntent;
        BuffIntent.EventType = EHktVFXEventType::Buff;
        BuffIntent.Element = Element;
        BuffIntent.Intensity = Potency;
        BuffIntent.Radius = 80.f;
        BuffIntent.Duration = 5.0f;

        VFXResolver->PlayVFXAttached(BuffIntent, GetMesh());
    }

    // --- 죽음 시 ---
    void OnDeath(EHktVFXElement KillingElement)
    {
        FHktVFXIntent DeathIntent;
        DeathIntent.EventType = EHktVFXEventType::Death;
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
    UHktVFXRuntimeResolver* Resolver;

    // VM에서 호출: 이벤트 발생 시 VFX 매핑
    void OnSimulationEvent(
        const FString& EventName,        // "explosion", "hit" 등
        const FString& ElementName,      // "fire", "ice" 등
        FVector Location,
        FVector Direction,
        float NormalizedDamage,          // 0~1
        float EffectRadius)
    {
        FHktVFXIntent Intent;

        // EventName → EHktVFXEventType 변환
        static TMap<FString, EHktVFXEventType> EventMap = {
            {TEXT("explosion"),  EHktVFXEventType::Explosion},
            {TEXT("hit"),        EHktVFXEventType::ProjectileHit},
            {TEXT("trail"),      EHktVFXEventType::ProjectileTrail},
            {TEXT("cast"),       EHktVFXEventType::Channel},
            {TEXT("summon"),     EHktVFXEventType::Summon},
            {TEXT("area"),       EHktVFXEventType::AreaEffect},
            {TEXT("buff"),       EHktVFXEventType::Buff},
            {TEXT("debuff"),     EHktVFXEventType::Debuff},
            {TEXT("heal"),       EHktVFXEventType::Heal},
        };

        static TMap<FString, EHktVFXElement> ElementMap = {
            {TEXT("fire"),      EHktVFXElement::Fire},
            {TEXT("ice"),       EHktVFXElement::Ice},
            {TEXT("lightning"), EHktVFXElement::Lightning},
            {TEXT("dark"),      EHktVFXElement::Dark},
            {TEXT("holy"),      EHktVFXElement::Holy},
            {TEXT("poison"),    EHktVFXElement::Poison},
            {TEXT("arcane"),    EHktVFXElement::Arcane},
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