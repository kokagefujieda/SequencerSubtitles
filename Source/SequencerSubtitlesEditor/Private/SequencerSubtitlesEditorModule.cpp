// Copyright 2026 kokage. All Rights Reserved.

#include "SequencerSubtitlesEditorModule.h"
#include "ISequencerModule.h"
#include "DialogueTrackEditor.h"

#define LOCTEXT_NAMESPACE "FSequencerSubtitlesEditorModule"

void FSequencerSubtitlesEditorModule::StartupModule()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	TrackEditorBindingHandle = SequencerModule.RegisterTrackEditor(
		FOnCreateTrackEditor::CreateStatic(&FDialogueTrackEditor::CreateTrackEditor));
}

void FSequencerSubtitlesEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.UnRegisterTrackEditor(TrackEditorBindingHandle);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSequencerSubtitlesEditorModule, SequencerSubtitlesEditor)
