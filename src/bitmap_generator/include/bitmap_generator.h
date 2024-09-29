#pragma once
#include <map>
#include <unordered_map>
#include <array>
#include <ostream>

namespace bitmap_generator {

enum FigureType {
	Circle,
	Square,
	Triangle,
	FigureCount,
};

constexpr int SAMPLE_COUNT = 128;
constexpr int SAMPLE_RES_WIDTH = 7;
constexpr int SAMPLE_RES_HEIGHT = 7;

using Bitmap = std::array<bool, SAMPLE_RES_WIDTH * SAMPLE_RES_HEIGHT>;

extern const Bitmap base_circle;
extern const Bitmap base_square;
extern const Bitmap base_triangle;

extern const std::array<Bitmap, SAMPLE_COUNT> circle_bitmaps;
extern const std::array<Bitmap, SAMPLE_COUNT> square_bitmaps;
extern const std::array<Bitmap, SAMPLE_COUNT> triangle_bitmaps;

/// @brief	Генерирует новую битовую карту на основе базовой. С вероятностью
/// 		равной noise_percent каждый бит может быть инвертирован, искажая
/// 		тем самым картинку.
/// @param	base_bitmap - битовая карта, которая будет зашумлена.
/// @param	noise_percent - вероятность, с которой каждый бит будет
/// 		инвертирован.
/// @return	Новая, "зашумленная" картинка.
Bitmap GenerateNoisyBitmap(Bitmap base_bitmap, double noise_percent);

/// @brief	Генерирует исходный код массива с битовыми картами для обучения
/// 		нейросети.
/// @param	out - поток вывода исходного кода.
/// @param	array_def - определение массива. Например:
/// 			"const std::array<Bitmap, 128> bitmaps"
/// @param	base_bitmap - битовая карта, которая будет зашумлена.
/// @param	noise_percent - вероятность, с которой каждый бит будет
/// 		инвертирован (см. GenerateNoisyBitmap).
/// @return	Поток вывода.
std::ostream& GenerateBitmapArraySourceCode(std::ostream& out,
		const std::string& array_def,
		Bitmap base_bitmap,
		double noise_percent);

std::ostream& operator<<(std::ostream& out, const Bitmap& bitmap);

} // namespace bitmap_generator
