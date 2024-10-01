#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "bitmap.hpp"
#include "netz.hpp"
#include "netz_formulas.hpp"

constexpr double ACCEPTABLE_ERROR 	= 10e-6;
constexpr char CMD_LOAD_WEIGHTS[] 	= "LOAD-WEIGHTS";
constexpr char CMD_DUMP_WEIGHTS[] 	= "DUMP-WEIGHTS";
constexpr char CMD_RUN[]		= "RUN";
constexpr size_t ARGV_CMD		= 1;
constexpr size_t ARGV_DUMP_FILE 	= 3;
constexpr size_t ARGV_IN_FILE		= 2;
constexpr size_t INPUT_WIDTH		= 7;
constexpr size_t INPUT_HEIGHT		= 7;
constexpr size_t INPUT_COUNT		= INPUT_HEIGHT * INPUT_HEIGHT;
constexpr size_t CIRCLE_OUTPUT		= 0;
constexpr size_t SQUARE_OUTPUT		= 1;
constexpr size_t TRIANGLE_OUTPUT	= 2;
constexpr size_t EPOCH_LIMIT		= 1000;

const std::vector<double> CIRCLE_EXPECTED_OUTPUT   = { 1, 0, 0 };
const std::vector<double> SQUARE_EXPECTED_OUTPUT   = { 0, 1, 0 };
const std::vector<double> TRIANGLE_EXPECTED_OUTPUT = { 0, 0, 1 };

const std::vector<double> base_circle = {
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 1, 1, 0, 0,
	0, 1, 0, 0, 0, 1, 0,
	0, 1, 0, 0, 0, 1, 0,
	0, 1, 0, 0, 0, 1, 0,
	0, 0, 1, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0,
};

const std::vector<double> base_square = {
	0, 0, 0, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 0,
	0, 1, 0, 0, 0, 1, 0,
	0, 1, 0, 0, 0, 1, 0,
	0, 1, 0, 0, 0, 1, 0,
	0, 1, 1, 1, 1, 1, 0,
	0, 0, 0, 0, 0, 0, 0,
};

const std::vector<double> base_triangle = {
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0,
	0, 0, 1, 0, 1, 0, 0,
	0, 1, 0, 0, 0, 1, 0,
	1, 0, 0, 0, 0, 0, 1,
	1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0,
};

std::ostream& PrintOutputs(std::ostream& out, const std::vector<double>& outs) {
	bool is_first = true;
	for (double outval : outs) {
		if (!is_first)
			out << '\n';
		is_first = false;
		out << "Output value: " << outval;
	}
	return out;
}

char ToUpper(char c) {
	return std::toupper(c);
}

void PrintUsage() {
	std::cout
	<< 	"Usage: ./netzwerk [cmd] args...\n"
	<< 	"Commands:\n"
	<< 	"\t dump-weights [input_file] [dump_filename] - dumps weights "
	<<	"into a file.\n"
	<< 	"\t load-weights - [input_file] [load_filename] - loads weights "
	<<	"from the file and skips learning.\n"
	<<	"\t run [input_file] - just run the program. The network will learn "
	<<	"without dumping its weights.\n"
	;
}

