

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace TestUtils
{
/** RAII tool for setting up and tearing down a unit test that depends on the JUCE MessageManager system. */
struct SetupAndTeardown
{
	SetupAndTeardown();
	~SetupAndTeardown();
};
/** Returns a MouseDown event in which the target is the provided Component. */
[[nodiscard]] juce::MouseEvent mockMouseDownEvent(juce::Component* component) noexcept;
/** Returns a MouseUp event in which the target is the provided Component. */
[[nodiscard]] juce::MouseEvent mockMouseUpEvent(juce::Component* component) noexcept;
/** Return a blank UndoableAction. */
[[nodiscard]] std::unique_ptr<juce::UndoableAction> mockUndoableAction() noexcept;
/** Returns a playhead containing the provided CurrentPositionInfo. */
[[nodiscard]] std::unique_ptr<juce::AudioPlayHead> mockPlayhead(const juce::AudioPlayHead::CurrentPositionInfo& infoToReturnFromPlayhead) noexcept;
/** Shortcut to return a playhead that reports isPlaying. */
[[nodiscard]] std::unique_ptr<juce::AudioPlayHead> mockPlayhead_Playing() noexcept;

} // namespace TestUtils
