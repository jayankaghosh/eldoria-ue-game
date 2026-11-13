#include "EldoriaSplash.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Gold(0.98f, 0.78f, 0.36f);
	// pixel sizes of the PNGs made by Scripts/make_logo.swift
	const FVector2D TitlePixels(2400.f, 820.f);
	const FVector2D StudioPixels(1900.f, 900.f);
	const FVector2D ClaudePixels(1900.f, 900.f);
}

FString SEldoriaSplash::File(const TCHAR* Name)
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Splash") / Name);
}

TSharedRef<FSlateDynamicImageBrush> SEldoriaSplash::TitleBrush(float Width)
{
	return MakeShared<FSlateDynamicImageBrush>(FName(*File(TEXT("T_Logo_Eldoria.png"))), FVector2D(Width, Width * TitlePixels.Y / TitlePixels.X));
}

FSlateFontInfo SEldoriaSplash::Font(float Size, int32 LetterSpacing)
{
	FSlateFontInfo Info(File(TEXT("Cinzel.ttf")), Size);
	Info.LetterSpacing = LetterSpacing;
	return Info;
}

void SEldoriaSplash::Construct(const FArguments& InArgs)
{
	bWithStudioCard = InArgs._bWithStudioCard;
	if (!bWithStudioCard)
	{
		ChildSlot[BuildLoadingScreen()];
		return;
	}
	StudioBrush = MakeShared<FSlateDynamicImageBrush>(FName(*File(TEXT("T_Logo_Studio.png"))), FVector2D(760.f, 760.f * StudioPixels.Y / StudioPixels.X));
	ClaudeBrush = MakeShared<FSlateDynamicImageBrush>(FName(*File(TEXT("T_Logo_Claude.png"))), FVector2D(760.f, 760.f * ClaudePixels.Y / ClaudePixels.X));
	Title = TitleBrush(1100.f);
	if (IFileManager::Get().FileExists(*File(TEXT("Title_Scene.png"))))
	{
		TitleScene = MakeShared<FSlateDynamicImageBrush>(FName(*File(TEXT("Title_Scene.png"))), FVector2D(1920.f, 1080.f));
	}

	// studio card 0.4-3.6 s, the Claude Code card 3.8-7.2 s, then the title from 7.4 s;
	// without the cards the title comes up at once
	const float TitleIn = bWithStudioCard ? 7.4f : 0.f;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(&Black)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]() { return bWithStudioCard && Elapsed() < 3.8f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SImage).Image(StudioBrush.Get())
				.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Envelope(0.4f, 3.f)); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 30.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("PRESENTS")))
				.Font(Font(24, 600))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(0.88f, 0.82f, 0.7f, Envelope(1.3f, 3.f))); })
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SImage).Image(ClaudeBrush.Get())
			.Visibility_Lambda([this]() { const float T = Elapsed(); return bWithStudioCard && T > 3.6f && T < 7.4f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Envelope(3.8f, 6.6f)); })
		]
		// behind the title: a dramatic scene from the game under a dark veil, coming up with the title
		+ SOverlay::Slot()
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
			.Visibility_Lambda([this]() { return TitleScene.IsValid() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SImage).Image_Lambda([this]() { return TitleScene.IsValid() ? TitleScene.Get() : nullptr; })
				.ColorAndOpacity_Lambda([this, TitleIn]() { return FLinearColor(1.f, 1.f, 1.f, Envelope(TitleIn, -1.f, 1.6f)); })
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(&Veil)
			.Visibility_Lambda([this]() { return TitleScene.IsValid() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderBackgroundColor_Lambda([this, TitleIn]() { return FLinearColor(1.f, 1.f, 1.f, Envelope(TitleIn, -1.f, 1.6f)); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SImage).Image(Title.Get())
			.ColorAndOpacity_Lambda([this, TitleIn]() { return FLinearColor(1.f, 1.f, 1.f, Envelope(TitleIn, -1.f, 1.2f)); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 60.f, 50.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("LOADING")))
			.Font(Font(18, 300))
			.ColorAndOpacity_Lambda([this, TitleIn]()
			{
				// a slow pulse once the title is up
				const float Pulse = 0.55f + 0.45f * FMath::Sin(Elapsed() * 2.4f);
				return FSlateColor(FLinearColor(Gold.R, Gold.G, Gold.B, Envelope(TitleIn + 0.8f, -1.f) * Pulse));
			})
		]
	];
}

float SEldoriaSplash::Elapsed() const
{
	const double Now = FPlatformTime::Seconds();
	if (StartTime == 0.0) StartTime = Now;
	return float(Now - StartTime);
}

float SEldoriaSplash::Envelope(float In, float Out, float Fade) const
{
	const float T = Elapsed();
	float A = FMath::Clamp((T - In) / Fade, 0.f, 1.f);
	if (Out >= 0.f) A *= 1.f - FMath::Clamp((T - Out) / Fade, 0.f, 1.f);
	// ease so the fades feel like a film dissolve rather than a linear ramp
	return A * A * (3.f - 2.f * A);
}

TSharedRef<SWidget> SEldoriaSplash::BuildLoadingScreen()
{
	// lore from the story and the lessons of the fight, one per load
	static const TCHAR* Lines[] =
	{
		TEXT("For three hundred years the Royal Key kept the dark sealed away. Mordrak broke that seal to cheat death."),
		TEXT("The knights of the Silver Oath swore to stand while even one of them still breathed."),
		TEXT("Mordrak's ward cannot be broken by any blade. Only the Royal Key can open the way to him."),
		TEXT("The Sky Shrine floats above the eastern ruins, where the Bone King bound his soul to bone."),
		TEXT("Raise your guard just before a blow lands to parry it and leave your enemy open."),
		TEXT("Every blow wears down an enemy's guard. When it breaks he is dazed: press E to finish him."),
		TEXT("A red glint means a blow that cannot be blocked. Dodge it."),
		TEXT("Dodge at the very last instant and time itself slows for your counter."),
		TEXT("Heavy blows break a guard faster, and a big enough hit throws a foe to the ground."),
		TEXT("Chests hold coin and healing draughts. Search the camps and the ruins."),
	};
	const int32 Pick = FMath::RandRange(0, UE_ARRAY_COUNT(Lines) - 1);

	// a scene from the game, chosen at random
	TArray<FString> Scenes;
	IFileManager::Get().FindFiles(Scenes, *File(TEXT("Loading_*.png")), true, false);
	if (Scenes.Num() > 0)
	{
		const FString Scene = File(*Scenes[FMath::RandRange(0, Scenes.Num() - 1)]);
		SceneBrush = MakeShared<FSlateDynamicImageBrush>(FName(*Scene), FVector2D(1920.f, 1080.f));
	}
	SmallTitle = TitleBrush(520.f);
	TSharedRef<SOverlay> Root = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(&Black)
		];
	if (SceneBrush.IsValid())
	{
		Root->AddSlot()
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
			[
				SNew(SImage).Image(SceneBrush.Get())
				// the scene drifts in out of black
				.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Envelope(0.f, -1.f, 1.2f)); })
			]
		];
	}
	Root->AddSlot()
	[
		SNew(SBorder).BorderImage(&Veil)
	];
	Root->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(90.f, 0.f, 90.f, 110.f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SImage).Image(SmallTitle.Get())
			.ColorAndOpacity_Lambda([this]() { return FLinearColor(1.f, 1.f, 1.f, Envelope(0.3f, -1.f, 1.f)); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 26.f, 0.f, 0.f)
		[
			SNew(SBox).MaxDesiredWidth(1100.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Lines[Pick]))
				.Font(Font(30, 20))
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(0.97f, 0.92f, 0.8f, Envelope(0.8f, -1.f, 1.f))); })
				.ShadowOffset(FVector2D(2.f, 2.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			]
		]
	];
	Root->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 60.f, 50.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("LOADING")))
		.Font(Font(18, 300))
		.ColorAndOpacity_Lambda([this]()
		{
			const float Pulse = 0.55f + 0.45f * FMath::Sin(Elapsed() * 2.4f);
			return FSlateColor(FLinearColor(Gold.R, Gold.G, Gold.B, Envelope(0.8f, -1.f) * Pulse));
		})
	];
	return Root;
}
