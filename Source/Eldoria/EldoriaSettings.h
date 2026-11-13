#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EldoriaSettings.generated.h"

class UPhysicsAsset;

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UStaticMesh;
class USoundBase;
class UParticleSystem;
class UMaterialInterface;

// Everything that defines an enemy type. Filled in from Config/DefaultGame.ini so imported
// art (Paragon characters, Fab props) can be swapped without touching code.
USTRUCT()
struct FEnemyArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) FName Name;
	UPROPERTY(EditAnywhere) FText DisplayName;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<USkeletalMesh> Mesh;
	// either an Animation Blueprint class...
	UPROPERTY(EditAnywhere) TSoftClassPtr<UAnimInstance> AnimClass;
	// ...or plain idle/run clips driven by UEldoriaAnimInstance (for characters without one)
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> IdleAnim;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> MoveAnim;
	// ground speed (cm/s) at which MoveAnim plays at normal rate
	UPROPERTY(EditAnywhere) float MoveAnimSpeed = 400.f;
	UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<UAnimSequenceBase>> AttackMontages;
	// hit and death can be montages or plain sequences; sequences play through HitSlot
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> HitAnim;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> DeathAnim;
	UPROPERTY(EditAnywhere) FName HitSlot = TEXT("UpperBody");
	// directional flinches (fall back to HitAnim when empty)
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> HitBackAnim;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> HitLeftAnim;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> HitRightAnim;
	// knocked to the ground: played forward to fall, then in reverse to get back up
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> KnockdownAnim;
	// variety: one is picked at random each time (falls, deaths)
	UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<UAnimSequenceBase>> KnockdownAnims;
	UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<UAnimSequenceBase>> DeathAnims;
	// The death clips only start the fall; the body is handed to a ragdoll partway through so it
	// always ends up lying flat. Overrides the mesh's own physics asset when set.
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UPhysicsAsset> RagdollPhysics;
	// how far into the death clip (0..1) the ragdoll takes over
	UPROPERTY(EditAnywhere) float RagdollAt = 0.7f;
	// Standing back up after a knockdown: GetUpAnim from GetUpFrom seconds (-1 = its start, or its end
	// when reversed). It should begin low, sitting, kneeling or crouched, and end standing; the ragdoll's
	// pose fades into that first frame. Without one the fall is played slowly in reverse.
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> GetUpAnim;
	UPROPERTY(EditAnywhere) float GetUpFrom = -1.f;
	UPROPERTY(EditAnywhere) bool bGetUpReverse = false;
	// dazed when the stance meter breaks
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> StunAnim;
	// how much punishment the guard takes before breaking (0 = cannot be stunned)
	UPROPERTY(EditAnywhere) float MaxStance = 60.f;
	// can heavy blows knock this enemy down while its guard is intact?
	UPROPERTY(EditAnywhere) bool bEasilyKnockedDown = true;
	// indices into AttackMontages that cannot be blocked (shown with a red glint)
	UPROPERTY(EditAnywhere) TArray<int32> UnblockableAttacks;
	// sparks spawned on the hero when this enemy's attack lands
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UParticleSystem> ImpactEffect;
	UPROPERTY(EditAnywhere) float MaxHealth = 60.f;
	UPROPERTY(EditAnywhere) float Damage = 10.f;
	UPROPERTY(EditAnywhere) float WalkSpeed = 380.f;
	UPROPERTY(EditAnywhere) float AttackRange = 180.f;
	UPROPERTY(EditAnywhere) float AttackCooldown = 1.6f;
	UPROPERTY(EditAnywhere) float AggroRange = 1600.f;
	UPROPERTY(EditAnywhere) float Scale = 1.f;
	UPROPERTY(EditAnywhere) int32 CoinDrop = 3;
	// rotation and offset that line the imported mesh up with the capsule
	UPROPERTY(EditAnywhere) float MeshYaw = -90.f;
	UPROPERTY(EditAnywhere) float MeshZ = -90.f;
};

// One shot of the new-game story: a line of narration over a slow camera move through the world.
USTRUCT()
struct FIntroShot
{
	GENERATED_BODY()

	// subtitle, as spoken
	UPROPERTY(EditAnywhere) FText Line;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<USoundBase> Narration;
	// where the shot is: an actor tag (the Objective_ markers), Hero, or None for world coordinates.
	// From / To / LookAt are offsets from it (in the hero's own frame for Hero).
	UPROPERTY(EditAnywhere) FName Anchor;
	UPROPERTY(EditAnywhere) FVector From = FVector::ZeroVector;
	UPROPERTY(EditAnywhere) FVector To = FVector::ZeroVector;
	UPROPERTY(EditAnywhere) FVector LookAt = FVector::ZeroVector;
	UPROPERTY(EditAnywhere) float Fov = 55.f;
};

