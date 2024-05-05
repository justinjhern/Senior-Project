#include <jsoncpp/json/json.h>
#include <stdio.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "node.hpp"

enum class command_list { INVALID, LIST, REFRESH, READ, CREATE, DELETE };

void runNodeRequestHandler(Node &node, std::atomic<bool> &serverRunning) {
  node.handleRequests(serverRunning);
}

std::vector<std::string> split_string(const std::string &str) {
  std::vector<std::string> words;
  std::istringstream iss(str);

  std::string word;
  while (std::getline(iss, word, ' ')) {
    words.push_back(word);
  }

  return words;
}

void readConfigFromFile(std::pair<std::string, int> &nodeIp,
                        std::vector<std::pair<std::string, int>> &targetNodes,
                        std::string &rootDir) {
  std::string jsonName = "CONFIGtest1.json";
  std::ifstream infile(jsonName);
  if (!infile.is_open()) {
    std::cerr << "Error: Could not open file: " << jsonName << std::endl;
    return;
  }

  Json::CharReaderBuilder readerBuilder;
  Json::Value root;
  std::string errors;

  if (!Json::parseFromStream(readerBuilder, infile, &root, &errors)) {
    std::cerr << "Error: Failed to parse JSON: " << errors << std::endl;
    return;
  }

  // Parse root directory
  rootDir = root["root_directory"].asString();

  // Parse node IP and port
  nodeIp.first = root["node_ip"].asString();
  nodeIp.second = root["node_port"].asInt();

  // Parse target nodes array
  const Json::Value targetNodesArray = root["target_nodes"];
  for (const auto &targetNode : targetNodesArray) {
    std::pair<std::string, int> target;
    target.first = targetNode["ip"].asString();
    target.second = targetNode["port"].asInt();
    targetNodes.push_back(target);
  }
}

void printHelp() {
  std::cout << "\
  SIMPLE DISTRIBUTED FILE STORAGE SYSTEM COMMANDS\n\
  1) read [filename]     | Prints contents of file, if the file is not on the user's node a copy will be downloaded.\n\
  2) create [filename]   | Creates a file on the user's node.\n\
  3) delete [filename]   | Deletes a file from the user's node.\n\
  4) update [filename]   | Searches for node of origin for file, updating it if node of origin is found.\n\
  5) list                | Updates files. Lists all files in the DFSS. Lists files on user's nodes first, followed by files on other active nodes.\n\
  6) exit                | Closes node, exits storage system."
            << std::endl;
}

void createFile(Node &node, std::string fileName) { node.createFile(fileName); }
void deleteFile(Node &node, std::string fileName) { node.deleteFile(fileName); }
void updateFile(Node &node) {}
void listFiles(Node &node) { node.listFiles(); }
void refresh(Node &node) { node.refreshFileData(); }

void process(std::vector<std::string> input, Node &node) {
  for (std::string entry : input) {
    std::cout << entry << std::endl;
  }
  if (input[0] == "help") printHelp();
  if (input[0] == "create") createFile(node, input[1]);
  if (input[0] == "delete") deleteFile(node, input[1]);
  if (input[0] == "refresh") refresh(node);
  if (input[0] == "list") listFiles(node);
}

std::string cleanStr(std::string line) {
  std::string str = line;
  remove_if(str.begin(), str.end(), isspace);
  std::for_each(str.begin(), str.end(), [](char &c) { c = std::tolower(c); });
  return str;
}

int main(int argc, char *argv[]) {
  std::pair<std::string, int> nodeIp;
  std::vector<std::pair<std::string, int>> targetNodes;
  std::string rootDir;

  readConfigFromFile(nodeIp, targetNodes, rootDir);

  Node node(rootDir, targetNodes, nodeIp.first, nodeIp.second);

  std::atomic<bool> serverRunning(true);

  std::thread serverThread(runNodeRequestHandler, std::ref(node),
                           std::ref(serverRunning));

  std::cout << "Input command \"help\" for commands. " << std::endl;

  bool runCli = true;
  while (runCli) {
    std::string line;
    std::cout << "> SDFSS ";
    std::cin >> line;
    std::vector<std::string> input = split_string(line);
    std::vector<std::string> temp;
    for (std::string str : input) temp.push_back(cleanStr(str));
    if (!line.empty() && temp[0] == "exit") {
      runCli = false;
    } else {
      process(temp, node);
    }
  }
  
  serverRunning = false;
  serverThread.join();
  std::cout << "see you later alligator!" << std::endl;
  return 0;
}