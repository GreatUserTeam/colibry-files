#include "Convert.hpp"

#include "FileOps.hpp"

#include <colibry/process.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace colibry::files::convert {

static std::string normalized(std::string ext) {
	if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
	for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return ext;
}

static const std::vector<std::string> kImages = {"png", "jpg", "jpeg", "webp", "gif", "bmp", "tiff"};
static const std::vector<std::string> kAudio = {"mp3", "ogg", "wav", "flac", "m4a", "opus"};
static const std::vector<std::string> kVideo = {"mp4", "mkv", "webm", "avi", "mov"};

static bool in(const std::vector<std::string>& list, const std::string& v) {
	return std::find(list.begin(), list.end(), v) != list.end();
}

Family familyOf(const std::string& extension) {
	const std::string e = normalized(extension);
	if (in(kImages, e)) return Family::Image;
	if (in(kAudio, e)) return Family::Audio;
	if (in(kVideo, e)) return Family::Video;
	return Family::None;
}

std::vector<std::string> targetsFor(const std::string& extension) {
	const std::string e = normalized(extension);
	std::vector<std::string> out;
	switch (familyOf(e)) {
		case Family::Image: out = kImages; break;
		case Family::Audio: out = kAudio; break;
		case Family::Video: out = kVideo; out.push_back("gif"); out.insert(out.end(), {"mp3", "ogg", "wav"}); break;
		case Family::None: break;
	}
	out.erase(std::remove(out.begin(), out.end(), e), out.end());
	return out;
}

ConvertPlan plan(const fs::path& source, const std::string& targetIn, const fs::path& outDir,
                 const std::string& magickProgram, bool ffmpegAvailable) {
	ConvertPlan p;
	const std::string target = normalized(targetIn);
	const std::string from = normalized(source.extension().string());
	const auto targets = targetsFor(from);
	if (familyOf(from) == Family::None) { p.error = "This file type cannot be converted."; return p; }
	if (!in(targets, target)) { p.error = "Cannot convert ." + from + " to ." + target + "."; return p; }

	const std::string name = fileops::uniqueName(outDir, source.stem().string() + "." + target);
	p.output = outDir / name;
	if (familyOf(from) == Family::Image) {
		if (magickProgram.empty()) { p.error = "ImageMagick (magick or convert) is not installed."; return p; }
		p.argv = {magickProgram, source.string(), p.output.string()};
	} else {
		if (!ffmpegAvailable) { p.error = "ffmpeg is not installed."; return p; }
		p.argv = {"ffmpeg", "-nostdin", "-n", "-i", source.string()};
		if (familyOf(from) == Family::Video && in(kAudio, target)) p.argv.push_back("-vn");  // keep the sound only
		p.argv.push_back(p.output.string());
	}
	p.ok = true;
	return p;
}

ConvertPlan run(const fs::path& source, const std::string& target, const fs::path& outDir) {
	const std::string magick = programExists("magick") ? "magick" : programExists("convert") ? "convert" : "";
	ConvertPlan p = plan(source, target, outDir, magick, programExists("ffmpeg"));
	if (!p.ok) return p;
	const ProcessResult r = runProcess(p.argv);
	if (!r.started || r.exitCode != 0) {
		std::error_code ec;
		fs::remove(p.output, ec);
		p.ok = false;
		p.error = r.started ? "The converter failed." : "Cannot start the converter.";
	}
	return p;
}

}  // namespace colibry::files::convert
