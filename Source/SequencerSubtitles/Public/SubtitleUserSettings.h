// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SubtitleUserSettings.generated.h"

/**
 * Player-facing subtitle options (accessibility), saved per user in GameUserSettings.ini.
 * Use the functions of USubtitleUserSettingsLibrary to change them from Blueprint.
 */
UCLASS(Config = GameUserSettings)
class SEQUENCERSUBTITLES_API USubtitleUserSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Show Sequencer subtitles. ShowMessage is not affected. */
	UPROPERTY(Config)
	bool bSubtitlesEnabled = true;

	/** Multiplier for subtitle and speaker name text size (0.5 - 3.0). */
	UPROPERTY(Config)
	float TextScale = 1.0f;

	/** Use BackgroundOpacity for the message window instead of the authored opacity. */
	UPROPERTY(Config)
	bool bOverrideBackgroundOpacity = false;

	/** Message window opacity chosen by the player (0 - 1). */
	UPROPERTY(Config)
	float BackgroundOpacity = 0.6f;

	float GetClampedTextScale() const { return FMath::Clamp(TextScale, 0.5f, 3.0f); }

	/** true when subtitles should be shown: this setting and the engine's subtitle switch are both on. */
	bool AreSubtitlesShown() const;

	/** Broadcast whenever a setting is changed through USubtitleUserSettingsLibrary. */
	static FSimpleMulticastDelegate& OnChanged();
};

/** Blueprint access to the player subtitle options. Call SaveSubtitleUserSettings to keep them. */
UCLASS()
class SEQUENCERSUBTITLES_API USubtitleUserSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Show or hide Sequencer subtitles. Also switches the engine's subtitle setting (GEngine->bSubtitlesEnabled). */
	UFUNCTION(BlueprintCallable, Category = "Subtitles|User Settings")
	static void SetSubtitlesEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Subtitles|User Settings")
	static bool AreSubtitlesEnabled();

	/** Text size multiplier (clamped to 0.5 - 3.0). Applied to subtitles already on screen too. */
	UFUNCTION(BlueprintCallable, Category = "Subtitles|User Settings")
	static void SetSubtitleTextScale(float Scale);

	UFUNCTION(BlueprintPure, Category = "Subtitles|User Settings")
	static float GetSubtitleTextScale();

	/**
	 * Message window opacity chosen by the player.
	 * bOverride = false uses the opacity set in each track / section.
	 */
	UFUNCTION(BlueprintCallable, Category = "Subtitles|User Settings")
	static void SetSubtitleBackgroundOpacity(bool bOverride, float Opacity);

	UFUNCTION(BlueprintPure, Category = "Subtitles|User Settings")
	static void GetSubtitleBackgroundOpacity(bool& bOverride, float& Opacity);

	/** Save the options to GameUserSettings.ini. */
	UFUNCTION(BlueprintCallable, Category = "Subtitles|User Settings")
	static void SaveSubtitleUserSettings();
};
