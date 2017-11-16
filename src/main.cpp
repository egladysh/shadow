#include <cstdlib>
#include <iostream>
#include <ostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <png.hpp>

class pixel_generator
    : public png::generator< png::gray_pixel_1, pixel_generator >
{
public:
    pixel_generator(size_t width, size_t height)
        : png::generator< png::gray_pixel_1, pixel_generator >(width, height),
          m_row(width)
    {
        for (size_t i = 0; i < m_row.size(); ++i)
        {
            m_row[i] = 1; //i > m_row.size() / 2 ? 1 : 0;
        }
    }

    png::byte* get_next_row(size_t /*pos*/)
    {
        size_t i = std::rand() % m_row.size();
        size_t j = std::rand() % m_row.size();
        png::gray_pixel_1 t = m_row[i];
        m_row[i] = m_row[j];
        m_row[j] = t;
        return reinterpret_cast< png::byte* >(row_traits::get_data(m_row));
    }

private:
    typedef png::packed_pixel_row< png::gray_pixel_1 > row;
	typedef png::row_traits< row > row_traits;
	row m_row;
};

namespace 
{
	void usage_help()
	{
		std::cout << "Usage:" << std::endl;
		std::cout << "shadow_match key_png_file png_file [png_files...]" << std::endl;
	}
}

std::pair<bool,bool> process_pixel(png::image<png::rgb_pixel>& img, int x, int y, int cnt)
{
	auto r = img.get_row(y);
	assert(r[x].red == r[x].green && r[x].green == r[x].blue);

	int xs = std::max(0, x-1);
	int ys = std::max(0, y-1);

	int xe = std::min(x+2, (int)img.get_width());
	int ye = std::min(y+2, (int)img.get_height());


	int pc = 0;
	unsigned int ac = 0;
	for (int yn = ys; yn != ye; ++yn) {
		for (int xn = xs; xn != xe; ++xn) {
			auto p = img.get_pixel(xn, yn);
			if (p.red != 255) {
				ac += p.red;
				++pc;
			}
		}
	}

	assert(pc);
	auto c = ac/pc;


	//get the pixel color
	//auto c = r[x].red;

	assert(c != 255);

	//set color
	unsigned char sc = c+4 < 254? c+4 : 254;

	bool donex = false;
	bool doney = false;
	for (int yn = ys; yn != ye; ++yn) {
		for (int xn = xs; xn != xe; ++xn) {
			if (img.get_pixel(xn, yn).red == 255) {
				img.set_pixel(xn, yn, {sc, sc, sc});
				if (xn > x)
					donex = true;
				if (yn > y)
					doney = true;
			}
		}
	}
	return {donex, doney};
}

//return false if nothing is left to do
bool shadow_pass(png::image<png::rgb_pixel>& img, int cnt)
{
	bool blank_found = false;
	bool pixel_found = false; 
	unsigned int w = img.get_width();
	unsigned int h = img.get_height();
	for (unsigned int y = 0; y < h; ++y) {
		const auto r = img.get_row(y);
		bool skip = false;
		for (unsigned int x = 0; x < w; ++x) {
			auto p = img.get_pixel(x, y);
			if (p.red == 255 && p.green == 255 && p.blue == 255) {
				blank_found = true;
			}
			else {
				pixel_found = true;
				auto d = process_pixel(img, x, y, cnt);
				if (d.first) {
					++x;
				}
				if (d.second) {
					skip = true;
				}
			}
		}
		if (skip) {
			++y;
		}
	}
	return blank_found && pixel_found;
}

void make_shadow(png::image<png::rgb_pixel>& img)
{
	for (int cnt = 0; true; ++cnt) {
		if (!shadow_pass(img, cnt))
			break;
	}
}