int main(int argc, char **argv) {
	using namespace netz;

	if (argc < 3) {
		PrintUsage();
		return 0;
	}

	std::string cmd(argv[ARGV_CMD]);
	std::transform(cmd.begin(), cmd.end(), cmd.begin(), ToUpper);

	bool dump_weights = false;
	bool load_weights = false;

	std::ofstream dump_out;
	std::ifstream in_file;
	std::ifstream dump_in;

	if (cmd == CMD_DUMP_WEIGHTS) {
		if (argc < 3) {
			PrintUsage();
			return 0;
		}

		dump_weights = true;

		dump_out.open(argv[ARGV_DUMP_FILE]);

		if (!dump_out) {
			std::cerr 	<< "Could not open file "
					<< argv[ARGV_DUMP_FILE] << std::endl;
			return 1;
		}

	} else if (cmd == CMD_LOAD_WEIGHTS) {
		if (argc < 3) {
			PrintUsage();
			return 0;
		}

		load_weights = true;

		dump_in.open(argv[ARGV_DUMP_FILE]);

		if (!dump_in) {
			std::cerr 	<< "Could not open file "
					<< argv[ARGV_DUMP_FILE] << std::endl;
			return 1;
		}
	} else if (cmd != CMD_RUN) {
		std::cerr 	<< "Unknown command.\n";
		return 1;
	}

	in_file.open(argv[ARGV_IN_FILE]);

	if (!in_file) {
		std::cerr 	<< "Could not open file "
				<< argv[ARGV_IN_FILE] << std::endl;
		return 1;
	}

	std::vector<double> input;

	while (!in_file.eof() && (input.size() < INPUT_COUNT)) {
		char c;
		in_file >> c;

		if (c == '1' || c == '0') {
			input.push_back(static_cast<double>(c - '0'));
		}
	}

	if (input.size() != INPUT_COUNT) {
		std::cerr 	<< "Warning, input size is "
				<< input.size() << std::endl;
	}

	Netzwerk netz;

	for (size_t i = 0; i < INPUT_COUNT; i++) {
		netz.AddInput(0);
	}

	netz	.AddLayer(3 * 2)
		.AddLayer(3 * 2)
		.AddLayer(3 * 2)
		.AddLayer(3 * 2)
		.AddLayer(3);

	double alpha = 0.2;

	if (load_weights) {
		netz.ReadWeights(dump_in);
		goto skip;
	}

	for (size_t epoch = 0; epoch < EPOCH_LIMIT; epoch++) {
		std::cout << "Epoch: " << epoch << std::endl;
		for (const auto& figure : circle_bitmaps) {
			for (size_t i = 0; i < INPUT_COUNT; i++) {
				netz.SetInput(
					i, static_cast<double>(figure.at(i)));
			}

			std::vector<double> outputs = netz.GetOuputs();

			double error = math::ErrorEstimation(
				CIRCLE_EXPECTED_OUTPUT,
				outputs
			);

			if (std::abs(error) > ACCEPTABLE_ERROR) {
				netz.AdjustWeights(alpha,
					CIRCLE_EXPECTED_OUTPUT);
			}
		}

		for (const auto& figure : square_bitmaps) {
			for (size_t i = 0; i < INPUT_COUNT; i++) {
				netz.SetInput(
					i, static_cast<double>(figure.at(i)));
			}

			std::vector<double> outputs = netz.GetOuputs();

			double error = math::ErrorEstimation(
				SQUARE_EXPECTED_OUTPUT,
				outputs
			);

			if (std::abs(error) > ACCEPTABLE_ERROR) {
				netz.AdjustWeights(alpha,
					SQUARE_EXPECTED_OUTPUT);
			}
		}

		for (const auto& figure : triangle_bitmaps) {
			for (size_t i = 0; i < INPUT_COUNT; i++) {
				netz.SetInput(
					i, static_cast<double>(figure.at(i)));
			}

			std::vector<double> outputs = netz.GetOuputs();

			double error = math::ErrorEstimation(
				TRIANGLE_EXPECTED_OUTPUT,
				outputs
			);

			if (std::abs(error) > ACCEPTABLE_ERROR) {
				netz.AdjustWeights(alpha,
					TRIANGLE_EXPECTED_OUTPUT);
			}
		}
	}

skip:

	for (size_t i = 0; i < INPUT_COUNT; i++) {
		netz.SetInput(i, static_cast<double>(input.at(i)));
	}

	std::vector<double> outputs = netz.GetOuputs();

	PrintOutputs(std::cout, outputs) << std::endl;

	auto max_iter = std::max_element(outputs.begin(), outputs.end());

	switch (max_iter - outputs.begin()) {
		case CIRCLE_OUTPUT:
			std::cout << "circle" << std::endl;
			break;
		case SQUARE_OUTPUT:
			std::cout << "square" << std::endl;
			break;
		case TRIANGLE_OUTPUT:
			std::cout << "triangle" << std::endl;
			break;
	}

	if (dump_weights) {
		netz.DumpWeights(dump_out);
	}

	return 0;
}