#include <array>
#include <byteswap.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>

// Function declarations
void print_check(bool check, const std::string& message);
bool option(const std::string& question);

const std::unordered_map<unsigned int, std::string> compression_methods = {
	{ 0, "BI_RGB" },
	{ 1, "BI_RLE8" },
	{ 2, "BI_RLE4" },
	{ 3, "BI_BITFIELDS" },
	{ 4, "BI_JPEG" },
	{ 5, "BI_PNG" },
	{ 6, "BI_ALPHABITFIELDS" },
	{ 11, "BI_CMYK" },
	{ 12, "BI_CMYKRLE8" },
	{ 13, "BI_CMYKRLE4" }
};

template <size_t N>
bool compare_bytes(const std::array<unsigned char, N>& bytes, const size_t offset, std::fstream& file)
{
	file.seekp(offset, std::ios::beg);

	for (unsigned char byte : bytes)
	{
		unsigned char read_byte;
		file.read((char*) &read_byte, sizeof(unsigned char));
		if (byte != read_byte)
			return false;
	}

	return true;
}

template <typename T>
T read_bytes(std::fstream& file, const size_t offset)
{
	file.seekp(offset, std::ios::beg);

	T data;
	file.read((char*) &data, sizeof(T));

	return data;
}

template <typename T>
void write_bytes(std::fstream& file, const size_t offset, T value)
{
	std::cout << "Writing 0x" << std::hex << value << " to 0x" << offset << "\n";
	file.seekp(offset, std::ios::beg);
	file.write((char*) &value, sizeof(T));
}

void print_check(bool check, const std::string& message)
{
	if (check)
		std::cout << "[🗸] " << message << "\n";
	else
		std::cout << "[ ] " << message << "\n";
}

template <typename T>
void print_value_with_hex(const std::string& text, const T value)
{
	std::cout << text << ": " << std::dec << value << " (" << std::hex << value << ")\n";
}

template <typename T>
T ask_for_value()
{
	std::cout << "Value: ";
	T value;
	std::cin >> value;
	return value;
}

