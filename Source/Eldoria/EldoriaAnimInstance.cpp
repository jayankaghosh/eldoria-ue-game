#include "EldoriaAnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr float ActionBlendIn = 0.12f;
	constexpr float ActionBlendOut = 0.2f;

	void Sample(UAnimSequenceBase* Anim, float Time, bool bLoop, FPoseContext& Into)
	{
		FAnimationPoseData Data(Into);
		Anim->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false, FDeltaTimeRecord(), bLoop));
	}
}

void UEldoriaAnimInstance::Setup(UAnimSequenceBase* InIdle, UAnimSequenceBase* InMove, float InMoveSpeed)
{
	IdleAnim = InIdle;
	MoveAnim = InMove;
	MoveSpeed = FMath::Max(50.f, InMoveSpeed);
}

void UEldoriaAnimInstance::SetupWalkAndFall(UAnimSequenceBase* InWalk, float InWalkSpeed, UAnimSequenceBase* InFall)
{
	WalkAnim = InWalk;
	WalkSpeed = FMath::Max(50.f, InWalkSpeed);
	FallAnim = InFall;
}

float UEldoriaAnimInstance::PlayAction(UAnimSequenceBase* Anim, float Rate, bool bInHold, bool bInReverse, float StartTime)
{
	if (!Anim || Rate <= 0.f) return 0.f;
	const float Length = Anim->GetPlayLength();
	ActionAnim = Anim;
	bReverse = bInReverse;
	ActionTime = StartTime >= 0.f ? FMath::Min(StartTime, Length) : (bReverse ? Length : 0.f);
	bActionUpperBody = false;
	ActionRate = Rate;
	bActionActive = true;
	bHold = bInHold;
	return (bReverse ? ActionTime : Length - ActionTime) / Rate;
}

void UEldoriaAnimInstance::BlendFromPose(const TArray<FTransform>& LocalPose, float Seconds)
{
	FrozenPose = LocalPose;
	FrozenWeight = 1.f;
	FrozenBlendTime = FMath::Max(0.05f, Seconds);
	// the action underneath should be fully in place, not fading up from locomotion
	ActionWeight = 1.f;
}

void UEldoriaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (FrozenWeight > 0.f)
	{
		FrozenWeight = FMath::Max(0.f, FrozenWeight - DeltaSeconds / FrozenBlendTime);
	}

	const APawn* Owner = TryGetPawnOwner();
	const float Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;

	// each gait clip plays at the rate its feet cover the ground at this speed, so they don't slide
	IdleTime += DeltaSeconds;
	if (IdleAnim) IdleTime = FMath::Fmod(IdleTime, FMath::Max(0.01f, IdleAnim->GetPlayLength()));
	if (WalkAnim)
	{
		// idle -> walk up to walking pace, then walk -> run
		const float WantMove = FMath::Clamp(Speed / (WalkSpeed * 0.6f), 0.f, 1.f);
		const float WantRun = FMath::Clamp((Speed - WalkSpeed) / FMath::Max(1.f, MoveSpeed - WalkSpeed), 0.f, 1.f);
		MoveWeight = FMath::FInterpTo(MoveWeight, WantMove, DeltaSeconds, 10.f);
		RunBlend = FMath::FInterpTo(RunBlend, WantRun, DeltaSeconds, 6.f);
		WalkTime = FMath::Fmod(WalkTime + DeltaSeconds * FMath::Clamp(Speed / WalkSpeed, 0.6f, 1.5f), FMath::Max(0.01f, WalkAnim->GetPlayLength()));
	}
	else
	{
		const float WantMove = FMath::Clamp(Speed / (MoveSpeed * 0.35f), 0.f, 1.f);
		MoveWeight = FMath::FInterpTo(MoveWeight, WantMove, DeltaSeconds, 10.f);
		RunBlend = 1.f;
	}
	MoveTime += DeltaSeconds * FMath::Clamp(Speed / MoveSpeed, 0.6f, 1.6f);
	if (MoveAnim) MoveTime = FMath::Fmod(MoveTime, FMath::Max(0.01f, MoveAnim->GetPlayLength()));

	// in the air: the falling loop takes over the legs and body
	const ACharacter* Character = Cast<const ACharacter>(Owner);
	const bool bFalling = FallAnim && Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling();
	AirWeight = FMath::FInterpTo(AirWeight, bFalling ? 1.f : 0.f, DeltaSeconds, 8.f);
	if (FallAnim) FallTime = FMath::Fmod(FallTime + DeltaSeconds, FMath::Max(0.01f, FallAnim->GetPlayLength()));

	if (bActionActive && ActionAnim)
	{
		const float Length = ActionAnim->GetPlayLength();
		ActionTime += DeltaSeconds * ActionRate * (bReverse ? -1.f : 1.f);
		const bool bFinished = bReverse ? ActionTime <= 0.f : ActionTime >= Length;
		if (bFinished)
		{
			ActionTime = FMath::Clamp(ActionTime, 0.f, Length);
			if (!bHold) bActionActive = false;
		}
		ActionWeight = FMath::Min(1.f, ActionWeight + DeltaSeconds / ActionBlendIn);
	}
	else
	{
		ActionWeight = FMath::Max(0.f, ActionWeight - DeltaSeconds / ActionBlendOut);
	}
}

void FEldoriaAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UEldoriaAnimInstance* I = CastChecked<UEldoriaAnimInstance>(InAnimInstance);
	Idle = I->IdleAnim;
	Move = I->MoveAnim;
	Action = I->ActionAnim;
	IdleTime = I->IdleTime;
	MoveTime = I->MoveTime;
	ActionTime = I->ActionTime;
	MoveWeight = I->MoveWeight;
	ActionWeight = I->ActionWeight;
	Walk = I->WalkAnim;
	Fall = I->FallAnim;
	WalkTime = I->WalkTime;
	FallTime = I->FallTime;
	RunBlend = I->RunBlend;
	AirWeight = I->AirWeight;
	bActionUpperBody = I->bActionUpperBody;
	FrozenWeight = I->FrozenWeight;
	if (FrozenWeight > 0.f) Frozen = I->FrozenPose;
	else Frozen.Reset();
}

bool FEldoriaAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!Idle && !Move)
	{
		return false;
	}

	// Blends A and B into Output with the engine's standard (shortest-arc) blend, then normalises
	// rotations so a mid-blend pose can never collapse.
	auto Blend = [this, &Output](FPoseContext& A, FPoseContext& B, float WeightOfA)
	{
		FPoseContext Result(this);
		FAnimationPoseData Out(Result);
		FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(A), FAnimationPoseData(B), WeightOfA, Out);
		Result.Pose.NormalizeRotations();
		Output.Pose.CopyBonesFrom(Result.Pose);
		Output.Curve.CopyFrom(Result.Curve);
		Output.CustomAttributes.CopyFrom(Result.CustomAttributes);
	};

	// locomotion: idle blended toward walk and run by speed
	if (Walk && Move)
	{
		// the moving gait: walk, run, or a blend of the two
		FPoseContext Gait(this);
		if (RunBlend <= 0.001f) Sample(Walk, WalkTime, true, Gait);
		else if (RunBlend >= 0.999f) Sample(Move, MoveTime, true, Gait);
		else
		{
			FPoseContext WalkPose(this);
			FPoseContext RunPose(this);
			Sample(Walk, WalkTime, true, WalkPose);
			Sample(Move, MoveTime, true, RunPose);
			Blend(WalkPose, RunPose, 1.f - RunBlend);
			Gait.Pose.CopyBonesFrom(Output.Pose);
			Gait.Curve.CopyFrom(Output.Curve);
		}
		if (Idle && MoveWeight < 0.999f)
		{
			FPoseContext IdlePose(this);
			Sample(Idle, IdleTime, true, IdlePose);
			if (MoveWeight <= 0.001f)
			{
				Output.Pose.CopyBonesFrom(IdlePose.Pose);
				Output.Curve.CopyFrom(IdlePose.Curve);
			}
			else Blend(IdlePose, Gait, 1.f - MoveWeight);
		}
		else
		{
			Output.Pose.CopyBonesFrom(Gait.Pose);
			Output.Curve.CopyFrom(Gait.Curve);
		}
	}
	else if (Idle && Move && MoveWeight > 0.001f && MoveWeight < 0.999f)
	{
		FPoseContext IdlePose(this);
		FPoseContext RunPose(this);
		Sample(Idle, IdleTime, true, IdlePose);
		Sample(Move, MoveTime, true, RunPose);
		Blend(IdlePose, RunPose, 1.f - MoveWeight);
	}
	else if (Move && (MoveWeight >= 0.999f || !Idle))
	{
		Sample(Move, MoveTime, true, Output);
	}
	else
	{
		Sample(Idle, IdleTime, true, Output);
	}

	// airborne: the falling loop
	if (Fall && AirWeight > 0.001f)
	{
		FPoseContext FallPose(this);
		Sample(Fall, FallTime, true, FallPose);
		FPoseContext Ground(this);
		Ground.Pose.CopyBonesFrom(Output.Pose);
		Ground.Curve.CopyFrom(Output.Curve);
		Blend(Ground, FallPose, 1.f - AirWeight);
	}

	// one-shot action layered over the full body
	if (Action && ActionWeight > 0.001f)
	{
		if (Action->IsValidAdditive())
		{
			// Hit reactions ship as additive clips: offsets to lay on top of whatever the body is doing.
			// Treated as a full pose they would crush every bone to the origin and the body would vanish.
			FPoseContext Additive(this, true);
			Sample(Action, ActionTime, false, Additive);
			FAnimationPoseData Base(Output);
			FAnimationRuntime::AccumulateAdditivePose(Base, FAnimationPoseData(Additive), ActionWeight, Action->GetAdditiveAnimType());
			Output.Pose.NormalizeRotations();
		}
		else
		{
			FPoseContext ActionPose(this);
			Sample(Action, ActionTime, false, ActionPose);
			if (bActionUpperBody)
			{
				// from spine_01 up the action leads; pelvis and legs stay with the walk underneath
				const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
				const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
				const int32 Spine = Ref.FindBoneIndex(TEXT("spine_01"));
				FPoseContext Loco(this);
				Loco.Pose.CopyBonesFrom(Output.Pose);
				for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
				{
					const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
					bool bUpper = false;
					for (int32 B = MeshIndex; B != INDEX_NONE && Spine != INDEX_NONE; B = Ref.GetParentIndex(B))
					{
						if (B == Spine) { bUpper = true; break; }
					}
					if (!bUpper) continue;
					FTransform Mixed;
					Mixed.Blend(Loco.Pose[Index], ActionPose.Pose[Index], ActionWeight);
					Output.Pose[Index] = Mixed;
				}
				Output.Pose.NormalizeRotations();
			}
			else if (ActionWeight >= 0.999f)
			{
				Output.Pose.CopyBonesFrom(ActionPose.Pose);
				Output.Curve.CopyFrom(ActionPose.Curve);
				Output.CustomAttributes.CopyFrom(ActionPose.CustomAttributes);
			}
			else
			{
				FPoseContext Loco(this);
				Loco.Pose.CopyBonesFrom(Output.Pose);
				Loco.Curve.CopyFrom(Output.Curve);
				Loco.CustomAttributes.CopyFrom(Output.CustomAttributes);
				Blend(Loco, ActionPose, 1.f - ActionWeight);
			}
		}
	}

	// fade out of a captured pose (a ragdoll at rest) into whatever the animation is doing
	if (FrozenWeight > 0.001f && Frozen.Num() > 0)
	{
		FPoseContext FrozenPose(this);
		FrozenPose.Pose.CopyBonesFrom(Output.Pose);
		FrozenPose.Curve.CopyFrom(Output.Curve);
		const FBoneContainer& Bones = FrozenPose.Pose.GetBoneContainer();
		for (const FCompactPoseBoneIndex Index : FrozenPose.Pose.ForEachBoneIndex())
		{
			const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
			if (Frozen.IsValidIndex(MeshIndex)) FrozenPose.Pose[Index] = Frozen[MeshIndex];
		}
		FPoseContext Anim(this);
		Anim.Pose.CopyBonesFrom(Output.Pose);
		Anim.Curve.CopyFrom(Output.Curve);
		Anim.CustomAttributes.CopyFrom(Output.CustomAttributes);
		const float W = FrozenWeight * FrozenWeight * (3.f - 2.f * FrozenWeight);
		Blend(FrozenPose, Anim, W);
	}

	// Paragon clips carry root motion (lunges, knock-ups). These characters are moved by their capsule,
	// so keep the root bone at its reference position; otherwise the body is drawn metres away from
	// where the enemy really is and snaps back when the clip ends.
	if (Output.Pose.GetNumBones() > 0)
	{
		const FCompactPoseBoneIndex Root(0);
		Output.Pose[Root].SetTranslation(Output.Pose.GetRefPose(Root).GetTranslation());
	}
	return true;
}
