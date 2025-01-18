#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <sstream>
#include <tuple>

#include <random>

std::random_device rd;
std::mt19937 mersenne_twister(rd());

constexpr auto MIN_MUTATION_VALUE = -2;
constexpr auto MAX_MUTATION_VALUE = 2;

/* Pre-created distribution which we can simply call (passing the generator as a value) */
std::uniform_int_distribution<> random_mutation_values(MIN_MUTATION_VALUE, MAX_MUTATION_VALUE);


/*
* The Job Shop problem solved by the Genetic Algorithm
* 
The horizon: total runtime if all the tasks will be put sequentially
We can also estimate the absolute lowest bound of the runtime:
if we ignore the restrictions

Absolute lowest bound and the horizon can be used to estimate the quality of the solution (the fitness))

Encoding of the solution (the chromosome):

the jobs themselves are fixed, there's nothing to modify or change
the only thing that can be changed is the order of the jobs

job is placed into the machine, the machine is fixed
and onto a time slot, the time slot can be modified


One of the first problems that must be dealt with in the JSP is that many proposed solutions have infinite cost: 
In fact, it is quite simple to concoct examples of such
by ensuring that two machines will deadlock, so that each waits for the output of the other's next step.


Parts of the encoding are as follows:
We have a job.
Job consists of steps.
Each step is a pair of a machine and a length.
Each step has two constraints.
First, it must be placed to a machine, that is, each step is bound to a machine.
Second, it has a sequence number inside the job, that is, step with the higher sequence number must be placed later as the step with the lower sequence number.

a solution is a list of pairs: step and time slot (position in the timeline)

once we have a list of steps, the filling for the machines is set in stone
we know for sure which steps will be placed on each machine

the only thing that can be changed is the order of the steps and their spread in time

so when generating the initial population, we can generate the list of steps and then shuffle them
each machine is a separate "track".
Number of "tracks" is different per each task to solve.
so, we can eliminate the violations in the same track at least
by sorting the steps by their sequence number in the same job number.

then the only violations which will be left are the violations of sequence numbers between machines.
This can be a part of the fitness function.

we will need a pretty big mutation rate which will "wiggle" the steps in the timeline

crossover operator will be "multiplexed" crossover
we will crossover all the tracks but each track will be crossed over separately
with the corresponding track from the second parent

this is because the steps are unable to change the machine they are bound to
but we need a way to move the steps in time
to find the position for the crossover
we will not pick a position in the timeline, but pick a *step number* on a track,
and then exchange the steps before and after that number.
the position in time of the right half of the chromosome will be the maximum of the two parents to not collide

There's a paper on GT/GA for JSP, but I 1) don't want to repeat what they did
2) don't have enough expertise to implement their pretty complicated algorithmic solution.

I cannot blindly swap the steps when crossing over, because I risk getting duplicates.
I need to invent a way to avoid duplicates.

I have two "tracks" of unique steps.
On every "track" they are in a different order.

I want to get two different tracks with the same steps

I cannot split tracks.
I need to switch the tracks themselves.

Crossover will split a list of tracks - list of machines -
to two lists of tracks,
and the solutions will exchange the full lists of tracks.

I only need to "wiggle" the steps in the timeline enough
to eliminate the violations of the sequence numbers between the machines.

Start with the most inefficient solution and go backwards!!!

This way we can ensure that all the solutions will always be feasible along the way,
no cross-track violations will be possible, or I can eliminate them immediately when mutating.

Mutations will _shorten_ the time gaps not _lengthen_ them.
*/

typedef std::tuple<const int /* job ID */, const int /* machine ID */, const int /* sequence number */, const int /* length */, int /* start time NOT const */> Task;

typedef std::vector<int /* task start time */> Chromosome;

/*
* 2-point crossover between two vectors.
* Min size of both vectors is 3.
* We never swap the first element.
*/
std::pair<Chromosome, Chromosome> crossover(const Chromosome& left, const Chromosome& right)
{
    if (left.size() != right.size())
    {
        throw std::runtime_error("Chromosomes must be of the same length.");
    }
	if (left.size() < 3)
	{
		throw std::runtime_error("Chromosomes must have at least 3 elements.");
	}

    std::uniform_int_distribution<> dist(1, left.size() - 2);
    int point1 = dist(mersenne_twister);
    int point2 = dist(mersenne_twister);

    if (point1 > point2)
    {
        std::swap(point1, point2);
    }
	if (point1 == point2)
	{
        point2++;
	}

	auto left_start_position = left.begin() + point1;
	auto left_end_position = left.begin() + point2;
	auto right_start_position = right.begin() + point1;

	Chromosome offspring1{ left };
	Chromosome offspring2{ right };

	//std::swap_ranges(left_start_position, left_end_position, right_start_position); // swap_ranges doesn't compile in VS 2022
	for (auto i = 0; i < point2 - point1; ++i)
	{
		std::swap(offspring1[point1 + i], offspring2[point1 + i]);
	}

    return std::make_pair(offspring1, offspring2);
}

/*
 * More natural way to get the random values.
 * Normally we have a function which we tell the bounds and it returns the value.
 * 
 * But with the device and the generator, we can create the distribution once and then call it with the generator as a value.
 * It's a bit unnatural to fiddle with such a low-level primitives as a generator, so it's wrapped in this helper.
 */
int make_random_mutation_value()
{
	return random_mutation_values(mersenne_twister);
}

/**
* Returns the NEW chromosome with one of the start times mutated.
* There's randomness both in the position of the mutation and the value of the mutation.
*/
Chromosome mutate(const Chromosome& input)
{
	std::uniform_int_distribution<> dist(0, input.size());
	auto position = dist(mersenne_twister);

	Chromosome result{ input };
	result[position] += make_random_mutation_value();
	return result;
}

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
        for (const auto &step: steps)
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
			std::cout << "\t" << task_id << " Job: " << std::get<0>(task) << ", Machine: " << std::get<1>(task) << ", Sequence: " << std::get<2>(task) << ", Length: " << std::get<3>(task) << ", Start time: " << std::get<4>(task) << "\n";
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
};

int main()
{
    std::ifstream file("numbers.txt");
    if (!file.is_open())
    {
        std::cerr << "Failed to open the file." << std::endl;
        return 1;
    }

	SolutionTemplate solution_template;

    std::vector<std::pair<int, int>> input_data_line;
    std::string input_text_line;
	int first, second;
	int job_id{ 0 };
	while (std::getline(file, input_text_line))
    {
		input_data_line.resize(0);
        std::istringstream iss(input_text_line);
        while (iss >> first >> second)
        {
            input_data_line.push_back(std::make_pair(first, second));
        }

		// Output the pairs to verify
		for (const auto& p : input_data_line)
		{
			std::cout << "(" << p.first << ", " << p.second << ") ";
		}
		std::cout << "\n";


		solution_template.add_job(job_id, input_data_line);
		std::cout << "Job " << job_id << " added.\n";

		++job_id;
    }
    
	std::cout << "Done reading the file.\n";


	std::cout << "Solution template:\n";
	solution_template.print();

    return 0;
}
