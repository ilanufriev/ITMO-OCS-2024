#include "bitmap_generator.h"
#include <ostream>
#include <random>
#include <iostream>

namespace bitmap_generator {

std::random_device rd;
std::mt19937 mt_gen(rd());
std::uniform_real_distribution<double> distribution(0.0, 1.0);

constexpr double EPSYLON = 10e-6;

const Bitmap base_circle = {
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 0, 0,
    0, 1, 0, 0, 0, 1, 0,
    0, 1, 0, 0, 0, 1, 0,
    0, 1, 0, 0, 0, 1, 0,
    0, 0, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
};

const Bitmap base_square = {
    0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 0,
    0, 1, 0, 0, 0, 1, 0,
    0, 1, 0, 0, 0, 1, 0,
    0, 1, 0, 0, 0, 1, 0,
    0, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0,
};

const Bitmap base_triangle = {
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0,
    0, 0, 1, 0, 1, 0, 0,
    0, 1, 0, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0,
};

template<typename FloatingPointType>
bool IsLess(const FloatingPointType lhs, const FloatingPointType rhs) {
    return ((rhs - lhs) > 0) && (std::abs(rhs - lhs) > EPSYLON);
}

Bitmap GenerateNoisyBitmap(Bitmap base_bitmap, double noise_percent) {
    for (size_t y = 0; y < SAMPLE_RES_HEIGHT; y++) {
        for (size_t x = 0; x < SAMPLE_RES_WIDTH; x++) {
            if (IsLess(distribution(mt_gen), noise_percent))
                base_bitmap[x + y + SAMPLE_RES_WIDTH]
                    = !base_bitmap[x + y * SAMPLE_RES_WIDTH];
        }
    }

    return base_bitmap;
}

std::ostream& GenerateBitmapArraySourceCode(std::ostream& out,
        const std::string& array_def,
        Bitmap base_bitmap,
        double noise_percent) {
    out << array_def << " = {" << std::endl;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        Bitmap bitmap = GenerateNoisyBitmap(base_bitmap, noise_percent);
        out << "\t{" << std::endl;

        bool is_first_endl = true;
        for (size_t y = 0; y < SAMPLE_RES_HEIGHT; y++) {

            if (!is_first_endl)
                out << std::endl;

            is_first_endl = false;

            bool is_first_comma = true;
            for (size_t x = 0; x < SAMPLE_RES_WIDTH; x++) {
                if (!is_first_comma)
                    out << ", ";
                else
                    out << "\t\t";

                is_first_comma = false;

                out << static_cast<int>(
                    bitmap.at(x + y * SAMPLE_RES_WIDTH));
            }

            out << ',';

        }

        out << std::endl << "\t}," << std::endl;
    }

    out << "};" << std::endl;

    return out;
}

std::ostream& operator<<(std::ostream& out, const Bitmap& bitmap) {
    bool is_first = true;
    for (size_t y = 0; y < bitmap.size(); y++) {
        if (!is_first)
            out << std::endl;

        is_first = false;

        for (size_t x = 0; x < SAMPLE_RES_WIDTH; x++) {
            out << (bitmap.at(y * SAMPLE_RES_WIDTH + x) ? '#' : ' ');
        }
    }
    return out;
}

} // namespace bitmap_generator