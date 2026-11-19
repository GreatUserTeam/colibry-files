// Files: the Colibry OS file manager on Raven.
#include "FilesApp.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>

int main(int argc, char** argv) {
	bool headless = false;
	std::filesystem::path start;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--headless") == 0) headless = true;
		else start = argv[i];
	}
	if (start.empty()) {
		const char* home = std::getenv("HOME");
		start = home ? home : "/";
	}
	if (!colibry::init({"Files", headless})) return 1;
	raven::Window window = colibry::window_from_file(colibry::sharePath("files/files.xml"));
	if (!window.is_valid()) return 1;

	colibry::files::FilesApp app;
	app.attach(window, start);
	return colibry::run();
}