// One hero attack. Moves chain into each other through the combo rules in AEldoriaCharacter.
USTRUCT()
struct FHeroMove
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) FName Name;
	// shown on screen when the move lands (leave empty for basic swings)
	UPROPERTY(EditAnywhere) FText Callout;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimSequenceBase> Anim;
	UPROPERTY(EditAnywhere) float Rate = 1.2f;
	UPROPERTY(EditAnywhere) float Damage = 20.f;
	UPROPERTY(EditAnywhere) float Reach = 230.f;
	UPROPERTY(EditAnywhere) float Radius = 110.f;
	// fraction of the animation when the blade connects / when the next move may start
	UPROPERTY(EditAnywhere) float HitTime = 0.42f;
	UPROPERTY(EditAnywhere) float EndTime = 0.8f;
	UPROPERTY(EditAnywhere) float Knockback = 420.f;
	// hits every enemy around the hero instead of only those in front
	UPROPERTY(EditAnywhere) bool bAllAround = false;
	// how much this blow wears down an enemy's guard
	UPROPERTY(EditAnywhere) float StanceDamage = 10.f;
	// throws enemies to the ground (if they can be knocked down)
	UPROPERTY(EditAnywhere) bool bKnockdown = false;
	// heavy moves glow as the hero winds up
	UPROPERTY(EditAnywhere) bool bWindUpGlow = false;
	UPROPERTY(EditAnywhere) float Cooldown = 0.f;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Eldoria"))
