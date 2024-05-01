#include <jsoncpp/json/json.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

// todo add more error checking

void setNodeIp(bool &nodeIpBool, std::pair<std::string, int> &nodeIp) {
  std::string ipAddress;
  int port;
  std::cout << "Setting node IP and port: " <<std::endl;
  std::cout << "Input IP address. Do not include port." << std::endl;
  std::cin >> ipAddress;
  std::cout << "Input port number. 31415 is recommended." << std::endl;
  std::cin >> port;
  nodeIpBool = true;
  nodeIp = std::make_pair(ipAddress, port);
}

void addTargetNodes(bool &targetNodesBool,
                    std::vector<std::pair<std::string, int>> &targetNodes,
                    std::pair<std::string, int> nodeIp) {
  std::string ipAddress;
  int port;
  std::cout << "Adding target node: " << std::endl;
  std::cout << "Input IP address. Do not include port." << std::endl;
  std::cin >> ipAddress;
  std::cout << "Input port number. 31415 is recommended." << std::endl;
  std::cin >> port;
  if (ipAddress == nodeIp.first) {
    std::cout << "Error, cannot have matching IP address as node." << std::endl;
  } else {
    targetNodesBool = true;
    targetNodes.push_back(std::make_pair(ipAddress, port));
  }
}

void deleteTargetNodes(bool &targetNodesBool, std::vector<std::pair<std::string, int>> &targetNodes) {
  std::string ipAddress;
  std::cout << "Delete node from target nodes. " << std::endl;
  std::cout << "Input IP address to delete. Do not include port." << std::endl;
  std::cin >> ipAddress;

  auto it =
      std::remove_if(targetNodes.begin(), targetNodes.end(),
                     [&ipAddress](const std::pair<std::string, int> &node) {
                       return node.first == ipAddress;
                     });

  if (it != targetNodes.end()) {
    targetNodes.erase(it, targetNodes.end());
    std::cout << "Node(s) with IP address " << ipAddress << " removed."
              << std::endl;
    if(targetNodes.empty()){
      targetNodesBool = false;
    }
  } else {
    std::cout << "No node with IP address " << ipAddress << " found."
              << std::endl;
  }
}

void setRootDir(bool &rootDirBool, std::string &rootDir) {
  std::string rootDirInput;
  std::cout << "Input root directory. This will be the file where the filesystem is active" << std::endl;
  std::cin >> rootDirInput;
  rootDirBool = true;
  rootDir = rootDirInput;
}

void writeConfigToFile(const std::pair<std::string, int> nodeIp,
                       const std::vector<std::pair<std::string, int>> targetNodes,
                       const std::string& rootDir) {
    std::string jsonName = "CONFIG.json"; 
    std::ofstream outfile(jsonName);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file: " << jsonName << std::endl;
        return;
    }

    outfile << "{" << std::endl;
    outfile << "  \"root_directory\": \"" << rootDir << "\"," << std::endl; 
    outfile << "  \"node_ip\": \"" << nodeIp.first << "\"," << std::endl;
    outfile << "  \"node_port\": " << nodeIp.second << "," << std::endl;
    outfile << "  \"target_nodes\": [" << std::endl;

    bool firstTarget = true;
    for (const auto& target : targetNodes) {
        if (!firstTarget) {
            outfile << "," << std::endl;
        }
        outfile << "    { \"ip\": \"" << target.first << "\", \"port\": " << target.second << " }";
        firstTarget = false;
    }

    outfile << std::endl << "  ]" << std::endl;
    outfile << "}" << std::endl;
    outfile.close();

    std::cout << "Configuration saved successfully to: " << jsonName << std::endl;
}

int main() {
  std::pair<std::string, int> nodeIp;
  std::vector<std::pair<std::string, int>> targetNodes;
  string rootDir;
  bool nodeIpBool = false, targetNodesBool = false, rootDirBool = false;

  std::cout << "JSON config creator" << std::endl;
  bool runLoop = true;
  while (runLoop) {
    std::cout << "INPUT NODE CONFIG SETTINGS \n"
                 "1: Change Node IP \n"
                 "2: Add to target nodes \n"
                 "3: Remove from target nodes \n"
                 "4: Set root directory. \n"
                 "5: View config \n"
                 "6: Finish config"
              << std::endl;
    int input;
    std::cin >> input;
    switch (input) {
      case 1:  // change node ip
        setNodeIp(nodeIpBool, nodeIp);
        break;
      case 2:  // add to target nodes
        addTargetNodes(targetNodesBool, targetNodes, nodeIp);
        break;
      case 3:  // remove from target nodes
        deleteTargetNodes(targetNodesBool, targetNodes);
        break;
      case 4:  // set root directory
        setRootDir(rootDirBool, rootDir);
        break;
      case 5:
        break;
      case 6:
        if(rootDirBool && targetNodesBool && nodeIpBool)
          runLoop = false;
        break;
      default:
        break;
    }
  }
  writeConfigToFile(nodeIp, targetNodes, rootDir);
  return 0;
}