#include <filesystem>
#include <iostream>
#include <string>
#include <zmq.hpp>

#include "node.hpp"

// Assuming you have a Node class and related functions (see previous
// discussions)
class Node;

int main() {
  // Define file path and root directory path
  const std::filesystem::path rootDir =
      "nodetesting";                                  // Replace with your directory path
  const std::string fileName = "data.txt";  // Replace with your file name

  // Get target IP and port from user input
  int targetPort, bindedPort, task;
  std::cout << "Enter target port to bind to: \n";
  std::cin >> bindedPort;
  std::cout << "Enter target port to connect to: \n";
  std::cin >> targetPort;
  std::cout << "Enter task 0 = send | 1 = accept: \n";
  std::cin >> task;

  // Create a Node instance with an empty target node list (no other nodes)
  Node node(rootDir, {{"localhost",targetPort}}, bindedPort);
  node.initialize();

  // Send the file to the target node
  if(task == 0) {
    node.sendRequest("localhost", Node::FileOperation::WRITE, fileName);
  } else {
    node.handleRequests();
  }

  // Simulate some work for the node (optional)
  std::cout << "Node started, press Enter to exit..." << std::endl;
  std::cin.get();

  // Shutdown the node (consider proper cleanup)
  // ... your shutdown logic for node

  return 0;
}