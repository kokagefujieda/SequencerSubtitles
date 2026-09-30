// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleUserSettings.h"
#include "Engine/Engine.h"

bool USubtitleUserSettings::AreSubtitlesShown() const
{
	return bSubtitlesEnabled && (!GEngine || GEngine->bSubtitlesEnabled);
}

FSimpleMulticastDelegate& USubtitleUserSettings::OnChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

void USubtitleUserSettingsLibrary::SetSubtitlesEnabled(bool bEnabled)
{
	GetMutableDefault<USubtitleUserSettings>()->bSubtitlesEnabled = bEnabled;

	// One switch for all subtitles: keep the engine's setting in sync
	if (GEngine)
	{
		GEngine->bSubtitlesEnabled = bEnabled;
	}
	USubtitleUserSettings::OnChanged().Broadcast();
}

bool USubtitleUserSettingsLibrary::AreSubtitlesEnabled()
{
	return GetDefault<USubtitleUserSettings>()->AreSubtitlesShown();
}

void USubtitleUserSettingsLibrary::SetSubtitleTextScale(float Scale)
{
	GetMutableDefault<USubtitleUserSettings>()->TextScale = FMath::Clamp(Scale, 0.5f, 3.0f);
	USubtitleUserSettings::OnChanged().Broadcast();
}

float USubtitleUserSettingsLibrary::GetSubtitleTextScale()
{
	return GetDefault<USubtitleUserSettings>()->GetClampedTextScale();
}

void USubtitleUserSettingsLibrary::SetSubtitleBackgroundOpacity(bool bOverride, float Opacity)
{
	USubtitleUserSettings* Settings = GetMutableDefault<USubtitleUserSettings>();
	Settings->bOverrideBackgroundOpacity = bOverride;
	Settings->BackgroundOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	USubtitleUserSettings::OnChanged().Broadcast();
}

void USubtitleUserSettingsLibrary::GetSubtitleBackgroundOpacity(bool& bOverride, float& Opacity)
{
	const USubtitleUserSettings* Settings = GetDefault<USubtitleUserSettings>();
	bOverride = Settings->bOverrideBackgroundOpacity;
	Opacity   = Settings->BackgroundOpacity;
}

void USubtitleUserSettingsLibrary::SaveSubtitleUserSettings()
{
	GetMutableDefault<USubtitleUserSettings>()->SaveConfig();
}
