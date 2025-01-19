#pragma once

#include "common.h"

/**
* The solution template for the Job Shop problem.
* It contains the tasks, the machines, and the jobs.
* From the given problem input, the solution template is constructed.
*
* Template is necessary to control the following:
* - the order of the tasks in each machine's timeline
* - the sequence of the tasks in each job
*
* because we need to guarantee that we don't have conflicts (overlaps of tasks) in the same machine
*  and sequence of tasks in every job is correct.
*
* You calculate the fitness of the chromosome by filling the start times from the chromosome into the solution template.
* Then the template calculates the fitness by counting the conflicts and measuring the total runtime.
*
* Three measures are performed for fitness:
* 1. number of absolute conflicts - overlaps of tasks in the same machine
* 2. number of sequence conflicts - wrong sequence of tasks in the same job (it's actually stronger than "sequence of start times", they cannot even overlap, it's sequence of start times followed by end times)
* 3. total runtime - the end time of the last task in the machine with the largest end time.
*
* Total runtime is compared to the horizon and the absolute lowest bound.
*/
class SolutionTemplate
{
private:
	std::vector < Task > tasks;

	/*
	 * for performance, we keep a vector of indices only, no need to copy the values all the time
	 * For performance reasons as well, sequence of tasks in the machines (in time) may be not the sequence of IDs in these vectors.
	 * so if you need to show the actual order of tasks on the timeline, you first need to sort this vector by the start time of the tasks.
	 */
	std::vector < std::vector < int /* index in tasks */ > > machines;

	/*
	 * same as machines, indices only
	 * Order of tasks in each job is important and it's being kept all the time.
	 * That is, the sequence of tasks in each job is exactly the sequence of IDs in these vectors.
	 */
	std::vector < std::vector < int /* index in tasks */ > > jobs;

	int __cached_horizon{ -1 };
	int __cached_absolute_lowest_bound{ -1 };

public:
	/**
	* This method is used to fill the template with the start times from the chromosome.
	* Use this method before calculating the fitness.
	*/
	void fill_start_times(const Chromosome& start_times)
	{
		if (start_times.size() != tasks.size())
		{
			throw std::runtime_error("Numbers of start times in the chromosome must be the same as number of tasks in the solution template.");
		}

		for (auto i = 0; i < tasks.size(); ++i)
		{
			std::get<4>(tasks[i]) = start_times[i];
		}

		// even if we don't run the conflict resolution algorithm, we need to sort the tasks in the machines by the start time
		for (auto& machine : machines)
		{
			std::sort(machine.begin(), machine.end(), [&tasks = tasks](int a, int b) {
				return std::get<4>(tasks[a]) < std::get<4>(tasks[b]);
				});
		}
	}

	/**
	* From the given input, construct one of the jobs of the solution template.
	* This is used before the genetic algorithm, to setup the proper solution template.
	*/
	void add_job(const int& job_id, const std::vector< std::pair<int /* machine ID */, int /* length */> >& steps)
	{
		jobs.resize(job_id + 1);
		// immediately set to the first element of the new job
		auto task_index = tasks.size();

		int sequence_number = 0;
		for (const auto& step : steps)
		{
			int machine_id = step.first;
			int length = step.second;

			// this is the task of `task_index` index
			tasks.push_back(std::make_tuple(job_id, machine_id, sequence_number, length, 0));

			// register in jobs
			jobs[job_id].push_back(task_index);

			// register in machines
			if (machines.size() <= machine_id)
			{
				machines.resize(machine_id + 1);
			}
			machines[machine_id].push_back(task_index);

			++sequence_number;
			++task_index;
		}
		__cached_absolute_lowest_bound = -1;
		__cached_horizon = -1;
	}

