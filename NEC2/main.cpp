#include <iostream>
#include <fstream>
#include <array>
#include <utility>
#include <sstream>

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

constexpr int N = 5; // Example value, replace with actual number of pairs

int main()
{
    std::ifstream file("numbers.txt");
    if (!file.is_open())
    {
        std::cerr << "Failed to open the file." << std::endl;
        return 1;
    }

    std::array<std::pair<int, int>, N> pairs;
    std::string line;
    if (std::getline(file, line))
    {
        std::istringstream iss(line);
        for (int i = 0; i < N; ++i)
        {
            int first, second;
            if (!(iss >> first >> second))
            {
                std::cerr << "Error reading pair number " << i + 1 << std::endl;
                return 1;
            }
            pairs[i] = std::make_pair(first, second);
        }
    }
    else
    {
        std::cerr << "Failed to read the line from the file." << std::endl;
        return 1;
    }

    // Output the pairs to verify
    for (const auto& p : pairs)
    {
        std::cout << "(" << p.first << ", " << p.second << ")" << std::endl;
    }

    return 0;
}
