// `convertX`: converts files with external tools (MIT). Images use ImageMagick, audio and video use ffmpeg.
// The argument list is built by a pure function, so it is testable without the tools.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace colibry::files {

struct ConvertPlan {
	bool ok = false;
	std::string error;
	std::vector<std::string> argv;
	std::filesystem::path output;
};

namespace convert {

enum class Family { None, Image, Audio, Video };

Family familyOf(const std::string& extension);                 // "png" or ".PNG" -> Image
std::vector<std::string> targetsFor(const std::string& extension);  // formats a file can become

// `magickProgram`: "magick", "convert" or "" (not installed); `ffmpegAvailable` likewise.
// The output goes next to nothing else: "<outDir>/<name>.<target>" with a free name, never overwriting.
ConvertPlan plan(const std::filesystem::path& source, const std::string& target, const std::filesystem::path& outDir,
                 const std::string& magickProgram, bool ffmpegAvailable);

// Looks for the tools, plans and runs. The output file is removed again when the tool fails.
ConvertPlan run(const std::filesystem::path& source, const std::string& target, const std::filesystem::path& outDir);

}  // namespace convert
}  // namespace colibry::files
