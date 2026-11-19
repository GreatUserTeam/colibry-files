// Archives with the system `tar` (MIT). No shell is used: names are passed as separate arguments.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace colibry::files {

struct ArchiveResult {
	bool ok = false;
	std::filesystem::path path;  // archive created, or folder extracted into
	std::string error;
};

namespace archive {

// True for .tar, .tar.gz, .tgz, .tar.xz, .tar.bz2 (by name).
bool isArchive(const std::filesystem::path& path);

// argv for creating "<name>.tar.gz" in `outDir` holding `sources` (all of them in one parent folder).
std::vector<std::string> createArgs(const std::filesystem::path& archivePath,
                                    const std::filesystem::path& parent,
                                    const std::vector<std::string>& names);

// An entry name that is absolute or climbs out of the destination ("..") is unsafe.
bool isSafeEntryName(const std::string& entry);

// Creates "<outDir>/<base>.tar.gz" (a free name; never overwrites). `sources` must share one parent.
ArchiveResult create(const std::vector<std::filesystem::path>& sources, const std::filesystem::path& outDir);

// Extracts into a new folder "<outDir>/<archive name without extension>" (free name). The entry list is
// checked first and the archive is refused when an entry would land outside the folder.
ArchiveResult extract(const std::filesystem::path& archivePath, const std::filesystem::path& outDir);

}  // namespace archive
}  // namespace colibry::files
