#include "ImageManagment.h"
#include <GLFW/glfw3.h>
#include "App.h"
std::mutex ImageManagment::instanceMutex;
std::mutex ImageManagment::imagesMutex;
std::mutex ImageManagment::reloadImagesMutex;
std::mutex ImageManagment::shouldOpenImageMutex;

ImageManagment* ImageManagment::instance = nullptr;
std::vector<std::string> ImageManagment::imageExtensions = {
	".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tiff", ".webp"
};
ImageManagment::ImageManagment() {
	images.reserve(10);
	selectedIndex = -1;
}

ImageManagment::~ImageManagment()
{
	shouldRunManagment = false;
	clearImages();
}

int ImageManagment::loadImages(std::string imagePath)
{
	if (imagePath.empty())
		return 0;
	clearImages();
	shouldOpenImageMutex.lock();
	shouldOpenImage = false;
	shouldOpenImageMutex.unlock();

	if (!fs::exists(imagePath) || !fs::is_regular_file(imagePath)) {
		return -1;
	}
	imagesMutex.lock();
	currentPath = fs::path(imagePath);
	fs::path parrentPath = currentPath.parent_path();
	for (fs::path p : fs::directory_iterator(parrentPath)) {
		std::string extention = p.extension().string();

		std::transform(extention.begin(), extention.end(), extention.begin(), ::tolower);

		if (!std::any_of(
			imageExtensions.begin(),
			imageExtensions.end(),
			[&extention](const std::string& ext) { return ext == extention; }))
			continue;

		Image i = { -1, 0, 0, p.string()}; // TODO: Maybe don't copy the path so much

		if (p == currentPath) {
			selectedIndex = images.size();

		}
		images.push_back(i);
	}
	if (selectedIndex == -1) {
		selectedIndex = 0;
	}
	imagesMutex.unlock();
	loadCloseImages();

	return 1;
}

void ImageManagment::loadImage(Image* image) {
	if (image->texId != -1)
		return;

	int width, height, num_channels;
	stbi_uc* image_data = stbi_load(image->imagePath.c_str(), &width, &height, &num_channels, 0);

	if (!image_data) {
		return;
	}
	App::windowMutex.lock();
	glfwMakeContextCurrent(App::window);
	GLuint texture;
	glGenTextures(1, (&texture));
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// Set texture wrapping and filtering options
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Load image data into the texture
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, (void*)image_data);
	glfwMakeContextCurrent(nullptr);
	App::windowMutex.unlock();
	// Free the image data after loading it into the texture
	stbi_image_free(image_data);

	image->texId = texture;
	image->w = width;
	image->h = height;

}

void ImageManagment::unloadImage(Image* image)
{
	if (image->texId == -1)
		return;
	App::windowMutex.lock();
	glfwMakeContextCurrent(App::window);
	glDeleteTextures(1, &image->texId);
	image->texId = -1;
	image->w = image->h = 0;
	if (image->flipX)
		flipImageX(image);
	if (image->flipY)
		flipImageY(image);
	image->uv[0] = { 0, 0 };
	image->uv[1] = { 1, 0 };
	image->uv[2] = { 1, 1 };
	image->uv[3] = { 0, 1 };
	image->rotation = 0;
	glfwMakeContextCurrent(nullptr);
	App::windowMutex.unlock();
}

