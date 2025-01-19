#pragma once
#include <tuple>
#include <limits>
#include <vector>

typedef std::tuple<
	const int /* 0 job ID */,
	const int /* 1 machine ID */,
	const int /* 2 sequence number */,
	const int /* 3 length */,
	int       /* 4 start time NOT const */
> Task;

typedef std::vector<int /* task start time */> Chromosome;

typedef int Fitness;

typedef std::tuple<Chromosome, Fitness, int /* generation */> Specimen;

typedef std::vector<Specimen> Population;