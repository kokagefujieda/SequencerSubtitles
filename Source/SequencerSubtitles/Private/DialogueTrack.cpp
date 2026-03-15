// Copyright 2026 kokage. All Rights Reserved.

#include "DialogueTrack.h"
#include "DialogueSection.h"
#include "DialogueEvalTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneDialogueTrack"

UMovieSceneDialogueTrack::UMovieSceneDialogueTrack()
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(80, 160, 240, 200);
#endif
}

bool UMovieSceneDialogueTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

bool UMovieSceneDialogueTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneDialogueSection::StaticClass();
}

UMovieSceneSection* UMovieSceneDialogueTrack::CreateNewSection()
{
	return NewObject<UMovieSceneDialogueSection>(this, NAME_None, RF_Transactional);
}

const TArray<UMovieSceneSection*>& UMovieSceneDialogueTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneDialogueTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UMovieSceneDialogueTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}

void UMovieSceneDialogueTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

void UMovieSceneDialogueTrack::RemoveSectionAt(int32 SectionIndex)
{
	Sections.RemoveAt(SectionIndex);
}

void UMovieSceneDialogueTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

FMovieSceneEvalTemplatePtr UMovieSceneDialogueTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	const UMovieSceneDialogueSection* DialogueSection = Cast<const UMovieSceneDialogueSection>(&InSection);
	if (DialogueSection)
	{
		return FDialogueEvalTemplate(*DialogueSection);
	}
	return FMovieSceneEvalTemplatePtr();
}

FText UMovieSceneDialogueTrack::GetEffectiveSpeakerName() const
{
	if (bOverrideSpeakerName && !SpeakerNameOverride.IsEmptyOrWhitespace())
	{
		return SpeakerNameOverride;
	}
	// Fall back to the track's display name
	return GetDisplayName();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneDialogueTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Subtitles");
}
#endif

#undef LOCTEXT_NAMESPACE
