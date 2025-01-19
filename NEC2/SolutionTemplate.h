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

	/* for performance, we keep a vector of indices only, no need to copy the values all the time */
	std::vector < std::vector < int /* index in tasks */ > > machines;

	/* same as machines, indices only */
	std::vector < std::vector < int /* index in tasks */ > > jobs;

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

	// TODO: fitness function

	// TODO: function to resolve the conflicts automatically.
	// the idea of the resolution process is to move the conflicting (overlapping) tasks forward in time.
	// for example, we have task A with start time 1 and length 5, and task B with start time 3 and length 3.
	// so we have an overlap of 2 units of time.
	// we need to go to the machine where the tasks A and B are, and move the task B AND ALL THE FOLLOWING TASKS forward in time by 2 units.
	// this will resolve the absolute-level conflict on the level of machines.
	// but we still need to resolve the sequence-level conflicts.
	// for that we go over the JOBS now, and check whether we have the overlaps there.
	// in case of overlaps, we still go to the machine and move the conflicting tasks forward in time in the span of the machine.
	// this will resolve the sequence-level conflicts.
	// So, the resolution happens always on the level of the machine, by moving the target task and all the following tasks forward in time.
	// this may be a separate function.
	// But the detection happens in two ways: either on the jobs level or on the machines level.
	// to perform the full resolution, it must be a repeated process. We go through the machines,
	// then we go through the jobs, then we go through the machines again, and so on, until there are no conflicts left.
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
			for (const auto& machine : machines)
			{
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

	// TODO this function accepts the random generator and setups the internal functions for the random distributions
	void setup_random_distributions(/* TODO */)
	{
		// TODO fill this later
	}

	/*
	 * make a chromosome
	 *
	 * DESTROYS CURRENT STATE OF THE TEMPLATE
	 *
	 * we cannot just make a random array of ints
	 * every int is a starting time of the task
	 * we need to apply the same conflict resolution algorithm to the chromosome
	 * also we need to allow chromosomes where starting time of non-conflicting tasks may overlap, because it's allowed and it's a whole point of parallelism through machines.
	*/
	Chromosome make_chromosome()
	{
		// 1. fill the start times of tasks with random numbers
		for (auto& task : tasks)
		{
			std::get<4>(task) = 0; // TODO fix this later
		}

		// 2. resolve conflicts

		// 3. make the Chromosome from the start times
		Chromosome result;
		// fill with the start times 
		for (const auto& task : tasks)
		{
			result.push_back(std::get<4>(task));
		}

		return result;
	}

	int horizon() const
	{
		int result = 0;
		for (const auto& task : tasks)
		{
			result += std::get<3>(task);
		}
		return result;
	}

	int absolute_lowest_bound() const
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
};
