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

#include <iostream>
#include <iomanip>
#include <fstream>
#include <utility>
#include <sstream>

#include <random>

#include "common.h"
#include "SolutionTemplate.h"

std::random_device rd;

/* ------------ SETTINGS BEGIN ------- */

std::mt19937 random_engine(rd());

constexpr auto problem_filename = "la40seti5.txt";
constexpr auto crossover_type = "1-point"; // "1-point" or "2-point"
constexpr auto is_selection_tainted = true; // whether we put the worst specimen back into the population
constexpr auto mutation_type = "singular"; // "singular" or "uniform XOR"


// set the number of chromosomes in the population
constexpr auto population_size = 10000;
constexpr auto index_of_middle_specimen = population_size / 2 - 1;
constexpr auto index_of_last_specimen = population_size - 1;

// set the number of generations
constexpr auto generations = 500;

// set the probability of mutation
constexpr auto mutation_probability = 10;
constexpr auto MIN_MUTATION_VALUE = -2;
constexpr auto MAX_MUTATION_VALUE = 2;

/* ------------ SETTINGS END ------- */

// for percents let's use actual percent values instead of doubles, it's easier to work with integers
static std::uniform_int_distribution<> random_percent_distribution(0, 100);


// the solution template is a global variable as we never create more than one instance of it
static SolutionTemplate solution_template;

/*
* 2-point crossover between two vectors.
* Min size of both vectors is 3.
* We never swap the first element.
*/
std::pair<Chromosome, Chromosome> crossover_2point(const Chromosome& left, const Chromosome& right)
{
    if (left.size() != right.size())
    {
        throw std::runtime_error("Chromosomes must be of the same length.");
    }
	if (left.size() < 3)
	{
		throw std::runtime_error("Chromosomes must have at least 3 elements.");
	}

	// sneaky sneaky static + globals
    static std::uniform_int_distribution<> dist(1, left.size() - 2);
	int point1 = dist(random_engine);
    int point2 = dist(random_engine);

    if (point1 > point2)
    {
        std::swap(point1, point2);
    }
	if (point1 == point2)
	{
        point2++;
	}


	Chromosome offspring1{ left };
	Chromosome offspring2{ right };

	// swap_ranges doesn't compile in VS 2022
	//auto left_start_position = left.begin() + point1;
	//auto left_end_position = left.begin() + point2;
	//auto right_start_position = right.begin() + point1;
	//std::swap_ranges(left_start_position, left_end_position, right_start_position); 
	// so we use a loop with standard swap instead
	for (auto i = 0; i < point2 - point1; ++i)
	{
		std::swap(offspring1[point1 + i], offspring2[point1 + i]);
	}

	solution_template.fill_start_times(offspring1);
	solution_template.resolve_conflicts();
	offspring1 = solution_template.get_chromosome();

	solution_template.fill_start_times(offspring2);
	solution_template.resolve_conflicts();
	offspring2 = solution_template.get_chromosome();

    return std::make_pair(offspring1, offspring2);
}

