#pragma once
#include <tuple>
#include <limits>
#include <vector>

typedef std::tuple<const int /* job ID */, const int /* machine ID */, const int /* sequence number */, const int /* length */, int /* start time NOT const */> Task;

typedef std::vector<int /* task start time */> Chromosome;

typedef int Fitness;