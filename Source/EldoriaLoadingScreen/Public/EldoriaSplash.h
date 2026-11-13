#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateColorBrush.h"

// Loading screen. The first time (from engine startup) it plays the studio card, "Chandrabindu
// Studios presents", then "in association with Claude Code", then the ELDORIA title. Later loads (a new
// game, Continue, a save) show a scene from the game under a dark veil, with a line of lore or a
// fighting tip to read while the world loads. It runs on the
// loading thread before any game asset exists, so the logos and the Cinzel font are read as plain
// files from Content/Splash and the animation runs on wall-clock time.
class ELDORIALOADINGSCREEN_API SEldoriaSplash : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEldoriaSplash) {}
		SLATE_ARGUMENT(bool, bWithStudioCard)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// how long the full sequence needs on screen
	static float SequenceLength(bool bWithStudioCard) { return bWithStudioCard ? 10.f : 4.f; }

	// Content/Splash/<Name>
	static FString File(const TCHAR* Name);
	// the ELDORIA logo at the given width (shared with the main menu); read from disk by Slate itself
	static TSharedRef<FSlateDynamicImageBrush> TitleBrush(float Width);
	static FSlateFontInfo Font(float Size, int32 LetterSpacing = 0);

private:
	// the scene-and-story screen for later loads
	TSharedRef<SWidget> BuildLoadingScreen();
	TSharedPtr<FSlateDynamicImageBrush> SceneBrush;
	TSharedPtr<FSlateDynamicImageBrush> TitleScene;
	TSharedPtr<FSlateDynamicImageBrush> SmallTitle;
	FSlateColorBrush Veil = FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.42f));
	// seconds since the splash was first drawn
	float Elapsed() const;
	// 0..1 envelope: fades in from In, holds, fades out from Out (Out < 0 never fades out)
	float Envelope(float In, float Out, float Fade = 0.7f) const;

	mutable double StartTime = 0.0;
	bool bWithStudioCard = true;
	TSharedPtr<FSlateDynamicImageBrush> StudioBrush;
	TSharedPtr<FSlateDynamicImageBrush> ClaudeBrush;
	TSharedPtr<FSlateDynamicImageBrush> Title;
	FSlateColorBrush Black = FSlateColorBrush(FLinearColor::Black);
};
