#include "node.hpp"

#include <fmt/core.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <zmq.hpp>

#include "node_filesystem.hpp"

Node::Node(const std::filesystem::path& rootDir,
           const std::vector<std::pair<std::string, int>>& initialTargetNodes,
           int port)
    : rootDir_(rootDir),
      targetNodes_(initialTargetNodes),
      fileSystem_(NodeFileSystem(rootDir)),
      port_(port) {}

Node::~Node() {
  context_.close();
  clientSocket_.close();
  serverSocket_.close();
}

void Node::initialize() {
  // todo change to fix ip address 
  std::cout << "Initializing Node to bind to tcp://localhost:" << port_ << "."
            << std::endl;
  context_ = zmq::context_t(1);

  serverSocket_ = zmq::socket_t(context_, ZMQ_ROUTER);
  serverSocket_.bind(
      "tcp://*:" +
      std::to_string(port_));  // bind to any connections on port 31415
  clientSocket_ = zmq::socket_t(context_, ZMQ_DEALER);

  for (const auto& target : targetNodes_) {
    std::string targetIp = target.first;
    int targetPort = target.second;
    clientSocket_.connect(fmt::format("tcp://{}:{}", targetIp, targetPort));
  }
}

void Node::handleRequests() {
  // run inifinite loop listening for messages
  for (;;) {
    zmq_msg_t msg;
    int rc = zmq_msg_init(&msg);

    assert(rc == 0);
    rc = zmq_msg_recv(&msg, serverSocket_, 0);

    if (rc == -1) {
      // todo error handling
      std::cerr << "Error receiving message: " << zmq_strerror(zmq_errno())
                << std::endl;
      zmq_msg_close(&msg);
      continue;
    }

    const char* message_data = static_cast<const char*>(zmq_msg_data(&msg));
    std::string test = message_data;
    // todo check whether file exists
    std::cout << "Request Accepted: " << test << std::endl;

    zmq_msg_close(&msg);
  }
}

// todo remove target IP
void Node::sendRequest(const std::string& targetIp, FileOperation operation,
                       const std::string& fileName) {
  zmq_msg_t requestMsg;
  char fileNameCStr[fileName.length() + 1];
  strcpy(fileNameCStr, fileName.c_str());

  zmq_msg_init_size(&requestMsg, strlen(fileNameCStr));
  memcpy(zmq_msg_data(&requestMsg), fileNameCStr, strlen(fileNameCStr));
  int rc = zmq_msg_send(&requestMsg, clientSocket_, 0);
  assert(rc == strlen(fileNameCStr));
  // todo add operation
  zmq_msg_close(&requestMsg);
}
