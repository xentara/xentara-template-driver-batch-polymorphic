// Copyright (c) embedded ocean GmbH
#include "TemplateIoTransaction.hpp"

#include "Attributes.hpp"
#include "Tasks.hpp"
#include "TemplateInput.hpp"
#include "TemplateOutput.hpp"
#include "WriteCommand.hpp"

#include <xentara/config/Context.hpp>
#include <xentara/config/Errors.hpp>
#include <xentara/data/ReadHandle.hpp>
#include <xentara/data/WriteHandle.hpp>
#include <xentara/memory/memoryResources.hpp>
#include <xentara/memory/WriteSentinel.hpp>
#include <xentara/model/Attribute.hpp>
#include <xentara/model/ForEachAttributeFunction.hpp>
#include <xentara/model/ForEachEventFunction.hpp>
#include <xentara/model/ForEachTaskFunction.hpp>
#include <xentara/process/ExecutionContext.hpp>
#include <xentara/utils/json/decoder/Object.hpp>
#include <xentara/utils/json/decoder/Errors.hpp>
#include <xentara/utils/eh/currentErrorCode.hpp>

namespace xentara::plugins::templateDriver
{

using namespace std::literals;

template <typename Buffer>
class TemplateIoTransaction::RuntimeBufferSentinel final
{
public:
	/// @brief The constructor clears the buffer of any garbage data that may still be in there
	RuntimeBufferSentinel(Buffer &buffer) : _buffer(buffer)
	{
		_buffer.clear();
	}