bool option(const std::string& question)
{
	std::cout << question << " (y/N): ";
	std::string answer;
	std::cin >> answer;

	return answer == "y" || answer == "Y";
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cout << "Usage: bmp-patch ./path_to_bmp_file\n";
		exit(1);
	}

	const std::string file_path = argv[1];

	if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path))
	{
		std::cout << "File doesn't exist or its not a file: " << file_path << "\n";
		return 1;
	}

	std::fstream file(argv[1], std::ios::in | std::ios::binary | std::ios::out);

	// Check the file type
	constexpr std::array<unsigned char, 2> bmp_file_sig = {0x42, 0x4D};
	print_check(compare_bytes(bmp_file_sig, 0, file), "File signature");

	// Check the file size
	const unsigned int reported_file_size = read_bytes<unsigned int>(file, 2);

	file.seekg(0, std::ios::end);
	const unsigned int real_file_size = file.tellg();

	print_check(reported_file_size == real_file_size, "File size");
	if (reported_file_size != real_file_size)
	{
		std::cout << "Reported file size: " << reported_file_size << "\n";
		std::cout << "Real file size: " << real_file_size << "\n";

		if (option("Patch the file size"))
			write_bytes<unsigned int>(file, 2, real_file_size);
	}

	// Get the starting point of data
	std::cout << "Data starts at address: " << std::hex << bswap_32(read_bytes<unsigned int>(file, 10)) << "\n";

	// Validate the header size
	const unsigned int header_size = read_bytes<unsigned int>(file, 0x0E);
	constexpr std::array<unsigned int, 8> valid_header_sizes = {
		12, 64, 16, 40, 52, 56, 108, 124
	};

	bool valid_header_size = false;
	for (unsigned int size : valid_header_sizes)
	{
		if (size == header_size)
		{
			valid_header_size = true;
			break;
		}
	}
	print_check(valid_header_size, "Header size valid");
	std::cout << "Header size: " << std::dec << header_size << "\n";

	if (!valid_header_size && option("Patch the header size to a known good value of 40"))
	{
		write_bytes<unsigned int>(file, 0x0E, 40); // Header size
		write_bytes<unsigned int>(file, 0x0A, 36); // Data offset
	}

	enum header_type
	{
		BITMAPCOREHEADER, BITMAPINFOHEADER, UNKNOWN
	};

	header_type header_type = header_type::UNKNOWN;

	std::cout << "\n# Information with BITMAPCOREHEADER header offsets:\n";

	{
		// Get the dimensions of the bitmap
		const unsigned short bitmap_width = read_bytes<unsigned short>(file, 0x12);
		const unsigned short bitmap_height = read_bytes<unsigned short>(file, 0x14);

		print_value_with_hex("Bitmap width", bitmap_width);
		print_value_with_hex("Bitmap height", bitmap_height);

		// Check the number of color planes
		const unsigned short color_plane_count = read_bytes<unsigned short>(file, 0x16);
		print_check(color_plane_count == 1, "Color plane count == 1");
		if (color_plane_count != 1)
			print_value_with_hex("Color plane count", color_plane_count);
		else
			header_type = header_type::BITMAPCOREHEADER;

		// Get the number of bits per pixel
		const unsigned short bits_per_pixel = read_bytes<unsigned short>(file, 0x18);
		print_value_with_hex("Bits per pixel", bits_per_pixel);
	}

	std::cout << "\n# Information with BITMAPINFOHEADER header offsets:\n";

	{
		const int bitmap_width = read_bytes<int>(file, 0x12);
		const int bitmap_height = read_bytes<int>(file, 0x16);

		print_value_with_hex("Bitmap width", bitmap_width);
		print_value_with_hex("Bitmap height", bitmap_height);

		const unsigned short color_plane_count = read_bytes<unsigned short>(file, 0x1A);
		print_check(color_plane_count == 1, "Color plane count == 1");
		if (color_plane_count != 1)
			print_value_with_hex("Color plane count", color_plane_count);
		else
			header_type = header_type::BITMAPINFOHEADER;

		const unsigned short bits_per_pixel = read_bytes<unsigned short>(file, 0x1C);
		bool valid_bits_per_pixel = bits_per_pixel == 1 ||
			bits_per_pixel == 4 ||
			bits_per_pixel == 8 ||
			bits_per_pixel == 16 ||
			bits_per_pixel == 24 ||
			bits_per_pixel == 32;

		print_check(valid_bits_per_pixel, "Bits per pixel");
		if (!valid_bits_per_pixel)
			print_value_with_hex("Bits per pixel", bits_per_pixel);

		const unsigned int compression_method = read_bytes<unsigned int>(file, 0x1E);
		print_check(compression_methods.contains(compression_method), "Known compression method");
		if (compression_methods.contains(compression_method))
			std::cout << "Compression method: " << compression_methods.at(compression_method) << "\n";
		else
			print_value_with_hex("Compression method", compression_method);

		const unsigned int image_size = read_bytes<unsigned int>(file, 0x22);
		print_value_with_hex("Image size", image_size); // Zero is a valid value

		const int horizontal_resolution = read_bytes<int>(file, 0x26);
		const int vertical_resolution = read_bytes<int>(file, 0x2A);

		print_value_with_hex("Horizontal resolution", horizontal_resolution);
		print_value_with_hex("Vertical resolution", vertical_resolution);

		const unsigned int colors_in_color_palette = read_bytes<unsigned int>(file, 0x2E);
		print_value_with_hex("Colors in color palette", colors_in_color_palette);

		const unsigned int important_color_count = read_bytes<unsigned int>(file, 0x32);
		print_value_with_hex("Number of important colors", important_color_count);
	}

	std::cout << "\nWith a guess based on color plane count, the header type is ";
	switch (header_type)
	{
		case BITMAPCOREHEADER:
			std::cout << "BITMAPCOREHEADER\n";
			break;

		case BITMAPINFOHEADER:
			std::cout << "BITMAPINFOHEADER\n";
			break;

		case UNKNOWN:
			std::cout << "unknown...";
			return 0;
	}

	std::cout << "# Patching #\n";
	std::cout << "0) Cancel\n";

	if (header_type == header_type::BITMAPCOREHEADER)
	{
		std::cout	<< "1) Bitmap width in pixels\n"
					<< "2) Bitmap height in pixels\n"
					<< "3) Number of bits per pixel\n"
					<< "4) Header size (also updates the data offset)\n";

		while (true)
		{
			char answer;
			std::cout << " > ";
			std::cin >> answer;

			switch (answer)
			{
				case '1':
					write_bytes<unsigned short>(file, 0x12, ask_for_value<unsigned short>());
					break;

				case '2':
					write_bytes<unsigned short>(file, 0x14, ask_for_value<unsigned short>());
					break;

				case '3':
					write_bytes<unsigned short>(file, 0x18, ask_for_value<unsigned short>());
					break;

				case '4':
				{
					unsigned int value = ask_for_value<unsigned int>();
					write_bytes<unsigned int>(file, 0x0E, value); // Header size
					write_bytes<unsigned int>(file, 0x0A, value - 4); // Data offset
					break;
				}

				case '0':
				default:
					break;
			}
		}
	}

	if (header_type == header_type::BITMAPINFOHEADER)
	{
		std::cout	<< "1) Bitmap width in pixels\n"
					<< "2) Bitmap height in pixels\n"
					<< "3) Number of bits per pixel (valid values: 1, 4, 8, 16, 24 and 32)\n"
					<< "4) Compression method (valid values: 0-6, 11-13)\n"
					<< "5) Image size (with BI_RGB compression can be 0)\n"
					<< "6) Horizontal resolution (pixel per metre)\n"
					<< "7) Vertical resolution (pixel per metre)\n"
					<< "8) Number of colors in the color palette\n"
					<< "9) Number of important colors used\n"
					<< "A) Header size (also updates the data offset)\n"
					<< "B) Data offset\n";

		while (true)
		{
			char answer;
			std::cout << " > ";
			std::cin >> answer;

			switch (answer)
			{
				case '1':
					write_bytes<int>(file, 0x12, ask_for_value<int>());
					break;

				case '2':
					write_bytes<int>(file, 0x16, ask_for_value<int>());
					break;

				case '3':
					write_bytes<unsigned short>(file, 0x1C, ask_for_value<unsigned short>());
					break;

				case '4':
					write_bytes<unsigned int>(file, 0x1E, ask_for_value<unsigned int>());
					break;

				case '5':
					write_bytes<unsigned int>(file, 0x22, ask_for_value<unsigned int>());
					break;

				case '6':
					write_bytes<int>(file, 0x26, ask_for_value<int>());
					break;

				case '7':
					write_bytes<int>(file, 0x2A, ask_for_value<int>());
					break;

				case '8':
					write_bytes<unsigned int>(file, 0x2E, ask_for_value<unsigned int>());
					break;

				case '9':
					write_bytes<unsigned int>(file, 0x32, ask_for_value<unsigned int>());
					break;

				case 'A':
					{
						unsigned int value = ask_for_value<unsigned int>();
						write_bytes<unsigned int>(file, 0x0E, value); // Header size
						write_bytes<unsigned int>(file, 0x0A, value - 4); // Data offset
						break;
					}

				case 'B':
					write_bytes<unsigned int>(file, 0x0A, ask_for_value<unsigned int>()); // Data offset
					break;

				case '0':
				default:
					return 0;
			}
		}
	}

	return 0;
}
