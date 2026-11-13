#include "EldoriaSplash.h"
#include "Modules/ModuleManager.h"
#include "MoviePlayer.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"

// Puts the splash up from engine startup and for every map load so the player never stares
// at a black window.
class FEldoriaLoadingScreenModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FEldoriaLoadingScreenModule::OnPreLoadMap);
		// the engine asks for a loading screen at the start of each map load, before our PreLoadMap
		// handler may run; answering here is what makes it show for Continue / Load / New Game
		if (IsMoviePlayerEnabled()) GetMoviePlayer()->OnPrepareLoadingScreen().AddRaw(this, &FEldoriaLoadingScreenModule::OnPrepare);
		// the engine starts playing this as soon as its window is up
		Show(true);
	}

	virtual void ShutdownModule() override
	{
		FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
		if (IsMoviePlayerEnabled()) GetMoviePlayer()->OnPrepareLoadingScreen().RemoveAll(this);
	}

	virtual bool IsGameModule() const override { return true; }

private:
	void OnPrepare()
	{
		// the startup splash is still running through the first map load: leave it be
		if (GetMoviePlayer()->IsMovieCurrentlyPlaying()) return;
		// later loads (a new game, Continue, a save) get the loading screen with a scene and a story line
		Show(false);
	}

	void OnPreLoadMap(const FString& MapName)
	{
		if (GIsEditor || IsRunningCommandlet() || !IsMoviePlayerEnabled()) return;
		if (GetMoviePlayer()->IsMovieCurrentlyPlaying()) return;
		// in case the engine checked before a screen was set up: set one up and start it now
		Show(false);
		GetMoviePlayer()->PlayMovie();
	}

	void Show(bool bFirst)
	{
		if (GIsEditor || IsRunningDedicatedServer() || IsRunningCommandlet() || !IsMoviePlayerEnabled()) return;
		FLoadingScreenAttributes Screen;
		Screen.WidgetLoadingScreen = SNew(SEldoriaSplash).bWithStudioCard(bFirst);
		Screen.MinimumLoadingScreenDisplayTime = SEldoriaSplash::SequenceLength(bFirst);
		Screen.bAutoCompleteWhenLoadingCompletes = true;
		Screen.bMoviesAreSkippable = true;
		Screen.bAllowInEarlyStartup = bFirst;
		// the world keeps ticking and streaming underneath while the sequence finishes
		Screen.bAllowEngineTick = true;
		GetMoviePlayer()->SetupLoadingScreen(Screen);
	}
};

IMPLEMENT_MODULE(FEldoriaLoadingScreenModule, EldoriaLoadingScreen);
