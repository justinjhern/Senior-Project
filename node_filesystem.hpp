#ifndef NODEFILESYSTEM_H
#define NODEFILESYSTEM_H

#include <filesystem>
#include <string>

class NodeFileSystem {
public:
    // creates root dir one doesnt exist
    NodeFileSystem(const std::filesystem::path& rootDir);

    void createFile(const std::string& fileName, const std::string& content = "");
    void readFile(const std::string& fileName);
    void deleteFile(const std::string& fileName);
    void listFiles();

private:
    std::filesystem::path rootDir_;
};

#endif // NODEFILESYSTEM_H