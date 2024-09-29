#include "bitmap_generator.h"
#include <iostream>
#include <fstream>
#include <ostream>

const std::string circle_vector_name =
	"const std::vector<std::vector<int>> circle_bitmaps";

const std::string square_vector_name =
	"const std::vector<std::vector<int>> square_bitmaps";

const std::string triangle_vector_name =
	"const std::vector<std::vector<int>> triangle_bitmaps";

int main(int argc, char **argv) {
	using namespace bitmap_generator;

	if (argc < 3) {
		std::cout << "Usage: ./bitmapgen [output.cpp] [output.hpp]\n";
		return 0;
	}

	std::ofstream fout_cpp(argv[1]);
	std::ofstream fout_hpp(argv[2]);

	fout_cpp << "#include \"" << argv[2] << "\"" << std::endl;

	fout_cpp << std::endl;
	GenerateBitmapArraySourceCode(fout_cpp,
			circle_vector_name,
			base_circle, 0.039) << std::endl;

	fout_cpp << std::endl;
	GenerateBitmapArraySourceCode(fout_cpp,
			square_vector_name,
			base_square, 0.039) << std::endl;

	fout_cpp << std::endl;
	GenerateBitmapArraySourceCode(fout_cpp,
			triangle_vector_name,
			base_triangle, 0.039) << std::endl;

	fout_cpp << std::endl;

	fout_hpp << "#pragma once" << std::endl;
	fout_hpp << "#include <vector>" << std::endl;
	fout_hpp << std::endl;

	fout_hpp << "extern " << circle_vector_name << ";" << std::endl;
	fout_hpp << "extern " << square_vector_name << ";" << std::endl;
	fout_hpp << "extern " << triangle_vector_name << ";" << std::endl;
	return 0;
}