	/* debug method for seeing the current state of the template (with start times filled or not) */
	void print() const
	{
		int task_id{ 0 };
		for (const auto& task : tasks)
		{
			std::cout << task_id << "\t" << " Job: " << std::get<0>(task) << ", Machine: " << std::get<1>(task) << ", Sequence: " << std::get<2>(task) << ", Length: " << std::get<3>(task) << ", Start time: " << std::get<4>(task) << "\n";
			++task_id;
		}

		int machine_index = 0;
		for (const auto& machine : machines)
		{
			std::cout << "Machine: " << machine_index++ << ": ";
			for (const auto& task_index : machine)
			{
				std::cout << task_index << " ";
			}
			std::cout << "\n";
		}

		int job_index = 0;
		for (const auto& job : jobs)
		{
			std::cout << "Job: " << job_index++ << ": ";
			for (const auto& task_index : job)
			{
				std::cout << task_index << " ";
			}
			std::cout << "\n";
		}
	}

	/*
	 * Function to resolve the conflicts automatically.
	 * The idea of the resolution process is to move the conflicting (overlapping) tasks forward in time.
	 *
	 * For example, we have task A with start time 1 and length 5, and task B with start time 3 and length 3.
	 * so we have an overlap of 2 units of time.
	 * 
	 * We need to first check the serious-level conflicts, which are the conflicts on the level of jobs.
	 * Because these conflicts include sequence breaks (a task with higher sequence number starts before the task with lower sequence number ends).
	 * Resolution of the job-level conflicts is moving all the tasks starting from the conflicting one in the same JOB forward in time.
	 * 
	 * This procedure can introduce wrong ordering on the machine level - order of indices in the machine vector
	 * will not represent the actual start times of the tasks on the machine anymore.
	 * So we sort the indices in all the machine vectors by the start times of the tasks.
	 * 
	 * After that we go over every machine and resolve the conflicts on the machine level 
	 * by doing the same: moving the conflicting tasks and all tasks in them forward in time,
	 * but this time we move by MACHINE not by job.
	 * 
	 * Then, we repeat the process until there are no conflicts left.
	 * This is the most brittle part of the process as I can't guarantee that the algorithm will always converge.
	 * It looks reasonable because we are always "spreading the tasks out" in time, but it's not guaranteed
	 * as we try to satisfy two constraints at the same time and I am unable to prove it mathematically right now.
	 */
	void resolve_conflicts()
	{
		bool had_collision;
		do
		{
			had_collision = false;
			/* The algorithm:
	had_collision := false
	for every job
		for every step
			if no step with the next sequence ID
				stop
			next_start := start time of the step with the next sequence ID
			if own_start + length > next_start
				had_collision := true
				diff := (own_start + length - next_start)
				pick the track of the next step
					move all the steps on this track `diff` forward
*/
			// first pass is the check for sequence breaks in jobs
			// we need to eliminate them first because if we have sequence breaks we will need to do drastical changes to schedule
			// (swap of the tasks inside the job) which potentially moves the task very far away on the machine timeline
			// after that we will need to resolve the conflicts on the machine level
			for (const auto& steps : jobs)
			{
				for (int i = 0; i < steps.size() - 1; ++i)
				{
					const auto& left_task_index = steps[i];
					const auto& right_task_index = steps[i + 1];
					const auto& task1 = tasks[left_task_index];
					const auto& task2 = tasks[right_task_index];
					int start1 = std::get<4>(task1);
					int length1 = std::get<3>(task1);
					int start2 = std::get<4>(task2);

					// positive diff means collision (overlap)
					// zero diff means zero time between the tasks
					int diff = start1 + length1 - start2;
					if (diff > 0)
					{
						had_collision = true;
						// move the right task and all after it forward in time by `diff`
						// note that it can break the sequence of IDs of tasks in the `machines` vector
						// that is, the sequence of IDs in the vectors inside `machines` will not actually represent the timelines of tasks on these machines
						for (int j = i + 1; j < steps.size(); ++j)
						{
							auto& task = tasks[steps[j]];
							std::get<4>(task) += diff;
						}
					}
				} // end for job steps
			} // end for jobs

			// now we need to resolve the machine-level collisions
			// especially because the job-level collisions resolution could have created new ones.
			for (auto& machine : machines)
			{
				// sort the ids in the machine by the start time, more accurately representing the timeline after the job-level resolution
				std::sort(machine.begin(), machine.end(), [&tasks = tasks](int a, int b) {
					return std::get<4>(tasks[a]) < std::get<4>(tasks[b]);
					});

				for (int i = 0; i < machine.size() - 1; ++i)
				{
					const auto& left_task_index = machine[i];
					const auto& right_task_index = machine[i + 1];
					const auto& task1 = tasks[left_task_index];
					const auto& task2 = tasks[right_task_index];
					int start1 = std::get<4>(task1);
					int length1 = std::get<3>(task1);
					int start2 = std::get<4>(task2);

					// positive diff means collision (overlap)
					// zero diff means zero time between the tasks
					int diff = start1 + length1 - start2;
					if (diff > 0)
					{
						had_collision = true;
						// move the right task and all after it forward in time by `diff`
						for (int j = i + 1; j < machine.size(); ++j)
						{
							auto& task = tasks[machine[j]];
							std::get<4>(task) += diff;
						}
					}
				} // end for machine steps
			} // end for machines
		} while (had_collision);
	}