class ELDORIA_API UEldoriaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ---------- hero ----------
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<USkeletalMesh> HeroMesh;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftClassPtr<UAnimInstance> HeroAnimClass;
	// When a walk clip is set the hero is animated from code instead of HeroAnimClass: idle, walk and run
	// blended by speed (each clip's natural ground speed in cm/s), a falling loop, and jump / land clips.
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroIdleAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroWalkAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") float HeroWalkAnimSpeed = 240.f;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroRunAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") float HeroRunAnimSpeed = 560.f;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroFallAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroJumpAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroLandAnim;
	// Light1-3, Heavy1-2, Finisher, Spin, AirSlam
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TArray<FHeroMove> HeroMoves;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroDodgeAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroHitAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroDeathAnim;
	// standing back up after a knockdown: the stretch of this clip from GetUpStart to GetUpEnd (seconds)
	// should begin low (kneeling or crouched) and end standing
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroGetUpAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") float HeroGetUpStart = 0.f;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") float HeroGetUpEnd = 0.f;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") FName HeroAnimSlot = TEXT("UpperBody");
	// shield-raised pose held while blocking
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroBlockAnim;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") float HeroMeshYaw = -90.f;
	UPROPERTY(Config, EditAnywhere, Category = "Hero") float HeroMeshZ = -90.f;

	// ---------- enemies ----------
	UPROPERTY(Config, EditAnywhere, Category = "Enemies") TArray<FEnemyArchetype> Enemies;

	// ---------- props ----------
	UPROPERTY(Config, EditAnywhere, Category = "Props") TSoftObjectPtr<UStaticMesh> CoinMesh;
	UPROPERTY(Config, EditAnywhere, Category = "Props") TSoftObjectPtr<UMaterialInterface> CoinMaterial;
	UPROPERTY(Config, EditAnywhere, Category = "Props") TSoftObjectPtr<UStaticMesh> PotionMesh;
	UPROPERTY(Config, EditAnywhere, Category = "Props") TSoftObjectPtr<UStaticMesh> ChestMesh;
	UPROPERTY(Config, EditAnywhere, Category = "Props") TSoftObjectPtr<UStaticMesh> GateMesh;

	// ---------- sound ----------
	// ---------- effects ----------
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> HeroImpactEffect;
	// blood: splatter over the screen edges when the hero is hurt, spatter on the ground for every
	// wound, and a pool under the fallen (Scripts/make_materials.py)
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UMaterialInterface> BloodScreenMaterial;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UMaterialInterface> BloodSpatterDecal;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UMaterialInterface> BloodPoolDecal;
	// animated liquid splashing onto the ground where the blood lands (Niagara): flung, and pooling
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<class UNiagaraSystem> BloodSplashEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<class UNiagaraSystem> BloodPoolEffect;
	// Greystone's own ability effects
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> WhirlwindStartEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> WhirlwindLoopEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> WhirlwindEndEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> GroundSmashEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> BlockEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> WindUpEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> DodgeTrailEffect;
	UPROPERTY(Config, EditAnywhere, Category = "Effects") TSoftObjectPtr<UParticleSystem> ParryEffect;
	// the hero's execution of a stunned enemy
	UPROPERTY(Config, EditAnywhere, Category = "Hero") TSoftObjectPtr<UAnimSequenceBase> HeroExecutionAnim;

	// the story told when a new game starts (Story/intro_narration.txt, recorded by Scripts/make_narration.sh)
	UPROPERTY(Config, EditAnywhere, Category = "Story") TArray<FIntroShot> IntroShots;
	// the score under the story (the field music steps aside for it)
	UPROPERTY(Config, EditAnywhere, Category = "Story") TSoftObjectPtr<USoundBase> IntroMusic;
	// the score ducks under the narrator and swells back up between lines
	UPROPERTY(Config, EditAnywhere, Category = "Story") float IntroMusicVolume = 0.5f;
	UPROPERTY(Config, EditAnywhere, Category = "Story") float IntroMusicUnderVoice = 0.25f;

	// sounds with several recordings: one is picked at random each time
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> SwingSounds;
	// heavier whooshes for heavy moves, brutes and the boss
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> HeavySwingSounds;
	// heartbeat when the hero's health is low
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> LowHealthSounds;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> ImpactSounds;
	// recorded blade strikes start this far in, so the strike lands on contact
	UPROPERTY(Config, EditAnywhere, Category = "Sound") float ImpactStartTime = 0.f;
	// quieter body thud layered under each strike
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> ImpactBodySounds;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> Music;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") float MusicMenuVolume = 0.8f;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") float MusicGameVolume = 0.22f;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> BlockSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> WalkFootstepSounds;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> RunFootstepSounds;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> JumpSounds;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> LandSounds;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TArray<TSoftObjectPtr<USoundBase>> CoinSounds;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> PotionSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> ChestSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> QuestSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> HeroAttackVoice;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> HeroPainVoice;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> HeroDeathVoice;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> EnemyDeathSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> BossRoarSound;
	UPROPERTY(Config, EditAnywhere, Category = "Sound") TSoftObjectPtr<USoundBase> AmbientLoop;

	// Plays a configured sound in the world with a little pitch variation so repeats don't sound identical.
	static void PlaySound(const UObject* Context, const TSoftObjectPtr<USoundBase>& Sound, const FVector& Location, float Volume = 1.f, float PitchJitter = 0.08f, float StartTime = 0.f);
	static void PlaySoundRandom(const UObject* Context, const TArray<TSoftObjectPtr<USoundBase>>& Sounds, const FVector& Location, float Volume = 1.f, float PitchJitter = 0.06f, float StartTime = 0.f);
	// a recorded strike plus a body thud underneath
	static void PlayImpact(const UObject* Context, const FVector& Location, float Volume);
	// Blood thrown onto the ground around a wound at Location, flung along Dir (spatter), or a pool
	// spreading under a body (bPool). Size in cm. Old stains are cleared so they never pile up.
	static void SpillBlood(const UObject* Context, const FVector& Location, const FVector& Dir, float Size, bool bPool = false);
	static void PlaySound2D(const UObject* Context, const TSoftObjectPtr<USoundBase>& Sound, float Volume = 1.f);

	const FHeroMove* FindMove(FName Name) const
	{
		return HeroMoves.FindByPredicate([Name](const FHeroMove& M) { return M.Name == Name; });
	}

	const FEnemyArchetype* FindEnemy(FName Name) const
	{
		return Enemies.FindByPredicate([Name](const FEnemyArchetype& A) { return A.Name == Name; });
	}

	static const UEldoriaSettings* Get() { return GetDefault<UEldoriaSettings>(); }

	// Plays a montage directly, or wraps a plain sequence in a dynamic montage on the given slot.
	// Returns the play length in seconds, already divided by the rate (0 if nothing played).
	static float PlayAnim(class USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim, float Rate, FName Slot, bool bHold = false, bool bReverse = false);

	// Turns a character body into a ragdoll that lies where it falls and never blocks pawns or the
	// camera. bKeepWeapon removes the weapon's own physics body so it stays in the fist (a knockdown);
	// a corpse lets go of it. Returns false when the mesh has no physics asset.
	static bool StartRagdoll(class USkeletalMeshComponent* Body, bool bKeepWeapon);
	// where a bone sits in component space at a moment of a clip, with the root at its reference pose
	static FVector ClipBoneLocation(const UAnimSequenceBase* Clip, FName Bone, float Time);
};
