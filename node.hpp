#ifndef NODE_H
#define NODE_H

#include <jsoncpp/json/json.h>

#include <atomic>
#include <filesystem>
#include <map>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "node_filesystem.hpp"

class Node {
 public:
  // Construcotr, root dir to create the file system, target nodes <ip, port>
  Node(const std::filesystem::path& rootDir,
       const std::vector<std::pair<std::string, int>>& initialTargetNodes,
       const std::string ipAddress, int port);
  ~Node();

  /* todo Add write functionality
   * SEND: Sends a copy file. Could be written or for reading
   * DELETE: Removes file
   * LIST: Sends the {filename, metadata} vector
   * CREATE: Creates a file
   */
  enum class FileOperation { SEND, DELETE, LIST, CREATE };

  // Function to initialize zmq sockets
  void initialize();

  // Function to listen on server socket and handle incoming requests
  void handleRequests(std::atomic<bool>& runServer);

  // Function to send file request messages to other nodes
  void sendRequest(FileOperation operation,
                   const std::string& fileName = "No File Name");

  // Function to set target nodes
  void setTargetNodes(
      const std::vector<std::pair<std::string, int>>& newTargetNodes);

  std::map<std::string, NodeFileSystem::fileMetadata> getMyFileData();

  std::map<std::string, NodeFileSystem::fileMetadata> getOtherFileData();

  void addFileData();

  void setFileData(std::map<std::string, NodeFileSystem::fileMetadata>);

  // creates file if it doesnt exist already
  void createFile(std::string fileName);

  // deletes file from filesystem
  void deleteFile(std::string fileName);

  // reads file. If not on users node asks other nodes for file.
  void readFile(std::string fileName);

  void listFiles();

  void refreshFileData();

 private:
  NodeFileSystem fileSystem_;

  std::filesystem::path rootDir_;

  std::string ipAddress_;

  std::string mode_;

  int port_;
  // context
  zmq::context_t context_;
  // Accepts requests for file, returning file conent
  zmq::socket_t clientSocket_;
  // Requests file, accepts file content
  zmq::socket_t serverSocket_;

  std::vector<std::pair<std::string, int>> targetNodes_;

  std::map<std::string, NodeFileSystem::fileMetadata> myFileMdata,
      otherFileMData;
};

class connect_monitor_t : public zmq::monitor_t {
 public:
  void on_event_connected(const zmq_event_t& event, const char* addr) override;
};

#endif  // NODE_H