	Chromosome get_chromosome() const
	{
		Chromosome result;
		for (const auto& task : tasks)
		{
			result.push_back(std::get<4>(task));
		}
		return result;
	}

	int horizon()
	{
		if (__cached_horizon == -1)
		{
			__cached_horizon = calculate_horizon();
		}
		return __cached_horizon;
	}

	int calculate_horizon() const
	{
		int result = 0;
		for (const auto& task : tasks)
		{
			result += std::get<3>(task);
		}
		return result;
	}

	int absolute_lowest_bound()
	{
		if (__cached_absolute_lowest_bound == -1)
		{
			__cached_absolute_lowest_bound = calculate_absolute_lowest_bound();
		}
		return __cached_absolute_lowest_bound;
	}

	int calculate_absolute_lowest_bound() const
	{
		int max_time{ 0 };
		for (const auto& machine : machines)
		{
			int current_time{ 0 };
			for (const auto& task_index : machine)
			{
				current_time += std::get<3>(tasks[task_index]);
			}
			if (current_time > max_time)
			{
				max_time = current_time;
			}
		}
		return max_time;
	}

	/*
	 * You MUST call this function after the conflicts are resolved!!!
	 * Also you MUST call this function only after you called `horizon()` and `absolute_lowest_bound()` at least once,
	 * so the cached values are set. To remove this restriction, we would need to refactor this class very significantly
	 * which does not worth it for the research problem.
	 */
	double fitness() const
	{
		/*
		* fitness calculation is as follows: 
		* calculate the total runtime
		* Determine as a `double` ratio value where in the range (absolute lowest bound, horizon) the total runtime is.
		*/
		double total_runtime_value = total_runtime();
		double horizon_value = __cached_horizon;
		double absolute_lowest_bound_value = __cached_absolute_lowest_bound;

		if (total_runtime_value < absolute_lowest_bound_value)
		{
			return 1.0;
		}

		if (total_runtime_value > horizon_value)
		{
			return 0.0;
		}

		return 1.0 - (total_runtime_value - absolute_lowest_bound_value) / (horizon_value - absolute_lowest_bound_value);
	}

	/*
	 * You MUST guarantee that the elements in vector `machines` are sorted by the start time of the tasks.
	 */
	int total_runtime() const
	{
		int max_time{ 0 };
		for (const auto& machine : machines)
		{
			int last_task_index = machine[machine.size() - 1];
			int end_time = std::get<4>(tasks[last_task_index]) + std::get<3>(tasks[last_task_index]);

			max_time = std::max(max_time, end_time);
		}
		return max_time;
	}

	void visualize() const
	{
		int machine_index{ 0 };
		for (const auto& machine_task_indices : machines)
		{
			std::cout << "Machine " << machine_index << ": ";
			for (const auto task_index : machine_task_indices)
			{
				const auto& task = tasks[task_index];
				std::cout << "(j" << std::get<0>(task) << "s" << std::get<2>(task) << " "
					<< std::get<4>(task) << "+" << std::get<3>(task) << ") ";
			}
			std::cout << "\n";
			++machine_index;
		}
		std::cout << "\n";
		std::cout << "Total runtime: " << total_runtime() << "\n";
	}
};
