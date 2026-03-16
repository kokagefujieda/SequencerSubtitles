// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleTrack.h"
#include "SubtitleSection.h"
#include "SubtitleEvalTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneSubtitleTrack"

UMovieSceneSubtitleTrack::UMovieSceneSubtitleTrack()
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(80, 160, 240, 200);
#endif
}

bool UMovieSceneSubtitleTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

bool UMovieSceneSubtitleTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneSeqSubtitleSection::StaticClass();
}

UMovieSceneSection* UMovieSceneSubtitleTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSeqSubtitleSection>(this, NAME_None, RF_Transactional);
}

const TArray<UMovieSceneSection*>& UMovieSceneSubtitleTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneSubtitleTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UMovieSceneSubtitleTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

void UMovieSceneSubtitleTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

void UMovieSceneSubtitleTrack::RemoveSectionAt(int32 SectionIndex)
{
	Sections.RemoveAt(SectionIndex);
}

void UMovieSceneSubtitleTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

FMovieSceneEvalTemplatePtr UMovieSceneSubtitleTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneSeqSubtitleSection* SubtitleSection = Cast<const UMovieSceneSeqSubtitleSection>(&InSection);
	if (SubtitleSection)
	{
		return FSubtitleEvalTemplate(*SubtitleSection);
	}
	return FMovieSceneEvalTemplatePtr();
}

FText UMovieSceneSubtitleTrack::GetEffectiveSpeakerName() const
{
	if (bOverrideSpeakerName && !SpeakerNameOverride.IsEmptyOrWhitespace())
	{
		return SpeakerNameOverride;
	}
	// Fall back to the track's display name
#if WITH_EDITORONLY_DATA
	return GetDisplayName();
#else
	return FText::FromString(TEXT("Speaker"));
#endif
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneSubtitleTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Subtitles");
}
#endif

#undef LOCTEXT_NAMESPACE
