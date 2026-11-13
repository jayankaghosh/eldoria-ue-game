#include "EldoriaMenu.h"
#include "EldoriaSplash.h"
#include "Engine/Font.h"
#include "HAL/FileManager.h"
#include "Widgets/Images/SImage.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "EldoriaMenu"

namespace
{
	const FLinearColor Gold(0.98f, 0.78f, 0.36f);
	const FLinearColor Ink(0.97f, 0.94f, 0.88f);
	const FLinearColor Muted(0.72f, 0.68f, 0.6f);

	FSlateFontInfo MenuFont(int32 Size, bool bBold = true)
	{
		static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		return FSlateFontInfo(Roboto, Size, bBold ? FName("Bold") : FName("Regular"));
	}

	FString FormatTime(float Seconds)
	{
		const int32 S = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%d:%02d:%02d"), S / 3600, (S / 60) % 60, S % 60);
	}
}

void SEldoriaMenu::Construct(const FArguments& InArgs)
{
	Game = InArgs._Game;
	Page = InArgs._Page;

	ButtonStyle = FButtonStyle()
		.SetNormal(FSlateColorBrush(FLinearColor(0.09f, 0.065f, 0.035f, 0.92f)))
		.SetHovered(FSlateColorBrush(FLinearColor(0.42f, 0.29f, 0.1f, 0.95f)))
		.SetPressed(FSlateColorBrush(FLinearColor(0.62f, 0.44f, 0.15f, 1.f)))
		.SetDisabled(FSlateColorBrush(FLinearColor(0.05f, 0.05f, 0.05f, 0.6f)))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));

	if (IFileManager::Get().FileExists(*SEldoriaSplash::File(TEXT("T_Logo_Eldoria.png"))))
	{
		LogoBrush = SEldoriaSplash::TitleBrush(900.f);
	}

	ShowPage(Page);
}

TSharedRef<SWidget> SEldoriaMenu::Title(const FString& Text, float Size, const FLinearColor& Color)
{
	return SNew(STextBlock)
		.Text(FText::FromString(Text))
		.Font(MenuFont(FMath::RoundToInt(Size)))
		.ColorAndOpacity(Color)
		.ShadowOffset(FVector2D(3.f, 3.f))
		.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
		.Justification(ETextJustify::Center);
}

TSharedRef<SWidget> SEldoriaMenu::Button(const FText& Label, TFunction<void()> OnClick, bool bEnabled, const FText& Detail)
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(Label).Font(MenuFont(28)).ColorAndOpacity(bEnabled ? Ink : Muted)
		];
	if (!Detail.IsEmpty())
	{
		Content->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(Detail).Font(MenuFont(17, false)).ColorAndOpacity(Muted).AutoWrapText(true).Justification(ETextJustify::Center)
		];
	}

	TSharedRef<SButton> Btn = SNew(SButton)
		.ButtonStyle(&ButtonStyle)
		.IsEnabled(bEnabled)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ContentPadding(FMargin(40.f, 14.f))
		.OnClicked_Lambda([this, OnClick]()
		{
			// run the action next frame: it may rebuild or close this menu, which must not happen mid-click
			TWeakPtr<SWidget> Self = AsShared();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self, OnClick](float)
			{
				if (Self.IsValid()) OnClick();
				return false;
			}));
			return FReply::Handled();
		})
		[
			Content
		];
	if (bEnabled && !FirstButton.IsValid()) FirstButton = Btn;

	return SNew(SBox).WidthOverride(560.f).Padding(FMargin(0.f, 7.f))
	[
		Btn
	];
}

