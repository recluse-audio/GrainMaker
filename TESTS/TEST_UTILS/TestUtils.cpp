

#include "TestUtils.h"

static juce::MouseEvent mockMouseEvent(juce::Component* component,
									   juce::ModifierKeys modifiers,
									   bool mouseWasDragged = false) noexcept
{
	float pressure = 0.0f;
	float orientation = 0.0f;
	float rotation = 0.0f;
	float tiltX = 0.0f;
	float tiltY = 0.0f;
	juce::Time eventTime = juce::Time::getCurrentTime();
	juce::Point<float> mouseDownPos;
	juce::Time mouseDownTime;
	int numberOfClicks = 1;
	
	auto mouse = juce::Desktop::getInstance().getMainMouseSource();
	
	juce::MouseEvent event(mouse,
						   juce::Point<float>(),
						   modifiers,
						   pressure,
						   orientation,
						   rotation,
						   tiltX,
						   tiltY,
						   component, // eventComponent
						   nullptr,   // originator
						   eventTime,
						   mouseDownPos,
						   mouseDownTime,
						   numberOfClicks,
						   mouseWasDragged);
	
	return event;
}

namespace TestUtils
{

SetupAndTeardown::SetupAndTeardown()
{
	// Required so that JUCE knows what the 'message' thread is
	juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();
}

SetupAndTeardown::~SetupAndTeardown()
{
	// deletes shared TooltipWindow
	juce::DeletedAtShutdown::deleteAll();
	// must be last
	juce::MessageManager::deleteInstance();
}

juce::MouseEvent mockMouseDownEvent(juce::Component* component) noexcept
{
	juce::ModifierKeys modifiers(juce::ModifierKeys::leftButtonModifier); // left click
	return mockMouseEvent(component, modifiers);
}

juce::MouseEvent mockMouseUpEvent(juce::Component* component) noexcept
{
	juce::ModifierKeys modifiers; // no modifiers, mouseUp
	return mockMouseEvent(component, modifiers);
}

std::unique_ptr<juce::UndoableAction> mockUndoableAction() noexcept
{
	struct DummyUndoableAction : public juce::UndoableAction
	{
		bool perform() override { return true; }
		bool undo() override { return true; }
	};
	auto action = std::make_unique<DummyUndoableAction>();
	
	return action;
}

std::unique_ptr<juce::AudioPlayHead> mockPlayhead(const juce::AudioPlayHead::CurrentPositionInfo& infoToReturnFromPlayhead) noexcept
{
	(void) infoToReturnFromPlayhead;
#if (JUCE_MAJOR_VERSION < 7 )
	class MockPlayhead : public juce::AudioPlayHead
	{
	public:
		explicit MockPlayhead(const CurrentPositionInfo& infoToReturnFromPlayhead) : info(infoToReturnFromPlayhead) {}
		
		bool getCurrentPosition (CurrentPositionInfo& result) override
		{
			result = info;
			return true;
		}
		
	private:
		CurrentPositionInfo info;
	};
	
	return std::make_unique<MockPlayhead>(infoToReturnFromPlayhead);
	
#else
	class MockPlayhead : public juce::AudioPlayHead
	{
	public:
		juce::Optional<PositionInfo> getPosition() const override
		{
			return info;
		}
		
	private:
		juce::Optional<PositionInfo> info;
	};
	
	return std::make_unique<MockPlayhead>();
#endif
}

std::unique_ptr<juce::AudioPlayHead> mockPlayhead_Playing() noexcept
{
	class MockPlayhead_Playing : public juce::AudioPlayHead
	{
	public:
		MockPlayhead_Playing() = default;
		
#if (JUCE_MAJOR_VERSION < 7 )
		bool getCurrentPosition (CurrentPositionInfo& result) override
		{
			CurrentPositionInfo info;
			result = info; // reset to default;
			result.isPlaying = true;
			return true;
		}
#else
		juce::Optional<PositionInfo> getPosition() const override
		{
			PositionInfo info;
			info.setIsPlaying(true);
			return info;
		}
#endif
	};
	
	return std::make_unique<MockPlayhead_Playing>();
}

} // namespace UnitTestUtils

