#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <zmq.hpp>

#include "node.hpp"

void runNodeRequestHandler(Node &node, std::atomic<bool> &serverRunning) {
  node.handleRequests(serverRunning);
}

int main() {
  // Define file path and root directory path
  // std::filesystem::path rootDir;                      // Replace with your directory path
  // std::string fileName = "data.txt", directory;  // Replace with your file name
  // // Get target IP and port from user input
  // int targetPort1, bindedPort, targetPort2;

  // std::cout << "Enter target port to bind to: \n";
  // std::cin >> bindedPort;
  // std::cout << "Enter target port to connect to: \n";
  // std::cin >> targetPort1;
  // std::cout << "Enter target root dir: \n";
  // std::cin >> directory;
  // rootDir = directory;
  // std::cout << "Enter second target port to connect to: \n";
  // std::cin >> targetPort2;
  // Create a Node instance with an empty target node list (no other nodes)
  // Node node("testingDir", {{"localhost", 31416}}, "*", 31415);  
  Node node("testingDir2", {{"localhost", 31415},{"localhost", 31417}},"*" ,31416);
  std::atomic<bool> serverRunning(true);

  std::thread serverThread(runNodeRequestHandler, std::ref(node),
                         std::ref(serverRunning));

  while (true) {
    int task;
    std::cout << "would you like to send a file? 0:CREATE | 1:SEND | 2:DELETE (call after create)" << std::endl;
    std::cin >> task;
    if (task == 0) {
      node.sendRequest(Node::FileOperation::LIST, "Nothing");
    } else if (task == 1) {
      node.sendRequest(Node::FileOperation::SEND, "testdraw.bmp");
    } else {
      break;
    }
  }

  std::cout << "exited loop" << std::endl;
  serverRunning = false;
  std::cout << "exited loop1" << std::endl;
  serverThread.join();
  std::cout << "see you later alligator!" << std::endl;

  return 0;
}