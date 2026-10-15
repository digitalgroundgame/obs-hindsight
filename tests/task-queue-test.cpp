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

#include "check.hpp"
#include "task-queue.hpp"

#include <atomic>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace {

void runs_tasks_in_order()
{
	std::vector<int> order;
	{
		TaskQueue queue(10);
		for (int i = 0; i < 5; i++)
			CHECK(queue.push([&order, i]() { order.push_back(i); }));
	}
	// The destructor ran everything still queued before returning.
	CHECK(order == (std::vector<int>{0, 1, 2, 3, 4}));
}

void refuses_tasks_past_the_limit()
{
	std::promise<void> release;
	std::shared_future<void> gate = release.get_future().share();
	std::promise<void> started;
	std::atomic<int> ran{0};
	{
		TaskQueue queue(2);
		// Hold the worker inside the first task so the queue fills up.
		CHECK(queue.push([&]() {
			started.set_value();
			gate.wait();
			ran++;
		}));
		started.get_future().wait();

		CHECK(queue.push([&]() { ran++; }));
		CHECK(queue.push([&]() { ran++; }));
		CHECK(!queue.push([&]() { ran++; }));

		release.set_value();
	}
	CHECK(ran == 3);
}

void accepts_tasks_from_many_threads()
{
	std::mutex lock;
	int total = 0;
	{
		TaskQueue queue(1000);
		std::vector<std::thread> threads;
		threads.reserve(4);
		for (int t = 0; t < 4; t++) {
			threads.emplace_back([&]() {
				for (int i = 0; i < 100; i++)
					queue.push([&]() {
						std::lock_guard<std::mutex> guard(lock);
						total++;
					});
			});
		}
		for (std::thread &thread : threads)
			thread.join();
	}
	CHECK(total == 400);
}

void stops_with_nothing_queued()
{
	// Constructing and destroying an idle queue must not hang.
	TaskQueue queue(1);
}

} // namespace

int main()
{
	runs_tasks_in_order();
	refuses_tasks_past_the_limit();
	accepts_tasks_from_many_threads();
	stops_with_nothing_queued();
	return check::finish();
}
