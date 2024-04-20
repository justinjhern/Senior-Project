#ifndef NODE_H
#define NODE_H

#include <filesystem>
#include <string>
#include <zmq.hpp>

#include "node_filesystem.hpp"

class Node {
 public:
  // Construcotr, root dir to create the file system, target nodes <ip, port>
  Node(const std::filesystem::path& rootDir,
       const std::vector<std::pair<std::string, int>>& initialTargetNodes,
       int port);
  ~Node();

  enum class FileOperation { READ, WRITE, DELETE, LIST };

  // Function to initialize zmq sockets
  void initialize();

  // Function to listen on server socket and handle incoming requests
  void handleRequests();

  // Function to send file request messages to other nodes
  void sendRequest(const std::string& targetIp, FileOperation operation,
                   const std::string& fileName = "No File Name");

  // Function to set target nodes
  void setTargetNodes(
      const std::vector<std::pair<std::string, int>>& newTargetNodes);

 private:
  NodeFileSystem fileSystem_;

  std::filesystem::path rootDir_;

  // todo remove port 2
  int port_;

  // zmq context
  zmq::context_t context_;
  // Accepts requests for file, returning file conent
  zmq::socket_t clientSocket_;
  // Requests file, accepts file content
  zmq::socket_t serverSocket_;

  std::vector<std::pair<std::string, int>> targetNodes_;
};

#endif  // NODE_H