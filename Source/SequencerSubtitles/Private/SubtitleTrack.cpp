// Copyright 2026 kokage. All Rights Reserved.

#include "SubtitleTrack.h"
#include "SubtitleSection.h"
#include "SubtitleEvalTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"

#define LOCTEXT_NAMESPACE "MovieSceneSubtitleTrack"

UMovieSceneSubtitleTrack::UMovieSceneSubtitleTrack()
{
	// Required for legacy template evaluation to work in SubSequences
	EvalOptions.bCanEvaluateNearestSection = EvalOptions.bEvaluateNearestSection_DEPRECATED = true;

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

void UMovieSceneSubtitleTrack::PostCompile(FMovieSceneEvaluationTrack& Track, const FMovieSceneTrackCompilerArgs& Args) const
{
	Track.SetEvaluationPriority(0);
}

FText UMovieSceneSubtitleTrack::GetEffectiveSpeakerName() const
{
	if (bOverrideSpeakerName && !SpeakerNameOverride.IsEmptyOrWhitespace())
	{
		return SpeakerNameOverride;
	}

#if WITH_EDITORONLY_DATA
	FText TrackDisplayName = GetDisplayName();
	if (!TrackDisplayName.IsEmpty() && !TrackDisplayName.EqualTo(GetDefaultDisplayName()))
	{
		return TrackDisplayName;
	}
#endif

	return FText::GetEmpty();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneSubtitleTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Subtitle");
}

bool UMovieSceneSubtitleTrack::ValidateDisplayName(const FText& NewDisplayName, FText& OutErrorMessage) const
{
	// Allow empty name — it will revert to default "Subtitle" (no speaker displayed)
	if (NewDisplayName.IsEmpty())
	{
		return true;
	}
	return Super::ValidateDisplayName(NewDisplayName, OutErrorMessage);
}

FSlateColor UMovieSceneSubtitleTrack::GetLabelColor(const FMovieSceneLabelParams& LabelParams) const
{
	FText CurrentName = GetDisplayName();
	if (CurrentName.IsEmpty() || CurrentName.EqualTo(GetDefaultDisplayName()))
	{
		return FSlateColor::UseSubduedForeground();
	}
	return FSlateColor::UseForeground();
}
#endif

#undef LOCTEXT_NAMESPACE
