#include "EldoriaIntro.h"
#include "EldoriaGameMode.h"
#include "EldoriaSplash.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SEldoriaIntro::Construct(const FArguments& InArgs)
{
	Game = InArgs._Game;
	const FLinearColor Parchment(0.97f, 0.9f, 0.74f);

	ChildSlot
	[
		SNew(SOverlay)
		// letterbox: the story plays as a widescreen film
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SNew(SBox).HeightOverride(115.f)
			[
				SNew(SBorder).BorderImage(&Bar)
			]
		]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)
		[
			SNew(SBox).HeightOverride(115.f)
			[
				SNew(SBorder).BorderImage(&Bar)
			]
		]
		// the narration, set just above the lower bar
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(80.f, 0.f, 80.f, 140.f)
		[
			SNew(SBox).MaxDesiredWidth(1400.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return Game.IsValid() ? Game->IntroLine() : FText::GetEmpty(); })
				.Font(SEldoriaSplash::Font(30, 40))
				.ColorAndOpacity_Lambda([this, Parchment]()
				{
					return FSlateColor(FLinearColor(Parchment.R, Parchment.G, Parchment.B, Game.IsValid() ? Game->IntroLineAlpha() : 0.f));
				})
				.ShadowOffset(FVector2D(2.f, 2.f))
				.ShadowColorAndOpacity_Lambda([this]() { return FLinearColor(0.f, 0.f, 0.f, 0.9f * (Game.IsValid() ? Game->IntroLineAlpha() : 0.f)); })
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 50.f, 45.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("SPACE  -  SKIP")))
			.Font(SEldoriaSplash::Font(14, 200))
			.ColorAndOpacity(FLinearColor(0.75f, 0.68f, 0.55f, 0.55f))
		]
	];
}

FReply SEldoriaIntro::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Escape
		|| Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Gamepad_Special_Right)
	{
		if (Game.IsValid()) Game->FinishIntro();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
