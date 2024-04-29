#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <zmq.hpp>

#include "node.hpp"

// Assuming you have a Node class and related functions (see previous
// discussions)
class Node;

int main() {
  // Define file path and root directory path
  const std::filesystem::path rootDir =
      "nodetesting";                        // Replace with your directory path
  const std::string fileName = "data.txt";  // Replace with your file name

  // Get target IP and port from user input
  int targetPort1, bindedPort, targetPort2;
  std::cout << "Enter target port to bind to: \n";
  std::cin >> bindedPort;
  std::cout << "Enter target port to connect to: \n";
  std::cin >> targetPort1;
  // std::cout << "Enter second target port to connect to: \n";
  // std::cin >> targetPort2;

  // Create a Node instance with an empty target node list (no other nodes)
  // Node node(rootDir, {{"localhost", targetPort1}, {"localhost", targetPort2}}, bindedPort);
  Node node(rootDir, {{"localhost", targetPort1}},"localhost" ,bindedPort);
  node.initialize();

  std::thread handleThread(&Node::handleRequests, &node);

  while (true) {
    int task;
    std::cout << "would you like to send a file? 0:CREATE | 1:SEND | 2:DELETE (call after create)" << std::endl;
    std::cin >> task;
    if (task == 0) {
      node.sendRequest(Node::FileOperation::LIST, fileName);
    } else if (task == 1) {
      node.sendRequest(Node::FileOperation::SEND, "testing.txt");
    } else {
      node.sendRequest(Node::FileOperation::DELETE, fileName);
    }
  }

  // Shutdown the node (consider proper cleanup)
  // ... your shutdown logic for node

  return 0;
}