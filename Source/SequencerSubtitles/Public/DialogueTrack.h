// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneNameableTrack.h"
#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#include "DialogueSettings.h"
#include "DialogueTrack.generated.h"

/** Sequencer track managing dialogue sections with per-track appearance settings. */
UCLASS(meta=(DisplayName="Subtitle Track"))
class SEQUENCERSUBTITLES_API UMovieSceneDialogueTrack
	: public UMovieSceneNameableTrack
	, public IMovieSceneTrackTemplateProducer
{
	GENERATED_BODY()

public:
	UMovieSceneDialogueTrack();

	/**
	 * Speaker name for this track. Defaults to the track display name.
	 * Enable bOverrideSpeakerName to set a custom name (supports localization).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker")
	bool bOverrideSpeakerName = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker",
		meta = (EditCondition = "bOverrideSpeakerName"))
	FText SpeakerNameOverride;

	/** Returns the effective speaker name (override → track display name). */
	FText GetEffectiveSpeakerName() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle Appearance",
		meta = (ShowOnlyInnerProperties))
	FDialogueAppearance Appearance;

	// UMovieSceneTrack interface
	virtual bool IsEmpty() const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual void RemoveAllAnimationData() override;
	virtual bool SupportsMultipleRows() const override { return true; }

	// IMovieSceneTrackTemplateProducer interface
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

private:
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> Sections;
};
