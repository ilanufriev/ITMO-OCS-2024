#include "bitmap_generator.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <ostream>

const std::string circle_vector_name =
	"const std::vector<std::vector<int>> circle_bitmaps";

const std::string square_vector_name =
	"const std::vector<std::vector<int>> square_bitmaps";

const std::string triangle_vector_name =
	"const std::vector<std::vector<int>> triangle_bitmaps";

constexpr size_t ARGV_CMD 		= 1;
constexpr double NOISINESS_COEFF	= 0.10;

int GenerateCppFilesMain(int argc, char **argv) {
	using namespace bitmap_generator;

	if (argc < 4) {
		std::cout << "Usage: ./bitmapgen cpp [output.cpp] [output.hpp]\n";
		return 0;
	}

	std::ofstream fout_cpp(argv[2]);
	std::ofstream fout_hpp(argv[3]);

	fout_cpp << "#include \"" << argv[3] << "\"" << std::endl;

	fout_cpp << std::endl;
	GenerateBitmapArraySourceCode(fout_cpp,
			circle_vector_name,
			base_circle, NOISINESS_COEFF) << std::endl;

	fout_cpp << std::endl;
	GenerateBitmapArraySourceCode(fout_cpp,
			square_vector_name,
			base_square, NOISINESS_COEFF) << std::endl;

	fout_cpp << std::endl;
	GenerateBitmapArraySourceCode(fout_cpp,
			triangle_vector_name,
			base_triangle, NOISINESS_COEFF) << std::endl;

	fout_cpp << std::endl;

	fout_hpp << "#pragma once" << std::endl;
	fout_hpp << "#include <vector>" << std::endl;
	fout_hpp << std::endl;

	fout_hpp << "extern " << circle_vector_name << ";" << std::endl;
	fout_hpp << "extern " << square_vector_name << ";" << std::endl;
	fout_hpp << "extern " << triangle_vector_name << ";" << std::endl;
	return 0;
}

constexpr size_t ARGV_AMOUNT 		= 3;
constexpr size_t ARGV_OUT_PATH 		= 2;

enum class FigureType {
	CIRCLE,
	SQUARE,
	TRIANGLE,
};

void GenerateFiguresIntoFiles(FigureType type, const std::string& output_path,
		size_t amount) {
	using namespace bitmap_generator;

	const std::string fname_template = type == FigureType::CIRCLE ? "C"
			: type == FigureType::SQUARE ? "S"
			: type == FigureType::TRIANGLE ? "T" : "U";

	const Bitmap *base_bitmap = type == FigureType::CIRCLE ? &base_circle
			: type == FigureType::SQUARE ? &base_square
			: type == FigureType::TRIANGLE ? &base_triangle
			: nullptr;

	if (!base_bitmap) {
		std::cerr << "Could not find base bitmap\n";
		return;
	}

	for (size_t i = 0; i < amount; i++) {
		std::string filename = output_path + "/"
				+ fname_template + std::to_string(i + 1)
				+ ".txt";

		std::ofstream fout(filename);

		if (!fout) {
			std::cerr 	<< "Could not create file "
					<< filename << std::endl;
			continue;
		}

		Bitmap bm = GenerateNoisyBitmap(*base_bitmap, NOISINESS_COEFF);

		for (size_t h = 0; h < SAMPLE_RES_HEIGHT; h++) {
			for (size_t w = 0; w < SAMPLE_RES_WIDTH; w++) {
				fout << (bm.at(w + h * SAMPLE_RES_WIDTH) ? 1
						: 0);
			}
			fout << '\n';
		}

		std::cout << "File " << filename << " created.\n";
	}
}

int GenerateIntoFilesMain(int argc, char **argv) {
	using namespace bitmap_generator;

	if (argc < 4) {
		std::cout << "Usage: ./bitmapgen files [output_path] [amount]\n";
		return 0;
	}

	size_t amount = std::stoi(argv[ARGV_AMOUNT]);
	const std::string output_path(argv[ARGV_OUT_PATH]);

	GenerateFiguresIntoFiles(FigureType::CIRCLE, output_path, amount);
	GenerateFiguresIntoFiles(FigureType::SQUARE, output_path, amount);
	GenerateFiguresIntoFiles(FigureType::TRIANGLE, output_path, amount);

	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "Usage:\t\t./bitmapgen files [output_path] [amount]\n";
		std::cout << "Or:\t\t./bitmapgen cpp [output.cpp] [output.hpp]\n";
		return 0;
	}

	if (std::strcmp(argv[ARGV_CMD], "files") == 0) {
		return GenerateIntoFilesMain(argc, argv);
	} else if (std::strcmp(argv[ARGV_CMD], "cpp") == 0) {
		return GenerateCppFilesMain(argc, argv);
	}

	std::cout << "Unknown command.\n";

	return 0;
}