/*
* 1-point crossover between two vectors.
* Min size of both vectors is 3.
*/
std::pair<Chromosome, Chromosome> crossover_1point(const Chromosome& left, const Chromosome& right)
{
	if (left.size() != right.size())
	{
		throw std::runtime_error("Chromosomes must be of the same length.");
	}
	if (left.size() < 3)
	{
		throw std::runtime_error("Chromosomes must have at least 3 elements.");
	}

	// sneaky sneaky static + globals
	static std::uniform_int_distribution<> dist(1, left.size() - 2);
	int crossover_point = dist(random_engine);

	Chromosome offspring1{ left };
	Chromosome offspring2{ right };

	for (auto i = 0; i < crossover_point; ++i)
	{
		std::swap(offspring1[i], offspring2[i]);
	}

	solution_template.fill_start_times(offspring1);
	solution_template.resolve_conflicts();
	offspring1 = solution_template.get_chromosome();

	solution_template.fill_start_times(offspring2);
	solution_template.resolve_conflicts();
	offspring2 = solution_template.get_chromosome();

	return std::make_pair(offspring1, offspring2);
}
/**
* Returns the NEW chromosome with one of the start times mutated.
* There's randomness both in the position of the mutation and the value of the mutation.
*/
Chromosome mutate(const Chromosome& input)
{
	// sneaky sneaky static + globals
	static std::uniform_int_distribution<> positions_distribution(0, input.size() - 1);
	auto position = positions_distribution(random_engine);

	static std::uniform_int_distribution<int> mutation_value_distribution(MIN_MUTATION_VALUE, MAX_MUTATION_VALUE);
	auto mutation_value = mutation_value_distribution(random_engine);

	Chromosome result{ input };
	result[position] += mutation_value;

	// if mutation goes below 0, we need to correct it
	if (result[position] < 0)
	{
		// we replace the mutation value with the absolute value of the mutation
		result[position] -= mutation_value * 2;
	}

	solution_template.fill_start_times(result);
	solution_template.resolve_conflicts();

	return solution_template.get_chromosome();
}

/*
 * make a chromosome
 *
 * MUST be called after the solution_template is set up!!!
 * 
 * we cannot just make a random array of ints
 * every int is a starting time of the task
 * we need to apply the same conflict resolution algorithm to the chromosome
 * also we need to allow chromosomes where starting time of non-conflicting tasks may overlap,
 * because it's allowed and it's a whole point of parallelism through machines.
*/
Chromosome make_chromosome()
{
	// get the chromosome of the correct length
	Chromosome raw = solution_template.get_chromosome();

	// 1. fill the chromosome with random numbers between 0 and half of horizon
	static std::uniform_int_distribution<> start_time_distribution(0, solution_template.horizon() / 2);
	for (auto& start_time : raw)
	{
		// start_time is a reference, so we can modify it directly
		start_time = start_time_distribution(random_engine);
	}

	// 2. put the randomized chromosome back into the solution template and resolve conflicts
	solution_template.fill_start_times(raw);
	solution_template.resolve_conflicts();

	// 3. get the clean chromosome back
	return solution_template.get_chromosome();
}

Specimen solve_using_genetic_algorithm()
{
	Population population;

	// generate the initial population
	for (int i = 0; i < population_size; ++i)
	{
		population.push_back({ make_chromosome(), 0, 0 });
	}

	for (int generation{ 0 }; generation < generations; ++generation)
	{
		for (auto& specimen : population)
		{
			// calculate the fitness of each chromosome of this generation
			if (std::get<2>(specimen) == generation)
			{
				solution_template.fill_start_times(std::get<0>(specimen));
				// don't need to resolve conflicts as all our operators do it
				std::get<1>(specimen) = solution_template.fitness();
			}
		}
		// sort the population by fitness descending
		std::sort(population.begin(), population.end(), [](const Specimen& a, const Specimen& b) {
			return std::get<1>(a) > std::get<1>(b);
			});

		std::cout << std::fixed << std::setprecision(2);
		if (generation % 50 == 0)
		{
			std::cout << "generation " << generation
				<< "\tbest fitnesses: "
				<< std::get<1>(population[0]) << ", "
				<< std::get<1>(population[1]) << ", "
				<< std::get<1>(population[2])
				<< "\tworst fitnesses: "
				<< std::get<1>(population[population_size - 2]) << ", "
				<< std::get<1>(population[population_size - 1]) << "\n";
		}

		if (generation == generations - 1)
		{
			// on the last generation we don't need to breed, just stop
			std::cout << "Last generation reached.\n";
			break;
		}

		// put the worst chromosome into the half of the population allowed to breed
		std::swap(population[index_of_middle_specimen], population[index_of_last_specimen]);

		// for each pair of specimens in the first half of the population
		for (size_t i = 0; i < population_size / 2; i += 2)
		{
			// obtain their chromosomes
			const auto& parent1 = std::get<0>(population[i]);
			const auto& parent2 = std::get<0>(population[i + 1]);

			// crossover the chosen chromosomes obtaining the new pair
			Chromosome offspring1;
			Chromosome offspring2;
			if (crossover_type == "1-point")
			{
				std::tie(offspring1, offspring2) = crossover_1point(parent1, parent2);
			}
			else if (crossover_type == "2-point")
			{
				std::tie(offspring1, offspring2) = crossover_2point(parent1, parent2);
			}
			else
			{
				throw std::runtime_error("Unknown crossover type.");
			}

			// construct two new specimens with the new pair and the generation number
			Specimen new_specimen1{ offspring1, 0, generation + 1 };
			Specimen new_specimen2{ offspring2, 0, generation + 1 };

			// put the new pair into the second half of the population
			population[population_size / 2 + i] = new_specimen1;
			population[population_size / 2 + i + 1] = new_specimen2;
		}

		// mutate the whole population
		for (auto& specimen : population)
		{
			bool should_mutate = random_percent_distribution(random_engine) < mutation_probability;
			if (should_mutate)
			{
				std::get<0>(specimen) = mutate(std::get<0>(specimen));
				if (std::get<2>(specimen) == generation)
				{
					// if we mutated the chromosome in the current generation,
					// we need to recalculate the fitness as we calculate the fitness only once per new generation
					solution_template.fill_start_times(std::get<0>(specimen));
					std::get<1>(specimen) = solution_template.fitness();
				}
			}
		}
	}

	solution_template.fill_start_times(std::get<0>(population[0]));
	std::cout << "Best solution found:\n";
	solution_template.print();
	solution_template.visualize();
	std::cout << "Fitness: " << std::get<1>(population[0]) << "\n";
	std::cout << "Generation: " << std::get<2>(population[0]) << "\n";

	return population[0];
}

