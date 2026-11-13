#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EldoriaHUD.generated.h"

// Canvas HUD: hero panel, quest card, objective waypoint, enemy and boss bars, combat feedback,
// prompts and a controls card. Sized for readability at a distance.
UCLASS()
class ELDORIA_API AEldoriaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// floating damage number above a character
	static void AddNumber(const UObject* Context, const FVector& Location, float Amount, const FLinearColor& Color);

	// hides the controls card if it is on screen (including the automatic intro showing), otherwise shows it
	void ToggleHelp();

private:
	// Text in points (scaled with the screen). Returns the drawn size.
	FVector2D Text(const FString& Str, float X, float Y, float Size, const FLinearColor& Color, bool bCenter = false, bool bBold = true);
	FVector2D Measure(const FString& Str, float Size, bool bBold = true);
	void Panel(float X, float Y, float W, float H, float Alpha = 0.72f);
	void Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& Fill, float LagPct = -1.f);
	void Coin(float X, float Y, float R);
	TArray<FString> Wrap(const FString& Str, float Size, float MaxWidth);

	void DrawHeroPanel(class AEldoriaCharacter* Hero);
	void DrawQuestCard(class AEldoriaGameMode* GM);
	void DrawObjective();
	void DrawEnemyBars();
	void DrawPopups(float Now);
	void DrawGlint(class AEnemyCharacter* E, const FVector& Screen);
	void DrawHelp(float Alpha);
	// blood splattered over the screen edges: fresh on a hit, lingering faintly while badly hurt
	void DrawBloodOverlay(float Amount);
	UPROPERTY() class UMaterialInstanceDynamic* BloodMaterial = nullptr;

	struct FPopup
	{
		FVector Location;
		FString Text;
		FLinearColor Color;
		float Born;
	};
	TArray<FPopup> Popups;

	float Ui = 1.f;
	float ShownHealth = 1.f;
	bool bShowHelp = false;
	bool bIntroHelpDismissed = false;
	bool bHelpVisible = false;
};