void ImageManagment::clearImages() {
	imagesMutex.lock();
	for (int i = 0; i < images.size(); i++) {
		unloadImage(&images[i]);
	}
	images.clear();
	imagesMutex.unlock();
}
void ImageManagment::deleteInstance()
{
	instance->shouldRunManagment = false;
	instanceMutex.lock();
	if (instance)
		delete instance;
	instanceMutex.unlock();
}
Image* ImageManagment::getImageAt(int i) {
	if (i < 0 && i >= images.size())
		return nullptr;
	return &images[i];
}
void ImageManagment::increaseZoom()
{
	zoom += (zoom-0.85f) / 10;
}
void ImageManagment::decreaseZoom()
{
	if(zoom > 0.85f)
		zoom -= (zoom - 0.85f) / 10;
}
void ImageManagment::rotateCurrentImage(int dir)
{

	dir = dir < 0 ? -1 : 1;
	images[selectedIndex].rotation = (images[selectedIndex].rotation + dir) % 4;
	if (dir < 0) {
		ImVec2 p = images[selectedIndex].uv[0];
		images[selectedIndex].uv[0] = images[selectedIndex].uv[1];
		images[selectedIndex].uv[1] = images[selectedIndex].uv[2];
		images[selectedIndex].uv[2] = images[selectedIndex].uv[3];
		images[selectedIndex].uv[3] = p;
	}
	else
	{
		ImVec2 p = images[selectedIndex].uv[3];
		images[selectedIndex].uv[3] = images[selectedIndex].uv[2];
		images[selectedIndex].uv[2] = images[selectedIndex].uv[1];
		images[selectedIndex].uv[1] = images[selectedIndex].uv[0];
		images[selectedIndex].uv[0] = p;
	}
	int w = images[selectedIndex].w;
	images[selectedIndex].w = images[selectedIndex].h;
	images[selectedIndex].h = w;
}
void ImageManagment::flipImageX(Image *image)
{
	if (image == nullptr)
		return;
	image->flipX = !image->flipX;

	ImVec2 p = image->uv[0];
	image->uv[0] = image->uv[1];
	image->uv[1] = p;

	p = image->uv[2];
	image->uv[2] = image->uv[3];
	image->uv[3] = p;
}
void ImageManagment::flipImageY(Image* image)
{
	if (image == nullptr)
		return;
	image->flipY = !image->flipY;
	ImVec2 p = image->uv[0];
	image->uv[0] = image->uv[3];
	image->uv[3] = p;

	p = image->uv[1];
	image->uv[1] = image->uv[2];
	image->uv[2] = p;
}
Image* ImageManagment::getCurrentImage() {
	if (!imagesMutex.try_lock()) {
		return nullptr;
	}
	imagesMutex.unlock();
	std::lock_guard g(imagesMutex);
	if (selectedIndex >= 0)
		return &images[selectedIndex];
	else
		return nullptr;
}
void ImageManagment::runManagment(std::string imagePath)
{
	setImagesPath(imagePath);
	bool sri = false;
	bool sli = false;
	while (shouldRunManagment) {
		shouldOpenImageMutex.lock();
		sli = shouldOpenImage;
		shouldOpenImageMutex.unlock();

		if (sli)
			loadImages(currentPath.string());

		reloadImagesMutex.lock();
		sri = shouldReloadImages;
		reloadImagesMutex.unlock();
		if (sri) {
			loadCloseImages();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

void ImageManagment::stopManagment()
{
	this->shouldRunManagment = false;
}

void ImageManagment::loadCloseImages()
{
	int j = 0;
	int counter;
	while (true) {
		if (j <= NUMBER_OF_LOADED_IMAGES) {
			if (selectedIndex + j < images.size())
				loadImage(&images[selectedIndex + j]);
			if (selectedIndex - j >= 0)
				loadImage(&images[selectedIndex - j]);
		}
		else {
			counter = 0;
			if (selectedIndex - j >= 0) {
				unloadImage(&images[selectedIndex - j]);
				counter++;
			}
			if (selectedIndex + j < (int)images.size()) {
				unloadImage(&images[selectedIndex + j]);
				counter++;
			}
			if (counter == 0) {
				break;
			}
		}
		j++;
	}
}

void ImageManagment::setImagesPath(std::string imagePath)
{
	shouldOpenImageMutex.lock();
	currentPath = imagePath;
	shouldOpenImage = true;
	selectedIndex = -1;
	shouldOpenImageMutex.unlock();
}

void saveImage(Image image, std::string newFilePath, int type, int quality)
{
	if (newFilePath.empty())
		newFilePath = image.imagePath;
	int width, height, num_channels;
	unsigned char* data = stbi_load(image.imagePath.c_str(), &width, &height, &num_channels, 0);
	if (image.flipX) {
		flipDataX(width, height, data);
	}
	if (image.flipY) {
		flipDataY(width, height, data);
	}
	int p = 0;
	image.rotation = (image.rotation + 4) % 4;
	switch (image.rotation)
	{
	case 1:
		rotateData90(width, height, data);
		p = width;
		width = height;
		height = p;
		break;
	case 2:
		flipDataX(width, height, data);
		flipDataY(width, height, data);
		break;
	case 3:
		rotateData90(width, height, data);
		p = width;
		width = height;
		height = p;
		flipDataY(width, height, data);
		break;
	default:
		break;
	}
	if (image.mod.contrast != 1.0) {
		contrast(width, height, data, image.mod.contrast);
	}
	if (image.mod.saturation != 1.0) {
		saturation(width, height, data, image.mod.saturation);
	}
	switch (type)
	{
	case PNG:
		stbi_write_png(newFilePath.c_str(), width, height, 3, data, width*3);
		break;
	case JPG:
		stbi_write_jpg(newFilePath.c_str(), width, height, 3, data, quality);
		break;
	case BMP:
		stbi_write_bmp(newFilePath.c_str(), width, height, 3, data);
		break;
	default:
		break;
	}

	stbi_image_free(data);
}

void flipDataX(int width, int height, unsigned char* data)
{
	for (int y = 0; y < height; ++y) {
		for (int i = 0; i < width / 2; ++i) {
			std::swap(data[y * width * 3 + i * 3], data[y * width * 3 + (width - i - 1) * 3]);
			std::swap(data[y * width * 3 + i * 3 + 1], data[y * width * 3 + (width - i - 1) * 3 + 1]);
			std::swap(data[y * width * 3 + i * 3 + 2], data[y * width * 3 + (width - i - 1) * 3 + 2]);
		}
	}
}

void flipDataY(int width, int height, unsigned char* data)
{
	for (int y = 0; y < height / 2; ++y) {
		for (int x = 0; x < width; ++x) {
			std::swap(data[(y * width + x) * 3], data[((height - y - 1) * width + x) * 3]);
			std::swap(data[(y * width + x) * 3 + 1], data[((height - y - 1) * width + x) * 3 + 1]);
			std::swap(data[(y * width + x) * 3 + 2], data[((height - y - 1) * width + x) * 3 + 2]);
		}
	}
}

void rotateData90(int width, int height, unsigned char* data)
{
	char* tmp = new char[width * height * 3];
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			tmp[(x * height + (height - y - 1)) * 3] = data[y * width * 3 + x * 3];
			tmp[(x * height + (height - y - 1)) * 3 + 1] = data[y * width * 3 + x * 3 + 1];
			tmp[(x * height + (height - y - 1)) * 3 + 2] = data[y * width * 3 + x * 3 + 2];
		}
	}
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			data[(y * width + x) * 3] = tmp[(y * width + x) * 3];
			data[(y * width + x) * 3 + 1] = tmp[(y * width + x) * 3 + 1];
			data[(y * width + x) * 3 + 2] = tmp[(y * width + x) * 3 + 2];
		}
	}
	delete[] tmp;
}

