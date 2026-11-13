#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Brushes/SlateColorBrush.h"

class AEldoriaGameMode;

// Drawn over the new-game story: cinematic letterbox bars, the narration as gold subtitles, and a skip
// hint. Space, Enter or Escape skips the story.
class SEldoriaIntro : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEldoriaIntro) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AEldoriaGameMode>, Game)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	TWeakObjectPtr<AEldoriaGameMode> Game;
	FSlateColorBrush Bar = FSlateColorBrush(FLinearColor::Black);
};
