#include "node_filesystem.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

// helper functions to conver filetime to string form

template <typename TP>
std::time_t to_time_t(TP tp) {
  using namespace std::chrono;
  auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() +
                                                      system_clock::now());
  return system_clock::to_time_t(sctp);
}

std::string fileTimeToISOString(
    const std::filesystem::file_time_type& fileTime) {
  // Convert file time to time_t using the provided template
  std::time_t time_t_value = to_time_t(fileTime);

  // Use std::put_time for more control over formatting (optional)
  std::tm* timeInfo = std::gmtime(&time_t_value);
  char buffer[80];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeInfo);

  // Alternatively, use std::ctime for simpler formatting
  // std::string formattedTime = std::ctime(&time_t_value);

  return std::string(buffer);
}

// Creates root dir if doesnt exist
NodeFileSystem::NodeFileSystem(const std::filesystem::path& rootDir)
    : rootDir_(rootDir) {
  if (!std::filesystem::exists(rootDir_)) {
    std::filesystem::create_directory(rootDir_);
    std::cout << "Created directory at " << rootDir_ << std::endl;
  } else {
    std::cout << "Directory at " << rootDir_ << " already exists." << std::endl;
  }

  std::map<std::string, NodeFileSystem::fileMetadata> tempMap;
  for (const auto& entry : std::filesystem::directory_iterator(rootDir_)) {
    NodeFileSystem::fileMetadata tempMd;
    tempMd.fileSize = file_size(entry);
    tempMd.lastModified = fileTimeToISOString(last_write_time(entry));
    tempMap[entry.path().filename()] = tempMd;
  }
  fileMData = tempMap;
  std::cout << "Printing files in filesystem" << std::endl;
  for (auto it = fileMData.begin(); it != fileMData.end(); ++it) {
    std::cout << it->first << std::endl;
  }
}

// Creates file from filename returns file metadata
NodeFileSystem::fileMetadata NodeFileSystem::createFile(
    const std::string& fileName) {
  std::ofstream file(rootDir_ / fileName);
  if (file.is_open()) {
    file.close();
    std::cout << "File \"" << fileName << "\" created successfully.\n";
    NodeFileSystem::fileMetadata tempMd;
    tempMd.lastModified =
        fileTimeToISOString(last_write_time(rootDir_ / fileName));
    tempMd.fileSize = file_size(rootDir_ / fileName);
    fileMData[fileName] = tempMd;
    return tempMd;
  } else {
    std::cerr << "Failed to create the file \"" << fileName << "\".\n";
    NodeFileSystem::fileMetadata tempMd;
    return tempMd;
  }
}

// Reads file as string
void NodeFileSystem::readFile(const std::string& fileName) {
  std::ifstream file(rootDir_ / fileName);
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  if (file.is_open()) {
    std::cout << "Contents of \"" << fileName << "\":\n"
              << content << std::endl;
    file.close();
  } else {
    std::cerr << "Failed to read the file \"" << fileName << "\".\n";
  }
}

std::string NodeFileSystem::deleteFile(const std::string& fileName) {
  if (std::filesystem::remove(rootDir_ / fileName)) {
    std::cout << "File \"" << fileName << "\" deleted successfully.\n";
    return fileName;
  } else {
    std::cerr << "Failed to delete the file \"" << fileName << "\".\n";
    return "-1";
  }
}

// will also update files
std::vector<std::string> NodeFileSystem::listFiles() {
  std::cout << "Files in \"" << rootDir_ << "\":\n";
  std::vector<std::string> files;
  for (const auto& entry : std::filesystem::directory_iterator(rootDir_)) {
    std::cout << entry.path().filename() << std::endl;
    files.push_back(entry.path().filename());
  }

  return files;
}

std::map<std::string, NodeFileSystem::fileMetadata>
NodeFileSystem::getFilesMetadata() {
  return fileMData;
}

NodeFileSystem::fileMetadata NodeFileSystem::getFileMetaData(
    std::string fileName) {
  std::ifstream file(rootDir_ / fileName);
  if (file.is_open()) {
    file.close();
    NodeFileSystem::fileMetadata tempMd;
    tempMd.lastModified =
        fileTimeToISOString(last_write_time(rootDir_ / fileName));
    tempMd.fileSize = file_size(rootDir_ / fileName);
    fileMData[fileName] = tempMd;
    return tempMd;
  } else {
    std::cerr << "Failed to get file metadata of \"" << fileName << "\".\n";
    // todo add proper error checking
    NodeFileSystem::fileMetadata tempMd;
    return tempMd;
  }
}