void contrast(int width, int height, unsigned char* data, float contrast)
{
	int p = 0;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			p = (float)(data[(y * width + x) * 3]) * contrast;
			data[(y * width + x) * 3] = p > 255 ? 255 : (unsigned char)p;

			p = (float)(data[(y * width + x) * 3 + 1]) * contrast;
			data[(y * width + x) * 3 + 1] = p > 255 ? 255 : (unsigned char)p;

			p = (float)(data[(y * width + x) * 3 + 2]) * contrast;
			data[(y * width + x) * 3 + 2] = p > 255 ? 255 : (unsigned char)p;
		}
	}
}

void saturation(int width, int height, unsigned char* data, float saturation)
{
	int p;
	unsigned char color[3] = { 0, 0, 0 };
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			color[0] = data[(y * width + x) * 3];
			color[1] = data[(y * width + x) * 3 + 1];
			color[2] = data[(y * width + x) * 3 + 2];
			rgb2hsv(color);
			p = ((int)color[1] * saturation);
			color[1] = p > 100 ? 100 : (unsigned char)p;
			hsv2rgb(color);
			data[(y * width + x) * 3] = color[0];
			data[(y * width + x) * 3 + 1] = color[1];
			data[(y * width + x) * 3 + 2] = color[2];
		}
	}
}


void rgb2hsv(unsigned char* color)
{
	const float k0 = 0.0f;
	const float k1 = -1.0f / 3.0f;
	const float k2 = 2.0f / 3.0f;
	const float k3 = -1.0f;

	float r = color[0] / 255.0f;
	float g = color[1] / 255.0f;
	float b = color[2] / 255.0f;

	float p[4] = {
		std::fmax(b, g),
		std::fmin(b, g),
		k2 * std::fmin(b, g) + k3,
		std::fmax(r, std::fmax(b, g))
	};
	float q[4] = {
		p[1],
		std::fmax(p[0], r),
		k1* std::fmin(p[0], r) + k3,
		std::fmax(p[2], p[3])
	};
	float d = q[3] - std::fmin(q[1], q[2]);
	float epsilon = 1e-10f;
	float hsv[3] = {
		std::abs(q[2] + (q[1] - q[2]) / (6.0f * d + epsilon)),
		d / (q[3] + epsilon),
		q[3]
	};
	color[0] = (unsigned int)(hsv[0] * 255);
	color[1] = (unsigned int)(hsv[1] * 255);
	color[2] = (unsigned int)(hsv[2] * 255);
}

void hsv2rgb(unsigned char* color)
{
	const float k0 = 1.0f;
	const float k1 = 2.0f / 3.0f;
	const float k2 = 1.0f / 3.0f;
	const float k3 = 3.0f;

	float h = color[0] / 255.0;
	float s = color[1] / 255.0;
	float v = color[2] / 255.0;

	h = std::fmod(h, 1.0f);

	float p[3] = {
		h + k2,
		h + k1,
		h
	};
	int i = std::floor(h * 6.0f);
	float f = h * 6.0f - i;
	float q[4] = {
		v,
		v * (1.0f - s),
		v * (1.0f - s * f),
		v * (1.0f - s * (1.0f - f))
	};
	if (i % 2 == 0) {
		color[0] = (unsigned char)(q[0] * 255);
		color[1] = (unsigned char)(q[1] * 255);
		color[2] = (unsigned char)(q[2] * 255);
	}
	else {
		color[0] = (unsigned char)(q[3] * 255);
		color[1] = (unsigned char)(q[0] * 255);
		color[2] = (unsigned char)(q[1] * 255);
	}
}