	/// @brief The destructor clears the buffer again
	~RuntimeBufferSentinel()
	{
		_buffer.clear();
	}

private:
	/// @brief The buffer to protect
	Buffer &_buffer;
};

auto TemplateIoTransaction::load(utils::json::decoder::Object &jsonObject, config::Context &context) -> void
{
	// Go through all the members of the JSON object that represents this object
	for (auto && [name, value] : jsonObject)
    {
		/// @todo load configuration parameters
		if (name == "TODO"sv)
		{
			/// @todo parse the value correctly
			auto todo = value.asNumber<std::uint64_t>();

			/// @todo check that the value is valid
			if (!"TODO")
			{
				/// @todo use an error message that tells the user exactly what is wrong
				utils::json::decoder::throwWithLocation(value, std::runtime_error("TODO is wrong with TODO parameter of template I/O transaction"));
			}

			/// @todo set the appropriate member variables
		}
		else
		{
            config::throwUnknownParameterError(name);
		}
    }

	/// @todo perform consistency and completeness checks
	if (!"TODO")
	{
		/// @todo use an error message that tells the user exactly what is wrong
		utils::json::decoder::throwWithLocation(jsonObject, std::runtime_error("TODO is wrong with template I/O transaction"));
	}
}

auto TemplateIoTransaction::addInput(std::reference_wrapper<AbstractInput> input) -> void
{
	// Make sure we belong to the same I/O component
	if (&input.get().ioComponent() != &_ioComponent.get())
	{
		/// @todo replace "template data point", "I/O transaction", and "I/O component" with more descriptive names
		throw std::runtime_error("Attempt to attach template data point to I/O transaction of different I/O component");
	}

	// Add it
	_inputs.push_back(input);
}

auto TemplateIoTransaction::addOutput(std::reference_wrapper<AbstractOutput> output) -> void
{
	// Make sure we belong to the same I/O component
	if (&output.get().ioComponent() != &_ioComponent.get())
	{
		/// @todo replace "template data point", "I/O transaction", and "I/O component" with more descriptive names
		throw std::runtime_error("Attempt to attach template data point to I/O transaction of different I/O component");
	}

	// Add it
	_outputs.push_back(output);
}

auto TemplateIoTransaction::forEachAttribute(const model::ForEachAttributeFunction &function) const -> bool
{
	return
		// Handle the read state attributes
		_readState.forEachAttribute(function) ||
		// Handle the write state attributes
		_writeState.forEachAttribute(function);

	/// @todo handle any additional attributes this class supports, including attributes inherited from the I/O component
}

auto TemplateIoTransaction::forEachEvent(const model::ForEachEventFunction &function) -> bool
{
	return
		// Handle the read state events
		_readState.forEachEvent(function, sharedFromThis()) ||
		// Handle the write state events
		_writeState.forEachEvent(function, sharedFromThis());

	/// @todo handle any additional events this class supports, including events inherited from the I/O component
}

auto TemplateIoTransaction::forEachTask(const model::ForEachTaskFunction &function) -> bool
{
	// Handle all the tasks we support
	return
		function(tasks::kRead, sharedFromThis(&_readTask)) ||
		function(tasks::kWrite, sharedFromThis(&_writeTask));

	/// @todo handle any additional tasks this class supports
}

auto TemplateIoTransaction::makeReadHandle(const model::Attribute &attribute) const noexcept -> std::optional<data::ReadHandle>
{
	// Handle the read state attributes
	if (auto handle = _readState.makeReadHandle(_readDataBlock, attribute))
	{
		return handle;
	}
	// Handle the write state attributes
	if (auto handle = _writeState.makeReadHandle(_writeDataBlock, attribute))
	{
		return handle;
	}

	/// @todo handle any additional readable attributes this class supports, including attributes inherited from the I/O component

	return std::nullopt;
}

auto TemplateIoTransaction::forEachReadStateAttribute(const model::ForEachAttributeFunction &function) const -> bool
{
	return _readState.forEachAttribute(function);
}

auto TemplateIoTransaction::forEachReadStateEvent(const model::ForEachEventFunction &function) -> bool
{
	return _readState.forEachEvent(function, sharedFromThis());
}

auto TemplateIoTransaction::makeReadStateReadHandle(const model::Attribute &attribute) const noexcept
	-> std::optional<data::ReadHandle>
{
	return _readState.makeReadHandle(_readDataBlock, attribute);
}

auto TemplateIoTransaction::realize() -> void
{
	// Track the buffer size we need for pending events
	std::size_t readEventCount { 0 };
	std::size_t writeEventCount { 0 };

	// Add our own states
	_readState.attach(_readDataArray, readEventCount);
	_writeState.attach(_writeDataArray, writeEventCount);

	// Attach all the inputs
	for (auto &&input : _inputs)
	{
		input.get().attachInput(_readDataArray, readEventCount);
	}
	// Attach all the outputs
	for (auto &&output : _outputs)
	{
		output.get().attachOutput(_writeDataArray, writeEventCount);
	}

	// Create the data blocks
	_readDataBlock.create(memory::memoryResources::data());
	_writeDataBlock.create(memory::memoryResources::data());

	// Reserve space in the buffers
	_runtimeBuffers._eventsToRaise.reset(std::max(readEventCount, writeEventCount));
	_runtimeBuffers._outputsToNotify.reset(_outputs.size());
}

auto TemplateIoTransaction::prepare() -> void
{
	// Create a read command
	/// @todo initialized the read command properly based on the inputs to read.
	_readCommand.reset(new ReadCommand);

	/// @todo provide the information needed to decode the value to the inputs, like e.g. the correct data data offsets.
}

auto TemplateIoTransaction::performReadTask(const process::ExecutionContext &context) -> void
{
	read(context.scheduledTime());
}

auto TemplateIoTransaction::read(std::chrono::system_clock::time_point timeStamp) -> void
{
	try
	{
		/// @todo send the read command
		ReadCommand::Payload payload = {};

		/// @todo if the read function does not throw errors, but uses return types or internal handle state,
		// throw an std::system_error here on failure, or call updateInputs() directly.

		// The read was successful
		updateInputs(timeStamp, payload);
	}
	catch (const std::exception &)
	{
		// Get the error from the current exception using this special utility function
		const auto error = utils::eh::currentErrorCode();
		// Update the state
		updateInputs(timeStamp, utils::eh::unexpected(error));
	}
}

auto TemplateIoTransaction::performWriteTask(const process::ExecutionContext &context) -> void
{
	write(context.scheduledTime());
}

auto TemplateIoTransaction::write(std::chrono::system_clock::time_point timeStamp) -> void
{
	// Protect use of the list of outputs to notify
	RuntimeBufferSentinel eventsToRaiseSentinel(_runtimeBuffers._outputsToNotify);

	// Create a command
	WriteCommand command;

	// Collect pending outputs
	for (auto &&output : _outputs)
	{
		// Add the output
		if (output.get().addToWriteCommand(command))
		{
			_runtimeBuffers._outputsToNotify.push_back(output);
		}
	}

	// If there were no pending outputs, just bail
	if (_runtimeBuffers._outputsToNotify.empty())
	{
		return;
	}

	try
	{
		/// @todo send the command

		/// @todo if the write function does not throw errors, but uses return types or internal handle state,
		// throw an std::system_error here on failure, or call updateOutputs() directly.

		// The write was successful
		updateOutputs(timeStamp, std::error_code(), _runtimeBuffers._outputsToNotify);
	}
	catch (const std::exception &)
	{
		// Get the error from the current exception using this special utility function
		const auto error = utils::eh::currentErrorCode();
		// Update the state
		updateOutputs(timeStamp, error, _runtimeBuffers._outputsToNotify);
	}
}

auto TemplateIoTransaction::invalidateData(std::chrono::system_clock::time_point timeStamp) -> void
{
	// Set the state to "No Data"
	updateInputs(timeStamp, utils::eh::unexpected(CustomError::NoData));
}

auto TemplateIoTransaction::updateInputs(std::chrono::system_clock::time_point timeStamp, const utils::eh::expected<std::reference_wrapper<const ReadCommand::Payload>, std::error_code> &payloadOrError)
	-> void
{
	// Protect use of the pending event buffer
	RuntimeBufferSentinel eventsToRaiseSentinel(_runtimeBuffers._eventsToRaise);

	// Make a write sentinel
	memory::WriteSentinel sentinel { _readDataBlock };

	// Update the common read state
	const auto commonChanges = _readState.update(sentinel, timeStamp, payloadOrError.error(), _runtimeBuffers._eventsToRaise);

	// Update all the inputs
	for (auto &&input : _inputs)
	{
		input.get().updateReadState(sentinel, timeStamp, payloadOrError, commonChanges, _runtimeBuffers._eventsToRaise);
	}

	// Commit the data and raise the events
	sentinel.commit(timeStamp, _runtimeBuffers._eventsToRaise);
}

auto TemplateIoTransaction::updateOutputs(std::chrono::system_clock::time_point timeStamp, std::error_code error, const OutputList &outputs) -> void
{
	// Protect use of the pending event buffer
	RuntimeBufferSentinel eventsToRaiseSentinel(_runtimeBuffers._eventsToRaise);

	// Make a write sentinel
	memory::WriteSentinel sentinel { _readDataBlock };

	// Update the latest state
	_writeState.update(sentinel, timeStamp, error, _runtimeBuffers._eventsToRaise);

	// Update all the relevant outputs
	for (auto &&output : outputs)
	{
		output.get().updateWriteState(sentinel, timeStamp, error, _runtimeBuffers._eventsToRaise);
	}

	// Commit the data and raise the events
	sentinel.commit(timeStamp, _runtimeBuffers._eventsToRaise);
}

} // namespace xentara::plugins::templateDriver