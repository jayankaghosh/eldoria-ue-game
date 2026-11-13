#include "EldoriaSettings.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "EldoriaAnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/DecalComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

float UEldoriaSettings::PlayAnim(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim, float Rate, FName Slot, bool bHold, bool bReverse)
{
	UAnimInstance* Instance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim || !Instance || Rate <= 0.f)
	{
		return 0.f;
	}
	if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(Instance))
	{
		// a montage is played as the clip inside it
		if (const UAnimMontage* Montage = Cast<UAnimMontage>(Anim))
		{
			if (Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
			{
				if (UAnimSequenceBase* Inner = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()) Anim = Inner;
			}
		}
		return Native->PlayAction(Anim, Rate, bHold, bReverse);
	}
	if (UAnimMontage* Montage = Cast<UAnimMontage>(Anim))
	{
		const float Length = Instance->Montage_Play(Montage, Rate);
		return Length > 0.f ? Length / Rate : 0.f;
	}
	if (UAnimMontage* Dynamic = Instance->PlaySlotAnimationAsDynamicMontage(Anim, Slot, bReverse ? 0.f : 0.1f, 0.2f, Rate))
	{
		if (bReverse)
		{
			// run it backwards from the last frame (standing up out of a fall)
			Instance->Montage_SetPosition(Dynamic, Dynamic->GetPlayLength() - 0.01f);
			Instance->Montage_SetPlayRate(Dynamic, -Rate);
		}
		return Anim->GetPlayLength() / Rate;
	}
	return 0.f;
}

void UEldoriaSettings::SpillBlood(const UObject* Context, const FVector& Location, const FVector& Dir, float Size, bool bPool)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	UMaterialInterface* Material = bPool ? Get()->BloodPoolDecal.LoadSynchronous() : Get()->BloodSpatterDecal.LoadSynchronous();
	if (!World || !Material) return;

	// where it lands: a little way along the blow, on whatever ground is below
	const FVector From = Location + Dir.GetSafeNormal2D() * (bPool ? 0.f : FMath::FRandRange(40.f, 140.f)) + FVector(0.f, 0.f, 60.f);
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("Blood"), false);
	Params.AddIgnoredActor(Cast<AActor>(Context));
	if (!World->LineTraceSingleByChannel(Hit, From, From - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params)) return;

	// projected straight down, spun at random, stretched a little along the direction it was flung
	const FRotator Rotation = FRotator(-90.f, bPool ? FMath::FRandRange(0.f, 360.f) : Dir.Rotation().Yaw, 0.f);
	const float Stretch = bPool ? 1.f : FMath::FRandRange(1.f, 1.5f);
	// the wet splash as it lands, then the stain it leaves
	if (UNiagaraSystem* Splash = bPool ? Get()->BloodPoolEffect.LoadSynchronous() : Get()->BloodSplashEffect.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Splash, Hit.ImpactPoint + FVector(0.f, 0.f, 1.f), FRotator(0.f, bPool ? FMath::FRandRange(0.f, 360.f) : Dir.Rotation().Yaw, 0.f), FVector(Size / 60.f));
	}
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, Material, FVector(60.f, Size * Stretch, Size), Hit.ImpactPoint, Rotation, 45.f);
	if (!Decal) return;
	Decal->SetFadeScreenSize(0.002f);

	static TArray<TWeakObjectPtr<UDecalComponent>> Recent;
	Recent.RemoveAll([](const TWeakObjectPtr<UDecalComponent>& D) { return !D.IsValid(); });
	Recent.Add(Decal);
	while (Recent.Num() > 48)
	{
		if (UDecalComponent* Old = Recent[0].Get()) Old->DestroyComponent();
		Recent.RemoveAt(0);
	}
}

bool UEldoriaSettings::StartRagdoll(USkeletalMeshComponent* Body, bool bKeepWeapon)
{
	if (!Body || !Body->GetPhysicsAsset()) return false;
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	if (bKeepWeapon)
	{
		// without its own physics body the weapon bone simply follows the hand
		TArray<FName> WeaponBones;
		for (const FBodyInstance* Instance : Body->Bodies)
		{
			const FName Bone = Instance && Instance->BodySetup.IsValid() ? Instance->BodySetup->BoneName : NAME_None;
			const FString Name = Bone.ToString().ToLower();
			if (Name.Contains(TEXT("weapon")) || Name.Contains(TEXT("sword")) || Name.Contains(TEXT("shield"))
				|| Name.Contains(TEXT("axe")) || Name.Contains(TEXT("club")) || Name.Contains(TEXT("blade")) || Name.Contains(TEXT("prop")))
			{
				WeaponBones.Add(Bone);
			}
		}
		for (const FName Bone : WeaponBones) Body->TermBodiesBelow(Bone);
		UE_LOG(LogTemp, Log, TEXT("Eldoria: %s ragdoll keeps %d weapon bone(s) in hand"), *Body->GetOwner()->GetName(), WeaponBones.Num());
		if (WeaponBones.Num() == 0)
		{
			FString Names;
			for (const FBodyInstance* Instance : Body->Bodies) if (Instance && Instance->BodySetup.IsValid()) Names += Instance->BodySetup->BoneName.ToString() + TEXT(" ");
			UE_LOG(LogTemp, Log, TEXT("Eldoria: ragdoll bodies: %s"), *Names);
		}
	}
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	return true;
}

FVector UEldoriaSettings::ClipBoneLocation(const UAnimSequenceBase* Clip, FName Bone, float Time)
{
	const UAnimSequence* Seq = Cast<UAnimSequence>(Clip);
	const USkeleton* Skeleton = Seq ? Seq->GetSkeleton() : nullptr;
	if (!Skeleton) return FVector::ZeroVector;
	const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
	const FAnimExtractContext Context(static_cast<double>(Time));
	FTransform Result = FTransform::Identity;
	for (int32 Index = Ref.FindBoneIndex(Bone); Index != INDEX_NONE; Index = Ref.GetParentIndex(Index))
	{
		FTransform Local = Ref.GetRefBonePose()[Index];
		if (Index > 0) Seq->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), Context, false);
		Result = Result * Local;
	}
	return Result.GetTranslation();
}

void UEldoriaSettings::PlaySound(const UObject* Context, const TSoftObjectPtr<USoundBase>& Sound, const FVector& Location, float Volume, float PitchJitter, float StartTime)
{
	if (USoundBase* S = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(Context, S, Location, Volume, FMath::FRandRange(1.f - PitchJitter, 1.f + PitchJitter), StartTime);
	}
}

void UEldoriaSettings::PlaySoundRandom(const UObject* Context, const TArray<TSoftObjectPtr<USoundBase>>& Sounds, const FVector& Location, float Volume, float PitchJitter, float StartTime)
{
	if (Sounds.Num() > 0)
	{
		PlaySound(Context, Sounds[FMath::RandRange(0, Sounds.Num() - 1)], Location, Volume, PitchJitter, StartTime);
	}
}

void UEldoriaSettings::PlayImpact(const UObject* Context, const FVector& Location, float Volume)
{
	const UEldoriaSettings* S = Get();
	PlaySoundRandom(Context, S->ImpactSounds, Location, Volume, 0.05f, S->ImpactStartTime);
	PlaySoundRandom(Context, S->ImpactBodySounds, Location, Volume * 0.55f, 0.08f);
}

void UEldoriaSettings::PlaySound2D(const UObject* Context, const TSoftObjectPtr<USoundBase>& Sound, float Volume)
{
	if (USoundBase* S = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(Context, S, Volume);
	}
}