TSharedRef<SWidget> SEldoriaMenu::BuildPage()
{
	FirstButton.Reset();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Add = [&Box](TSharedRef<SWidget> W, float Top = 0.f)
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, Top, 0.f, 0.f)[W];
	};
	AEldoriaGameMode* GM = Game.Get();

	switch (Page)
	{
	case EMenuPage::Main:
	{
		if (LogoBrush.IsValid())
		{
			Add(SNew(SImage).Image(LogoBrush.Get()));
		}
		else
		{
			Add(Title(TEXT("ELDORIA"), 110, Gold));
			Add(Title(TEXT("THE BONE KING"), 30, Ink), 4.f);
		}
		const FString Latest = AEldoriaGameMode::LatestSlot();
		Add(Button(LOCTEXT("Continue", "Continue"), [GM]() { if (GM) GM->ContinueGame(); }, !Latest.IsEmpty()), 30.f);
		Add(Button(LOCTEXT("NewGame", "New Game"), [this]() { ShowPage(EMenuPage::Difficulty); }));
		Add(Button(LOCTEXT("LoadGame", "Load Game"), [this]() { ReturnPage = EMenuPage::Main; ShowPage(EMenuPage::Load); }));
		Add(Button(FText::Format(LOCTEXT("GraphicsMain", "Graphics: {0}"), AEldoriaGameMode::QualityName(AEldoriaGameMode::GetQuality())), [this]()
		{
			AEldoriaGameMode::ApplyQuality((AEldoriaGameMode::GetQuality() + 1) % 3, true);
			ShowPage(EMenuPage::Main);
		}));
		Add(Button(LOCTEXT("Quit", "Quit"), [GM]() { if (GM) GM->QuitGame(); }));
		Add(Title(TEXT("Use the mouse, or the arrow keys and Enter"), 18, Muted), 40.f);
		break;
	}
	case EMenuPage::Pause:
	{
		Add(Title(TEXT("PAUSED"), 72, Gold));
		if (GM) Add(Title(FString::Printf(TEXT("Difficulty: %s"), *AEldoriaGameMode::DifficultyName(GM->GetDifficulty()).ToString()), 20, Muted), 4.f);
		Add(Button(LOCTEXT("Resume", "Resume"), [GM]() { if (GM) GM->CloseMenu(); }), 40.f);
		Add(Button(LOCTEXT("Save", "Save Game"), [this]() { ReturnPage = EMenuPage::Pause; ShowPage(EMenuPage::Save); }));
		Add(Button(LOCTEXT("Load", "Load Game"), [this]() { ReturnPage = EMenuPage::Pause; ShowPage(EMenuPage::Load); }));
		Add(Button(FText::Format(LOCTEXT("Graphics", "Graphics: {0}"), AEldoriaGameMode::QualityName(AEldoriaGameMode::GetQuality())), [this]()
		{
			AEldoriaGameMode::ApplyQuality((AEldoriaGameMode::GetQuality() + 1) % 3, true);
			ShowPage(EMenuPage::Pause);
		}));
		Add(Button(LOCTEXT("MainMenu", "Main Menu"), [GM]() { if (GM) GM->ReturnToMainMenu(); }));
		Add(Button(LOCTEXT("QuitGame", "Quit Game"), [GM]() { if (GM) GM->QuitGame(); }));
		Add(Title(TEXT("Esc to resume"), 18, Muted), 30.f);
		break;
	}
	case EMenuPage::Difficulty:
	{
		Add(Title(TEXT("CHOOSE DIFFICULTY"), 56, Gold));
		Add(Button(LOCTEXT("Easy", "Easy"), [GM]() { if (GM) GM->NewGame(EDifficulty::Easy); }, true,
			LOCTEXT("EasyDetail", "More health. Slower enemies who hit softly and attack one at a time.")), 36.f);
		// Medium is the one highlighted when the page opens
		FirstButton.Reset();
		Add(Button(LOCTEXT("Medium", "Medium"), [GM]() { if (GM) GM->NewGame(EDifficulty::Medium); }, true,
			LOCTEXT("MediumDetail", "The fight as intended.")));
		Add(Button(LOCTEXT("Hard", "Hard"), [GM]() { if (GM) GM->NewGame(EDifficulty::Hard); }, true,
			LOCTEXT("HardDetail", "Less health. Fast, heavy-hitting enemies who swing sooner and gang up.")));
		Add(Button(LOCTEXT("Back", "Back"), [this]() { Back(); }), 24.f);
		break;
	}
	case EMenuPage::Save:
	case EMenuPage::Load:
	{
		const bool bSaving = Page == EMenuPage::Save;
		Add(Title(bSaving ? TEXT("SAVE GAME") : TEXT("LOAD GAME"), 64, Gold));
		float Top = 36.f;
		for (const FSaveSlotInfo& Info : AEldoriaGameMode::ListSlots())
		{
			// the autosave can be loaded but not overwritten by hand
			if (bSaving && Info.Slot == AEldoriaGameMode::AutosaveSlot()) continue;
			const FText Detail = Info.bExists
				? FText::FromString(FString::Printf(TEXT("%s   -   played %s   -   %s"), *Info.QuestTitle, *FormatTime(Info.PlayTime), *Info.SavedAt.ToString(TEXT("%d %b %Y, %H:%M"))))
				: LOCTEXT("Empty", "Empty");
			const FString Slot = Info.Slot;
			const bool bEnabled = bSaving || Info.bExists;
			Add(Button(FText::FromString(Info.Label), [this, GM, Slot, bSaving]()
			{
				if (!GM) return;
				if (bSaving)
				{
					GM->SaveToSlot(Slot);
					ShowPage(EMenuPage::Save);
				}
				else
				{
					GM->LoadFromSlot(Slot);
				}
			}, bEnabled, Detail), Top);
			Top = 0.f;
		}
		Add(Button(LOCTEXT("Back", "Back"), [this]() { Back(); }), 24.f);
		break;
	}
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(&Backdrop)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(70.f, 50.f))
			[
				Box
			]
		];
}

void SEldoriaMenu::ShowPage(EMenuPage InPage)
{
	Page = InPage;
	ChildSlot[BuildPage()];
	FocusFirst();
}

void SEldoriaMenu::FocusFirst()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocus(FirstButton.IsValid() ? FirstButton : AsShared(), EFocusCause::SetDirectly);
	}
}

void SEldoriaMenu::Back()
{
	if (Page == EMenuPage::Save || Page == EMenuPage::Load)
	{
		ShowPage(ReturnPage);
	}
	else if (Page == EMenuPage::Difficulty)
	{
		ShowPage(EMenuPage::Main);
	}
	else if (Page == EMenuPage::Pause && Game.IsValid())
	{
		Game->CloseMenu();
	}
}

FReply SEldoriaMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		Back();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
