#include "node_filesystem.hpp"
#include <fstream>
#include <iostream>

// Creates root dir if doesnt exist
NodeFileSystem::NodeFileSystem(const std::filesystem::path& rootDir) : rootDir_(rootDir) {
    if (!std::filesystem::exists(rootDir_)) {
        std::filesystem::create_directory(rootDir_);
        std::cout << "Created directory at " << rootDir_ << std::endl;
    }
    std::cout << "Directory at " << rootDir_ << " already exists." << std::endl;
}

// Creates file from filename and string as content
void NodeFileSystem::createFile(const std::string& fileName, const std::string& content) {
    std::ofstream file(rootDir_ / fileName);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "File \"" << fileName << "\" created successfully.\n";
    } else {
        std::cerr << "Failed to create the file \"" << fileName << "\".\n";
    }
}

// Reads file as string
void NodeFileSystem::readFile(const std::string& fileName) {
    std::ifstream file(rootDir_ / fileName);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.is_open()) {
        std::cout << "Contents of \"" << fileName << "\":\n" << content << std::endl;
        file.close();
    } else {
        std::cerr << "Failed to read the file \"" << fileName << "\".\n";
    }
}

void NodeFileSystem::deleteFile(const std::string& fileName) {
    if (std::filesystem::remove(rootDir_ / fileName)) {
        std::cout << "File \"" << fileName << "\" deleted successfully.\n";
    } else {
        std::cerr << "Failed to delete the file \"" << fileName << "\".\n";
    }
}

void NodeFileSystem::listFiles() {
    std::cout << "Files in \"" << rootDir_ << "\":\n";
    for (const auto& entry : std::filesystem::directory_iterator(rootDir_)) {
        std::cout << entry.path().filename() << std::endl;
    }
}