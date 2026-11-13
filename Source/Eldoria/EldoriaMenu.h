#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "EldoriaGameMode.h"

// Full-screen menu drawn with Slate: main menu, pause menu and the save/load slot pages.
// Works with the mouse, arrow keys + Enter, and a gamepad; Escape goes back.
class SEldoriaMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEldoriaMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AEldoriaGameMode>, Game)
		SLATE_ARGUMENT(EMenuPage, Page)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void ShowPage(EMenuPage InPage);
	void FocusFirst();

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	TSharedRef<SWidget> BuildPage();
	TSharedRef<SWidget> Button(const FText& Label, TFunction<void()> OnClick, bool bEnabled = true, const FText& Detail = FText::GetEmpty());
	TSharedRef<SWidget> Title(const FString& Text, float Size, const FLinearColor& Color);
	void Back();

	TWeakObjectPtr<AEldoriaGameMode> Game;
	EMenuPage Page = EMenuPage::Main;
	EMenuPage ReturnPage = EMenuPage::Main;
	TSharedPtr<SWidget> FirstButton;

	FButtonStyle ButtonStyle;
	TSharedPtr<FSlateBrush> LogoBrush;
	FSlateColorBrush Backdrop = FSlateColorBrush(FLinearColor(0.01f, 0.008f, 0.005f, 0.72f));
	FSlateColorBrush CardBrush = FSlateColorBrush(FLinearColor(0.04f, 0.03f, 0.02f, 0.88f));
};
