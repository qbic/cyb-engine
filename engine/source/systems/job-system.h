#pragma once
#include <atomic>
#include <functional>

namespace cyb::jobsystem
{
	struct JobArgs
	{
		uint32_t jobIndex;
		uint32_t groupID;
		uint32_t groupIndex;
		bool isFirstJobInGroup;
		bool isLastJobInGroup;
	};

	void Initialize();
	uint32_t GetThreadCount();

	// Defines a state of execution, can be waited on
	struct Context
	{
		std::atomic<uint32_t> counter{ 0 };
	};

	void Execute(Context& ctx, const std::function<void(JobArgs)>& task);
	void Dispatch(Context& ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)>& task);
	bool IsBusy(const Context& ctx);
	void Wait(const Context& ctx);
}
