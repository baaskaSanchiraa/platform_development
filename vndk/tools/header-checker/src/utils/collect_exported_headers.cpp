// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "utils/header_abi_util.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

#include <set>
#include <string>
#include <vector>


namespace header_checker {
namespace utils {


static bool ShouldSkipFile(llvm::StringRef &file_name) {
  // Ignore swap files, hidden files, and hidden directories. Do not recurse
  // into hidden directories either. We should also not look at source files.
  // Many projects include source files in their exports.
  return (file_name.empty() || file_name.startswith(".") ||
          file_name.endswith(".swp") || file_name.endswith(".swo") ||
          file_name.endswith("#") || file_name.endswith(".cpp") ||
          file_name.endswith(".cc") || file_name.endswith(".c"));
}

std::string GetCwd() {
  llvm::SmallString<256> cwd;
  if (llvm::sys::fs::current_path(cwd)) {
    llvm::errs() << "ERROR: Failed to get current working directory\n";
    ::exit(1);
  }
  return cwd.c_str();
}

std::string NormalizePath(const std::string &path,
                          const std::string &root_dir) {
  llvm::SmallString<256> norm_path(path);
  if (llvm::sys::fs::make_absolute(norm_path)) {
    return "";
  }
  llvm::sys::path::remove_dots(norm_path, /* remove_dot_dot = */ true);
  // Convert /cwd/path to /path.
  if (llvm::sys::path::replace_path_prefix(norm_path, root_dir, "")) {
    // Convert /path to path.
    return llvm::sys::path::relative_path(norm_path.str()).str();
  }
  return std::string(norm_path);
}

static bool CollectExportedHeaderSet(const std::string &dir_name,
                                     std::set<std::string> *exported_headers,
                                     const std::string &root_dir) {
  std::error_code ec;
  llvm::sys::fs::recursive_directory_iterator walker(dir_name, ec);
  // Default construction - end of directory.
  llvm::sys::fs::recursive_directory_iterator end;
  for ( ; walker != end; walker.increment(ec)) {
    if (ec) {
      llvm::errs() << "Failed to walk directory: " << dir_name << ": "
                   << ec.message() << "\n";
      return false;
    }

    const std::string &file_path = walker->path();

    llvm::StringRef file_name(llvm::sys::path::filename(file_path));
    // Ignore swap files and hidden files / dirs. Do not recurse into them too.
    // We should also not look at source files. Many projects include source
    // files in their exports.
    if (ShouldSkipFile(file_name)) {
      walker.no_push();
      continue;
    }

    llvm::ErrorOr<llvm::sys::fs::basic_file_status> status = walker->status();
    if (!status) {
      llvm::errs() << "Failed to stat file: " << file_path << "\n";
      return false;
    }

    if ((status->type() != llvm::sys::fs::file_type::symlink_file) &&
        (status->type() != llvm::sys::fs::file_type::regular_file)) {
      // Ignore non regular files, except symlinks.
      continue;
    }

    exported_headers->insert(NormalizePath(file_path, root_dir));
  }
  return true;
}

std::set<std::string>
CollectAllExportedHeaders(const std::vector<std::string> &exported_header_dirs,
                          const std::string &root_dir) {
  std::set<std::string> exported_headers;
  for (auto &&dir : exported_header_dirs) {
    if (!CollectExportedHeaderSet(dir, &exported_headers, root_dir)) {
      llvm::errs() << "Couldn't collect exported headers\n";
      ::exit(1);
    }
  }
  return exported_headers;
}

}  // namespace utils
}  // namespace header_checker
