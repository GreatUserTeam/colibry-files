#include "Archive.hpp"

#include "FileOps.hpp"

#include <colibry/process.hpp>

#include <algorithm>
#include <sstream>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace colibry::files::archive {

static std::string lower(std::string s) {
	for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

static bool endsWith(const std::string& s, const std::string& suffix) {
	return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string stripArchiveExtension(const std::string& name) {
	const std::string l = lower(name);
	for (const char* ext : {".tar.gz", ".tar.xz", ".tar.bz2", ".tgz", ".tar"})
		if (endsWith(l, ext)) return name.substr(0, name.size() - std::string(ext).size());
	return name;
}

bool isArchive(const fs::path& path) {
	const std::string l = lower(path.filename().string());
	return endsWith(l, ".tar") || endsWith(l, ".tar.gz") || endsWith(l, ".tgz") || endsWith(l, ".tar.xz") || endsWith(l, ".tar.bz2");
}

std::vector<std::string> createArgs(const fs::path& archivePath, const fs::path& parent, const std::vector<std::string>& names) {
	std::vector<std::string> argv = {"tar", "-czf", archivePath.string(), "-C", parent.string(), "--"};
	argv.insert(argv.end(), names.begin(), names.end());
	return argv;
}

bool isSafeEntryName(const std::string& entry) {
	if (entry.empty() || entry[0] == '/') return false;
	std::stringstream parts(entry);
	std::string part;
	while (std::getline(parts, part, '/'))
		if (part == "..") return false;
	return true;
}

ArchiveResult create(const std::vector<fs::path>& sources, const fs::path& outDir) {
	ArchiveResult r;
	if (sources.empty()) { r.error = "Nothing to archive."; return r; }
	if (!programExists("tar")) { r.error = "The 'tar' program is not installed."; return r; }
	const fs::path parent = sources.front().parent_path();
	std::vector<std::string> names;
	for (const auto& s : sources) {
		if (s.parent_path() != parent) { r.error = "The items must be in one folder."; return r; }
		names.push_back(s.filename().string());
	}
	const std::string base = sources.size() == 1 ? sources.front().filename().string() : "archive";
	const fs::path target = outDir / fileops::uniqueName(outDir, base + ".tar.gz");
	const ProcessResult run = runProcess(createArgs(target, parent, names));
	if (!run.started || run.exitCode != 0) {
		std::error_code ec;
		fs::remove(target, ec);
		r.error = run.started ? "tar failed: " + run.output : "Cannot start tar.";
		return r;
	}
	r.ok = true;
	r.path = target;
	return r;
}

ArchiveResult extract(const fs::path& archivePath, const fs::path& outDir) {
	ArchiveResult r;
	if (!isArchive(archivePath)) { r.error = "This is not a supported archive."; return r; }
	if (!programExists("tar")) { r.error = "The 'tar' program is not installed."; return r; }
	const ProcessResult listing = runProcess({"tar", "-tf", archivePath.string()});
	if (!listing.started || listing.exitCode != 0) { r.error = "Cannot read the archive."; return r; }
	std::stringstream lines(listing.output);
	std::string entry;
	while (std::getline(lines, entry))
		if (!isSafeEntryName(entry)) { r.error = "The archive contains an unsafe path: " + entry; return r; }

	const std::string folder = fileops::uniqueName(outDir, stripArchiveExtension(archivePath.filename().string()));
	const fs::path target = outDir / folder;
	std::error_code ec;
	fs::create_directories(target, ec);
	if (ec) { r.error = "Cannot create the folder."; return r; }
	const ProcessResult run = runProcess({"tar", "-xf", archivePath.string(), "-C", target.string(), "--no-same-owner"});
	if (!run.started || run.exitCode != 0) {
		fs::remove_all(target, ec);
		r.error = "tar failed: " + run.output;
		return r;
	}
	r.ok = true;
	r.path = target;
	return r;
}

}  // namespace colibry::files::archive