/* debug function to test the conflict resolution */
void single_test()
{
	Chromosome left = make_chromosome();
	Chromosome right = make_chromosome();
	solution_template.fill_start_times(left);
	std::cout << "Left chromosome:\n";
	solution_template.visualize();
	solution_template.fill_start_times(right);
	std::cout << "Right chromosome:\n";
	solution_template.visualize();
	auto [offspring1, offspring2] = crossover_2point(left, right);

	std::cout << "Offspring 1:\n";
	solution_template.fill_start_times(offspring1);
	solution_template.visualize();
	solution_template.resolve_conflicts();
	std::cout << "Offspring 1 resolved:\n";
	solution_template.visualize();

	std::cout << "Offspring 2:\n";
	solution_template.fill_start_times(offspring2);
	solution_template.visualize();
	solution_template.resolve_conflicts();
	std::cout << "Offspring 2 resolved:\n";
	solution_template.visualize();
}

void exact_test()
{
	Chromosome test{ 6, 14, 20, 3, 13, 24, 5, 11 };
	solution_template.fill_start_times(test);
	std::cout << "Test chromosome:\n";
	solution_template.visualize();
	solution_template.resolve_conflicts();
	std::cout << "Test chromosome resolved:\n";
	solution_template.visualize();
	std::cout << "Fitness: " << solution_template.fitness() << "\n";
}

int main()
{
    std::ifstream file(problem_filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open the file." << std::endl;
        return 1;
    }

	int horizon{ 0 };
	int chromosome_length{ 0 };

    std::vector<std::pair<int, int>> input_data_line;
    std::string input_text_line;
	int machine_id;
	int task_length;
	int job_id{ 0 };
	while (std::getline(file, input_text_line))
    {
		input_data_line.clear();
        std::istringstream iss(input_text_line);
        while (iss >> machine_id >> task_length)
        {
            input_data_line.push_back(std::make_pair(machine_id, task_length));
			horizon += task_length;
			++chromosome_length;
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

	std::cout << "Horizon by us: " << horizon << " Horizon by template: " << solution_template.horizon() <<  "\n";
	std::cout << "Absolute lowest_bound: " << solution_template.absolute_lowest_bound() << "\n";

	solve_using_genetic_algorithm();

	// uncomment only for debugging purposes
	// single_test();
	// exact_test();

    return 0;
}