std::vector<int> diff_slices(const png::image<png::rgb_pixel>& img1, const png::image<png::rgb_pixel>& img2, int offsetx, int offsety, int slice_size)
{
	if (!(img1.get_width() == img2.get_width() && img1.get_height() == img2.get_height())) {
		throw std::runtime_error("the image sizes must be the same");
	}

	auto w = img1.get_width() - offsetx;
	auto h = img1.get_height() - offsety;

	int upx = offsetx + slice_size;
	int upy = offsety + slice_size;

	int bx = w - slice_size;
	int by = h - slice_size;

	std::vector<int> v;

	for (auto x = offsetx; x != w; ++x) {
		for( auto y = offsety; y != h; ++y) {
			if ((x >= upx && x < bx) && (y >= upy && y < by))
				continue;
			int d = static_cast<int>(img1.get_pixel(x,y).red) - static_cast<int>(img2.get_pixel(x,y).red);
			v.push_back(d);
		}
	}

	return v;
} 
std::vector<int> diff_images(const png::image<png::rgb_pixel>& img1, const png::image<png::rgb_pixel>& img2, int offsetx, int offsety)
{
	if (!(img1.get_width() == img2.get_width() && img1.get_height() == img2.get_height())) {
		throw std::runtime_error("the image sizes must be the same");
	}

	auto w = img1.get_width() + offsetx;
	auto h = img1.get_height() + offsety;


	std::vector<int> v;

	for (auto x = offsetx; x != w; ++x) {
		if (x >= img1.get_width()) {
			for( auto y = offsety; y != h; ++y) {
				v.push_back(-1);
			}
		}
		else
		{
			for( auto y = offsety; y != h; ++y) {
				if (y >= img1.get_height()) {
					v.push_back(-1);
				}
				else {
					int d = static_cast<int>(img1.get_pixel(x,y).red) - static_cast<int>(img2.get_pixel(x,y).red);
					v.push_back(d);
				}
			}
		}
	}
	return v;
} 


template<typename T>
float calc_mean(const std::vector<T>& v)
{
	float mean = .0;

	for (auto n: v) {
		mean += n;
	}

	return mean / v.size();
}


template<typename T>
std::pair<float,float> calc_deviation(const std::vector<T>& v)
{
	//find the mean
	float mean = calc_mean<T>(v);

	//deviation
	float d = .0;

	for (auto n: v) {
		float x = (float)n - mean;
		d += x*x;
	}

	d /= v.size();

	return {mean, std::sqrt(d)};
}

float check_randomness(const std::vector<int>& v)
{
	int max = 0;
	int min = 10000000;
	for (auto n: v) {
		if (n > max)
			max = n;
		if (n < min)
			min = n;
	}
	if (!min && !max)
		return .0;
	
	int rnd = 0;
	for (auto n: v) {
		float r = (float)std::rand()/(float)RAND_MAX;
		float x = (float)(n-min)/(float)(max-min);
		if (r < 0.5 && x < 0.5)
			++rnd;
	}

	return (float)rnd/(float)v.size();

}

void score_image(const png::image<png::rgb_pixel>& key, png::image<png::rgb_pixel>& img)
{
	make_shadow(img);


	int slice_width = key.get_width()/10;
	if (!slice_width)
	  slice_width = key.get_width();

	std::cout << "slice width: " << slice_width << std::endl;

	std::vector<float> deviations;

	for (int i = 0; i != 3; ++i) {
		int x = 0; int y = 0;
		if (i == 0) {
			x = 0;
			y = 0;
		}
		else if (i == 1) {
			x = key.get_width()/3 - slice_width;
			if (x <= 0) continue;
			if (x + 2*slice_width >= key.get_width()/2)
				continue;
			y = x;
		}
		else if (i == 2) {
			x = key.get_width()/2 - slice_width;
			y = x;
		}
		else {
			continue;
		}
		auto d = diff_slices(key, img, x, x, slice_width);
		auto r =  calc_deviation<int>(d);
		std::cout << "(" << x << "," << y << ")" <<  " mean=" << r.first << " div=" << r.second << std::endl;
	}
}

int main(int argc, char* argv[])
try
{ 
	std::srand(std::time(0)); 

	if (argc < 3) {
		usage_help();
		return 1;
	}
	
	std::string keyfile;
	std::vector<const char*> files;

	for (int i = 1; i < argc; ++i) {
		const char* s = argv[i];
		if (i == 1) {
			keyfile = std::string(argv[i]);
		}
		else {
			files.push_back(argv[i]);
		}
	}
	png::image<png::rgb_pixel> key_image(keyfile);
	assert(key_image.get_width() == key_image.get_height());

	make_shadow(key_image);
	key_image.write("key_shadow.png");

	for (auto file: files) {
		png::image<png::rgb_pixel> image(file);
		std::cout << "(" << file << ")  ";
		assert(image.get_width() == image.get_height() && key_image.get_width() == image.get_width());
		score_image(key_image, image);
		image.write(std::string("shadow_")+file);
	}
}
catch (std::exception const& error)
{
    std::cerr << "shadow_match: " << error.what() << std::endl;
    return EXIT_FAILURE;
}
