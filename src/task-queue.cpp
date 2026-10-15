/*
Hindsight
Copyright (C) 2026 DGG AC Dev Team

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "task-queue.hpp"

#include <utility>

TaskQueue::TaskQueue(std::size_t max_pending) : max_pending(max_pending), worker([this]() { run(); }) {}

TaskQueue::~TaskQueue()
{
	{
		std::lock_guard<std::mutex> guard(lock);
		stopping = true;
	}
	wake.notify_one();
	worker.join();
}

bool TaskQueue::push(std::function<void()> task)
{
	{
		std::lock_guard<std::mutex> guard(lock);
		if (stopping || tasks.size() >= max_pending)
			return false;
		tasks.push_back(std::move(task));
	}
	wake.notify_one();
	return true;
}

void TaskQueue::run()
{
	for (;;) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> guard(lock);
			wake.wait(guard, [this]() { return stopping || !tasks.empty(); });
			// Finish what was queued before stopping.
			if (tasks.empty())
				return;
			task = std::move(tasks.front());
			tasks.pop_front();
		}
		task();
	}
}
