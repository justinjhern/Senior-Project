#ifndef NODEFILESYSTEM_H
#define NODEFILESYSTEM_H

#include <jsoncpp/json/json.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

class NodeFileSystem {
 public:
  // creates root dir one doesnt exist
  NodeFileSystem(const std::filesystem::path& rootDir);

  // struct for file storage
  struct fileMetadata {
    std::string storedIpAddress;
    std::uintmax_t fileSize;
    std::string lastModified;

    // refactor code to use this 
    Json::Value toJson() const {
      Json::Value val;
      val["storedIpAddress"] = storedIpAddress;
      val["fileSize"] = Json::UInt64(fileSize);
      val["lastModified"] = lastModified;
      return val;
    }

    // Deserialize JSON to fileMetadata
    static fileMetadata fromJson(const Json::Value& val) {
      fileMetadata metadata;
      metadata.storedIpAddress = val["storedIpAddress"].asString();
      metadata.fileSize = val["fileSize"].asUInt64();
      metadata.lastModified = val["lastModified"].asString();
      return metadata;
    }
  };

  void createFile(const std::string& fileName);
  void readFile(const std::string& fileName);
  void getFile(const std::string& fileName);
  std::string deleteFile(const std::string& fileName);
  std::vector<std::string> listFiles();
  std::map<std::string, NodeFileSystem::fileMetadata> getFileMetadata();

 private:
  std::filesystem::path rootDir_;
  std::map<std::string, NodeFileSystem::fileMetadata> fileMData;
};

#endif  // NODEFILESYSTEM_H