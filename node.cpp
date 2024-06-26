#include "node.hpp"

#include <fmt/core.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <zmq.hpp>

#include "node_filesystem.hpp"

const size_t CHUNK_SIZE = 1024;  // 1 kb
#define TIMEOUT_MS 3000          // 3000 ms (3 seconds)

Node::Node(const std::filesystem::path& rootDir,
           const std::vector<std::pair<std::string, int>>& initialTargetNodes,
           const std::string ipAddress, int port)
    : rootDir_(rootDir),
      targetNodes_(initialTargetNodes),
      fileSystem_(NodeFileSystem(rootDir)),
      ipAddress_(ipAddress),
      port_(port) {
  myFileMdata = fileSystem_.getFilesMetadata();
  for (auto& [filename, metadata] : myFileMdata) {
    metadata.storedIpAddress = ipAddress_ + ":" + std::to_string(port_);
  }
  Node::initialize();
}

Node::~Node() {
  for (auto& wrapper : clientSockets_) {
    wrapper->getSocket()->close();
  }
  serverSocket_.close();
  clientSockets_.clear();
  context_.close();
}

void Node::initialize() {
  std::cout << "Initializing server to bind to "
            << fmt::format("tcp://{}:{}.", ipAddress_, port_) << std::endl;
  serverSocket_ = zmq::socket_t(context_, zmq::socket_type::router);
  serverSocket_.bind(fmt::format("tcp://{}:{}", ipAddress_, port_));
  serverSocket_.set(zmq::sockopt::rcvtimeo, 500);

  for (const auto& target : targetNodes_) {
    zmq::socket_t* clientSocket =
        new zmq::socket_t(context_, zmq::socket_type::dealer);
    std::string targetIp = target.first;
    int targetPort = target.second;
    // {public ip}:{port}
    std::cout << "Connecting to: "
              << fmt::format("tcp://{}:{}.", targetIp, targetPort) << std::endl;
    clientSocket->connect(fmt::format("tcp://{}:{}", targetIp, targetPort));

    clientSockets_.push_back(
        std::make_unique<SocketWrapper>(clientSocket, targetIp, targetPort));
  }
}

/*
 * Listens to requests infinitely
 * Requests Handled:
 *   READ: Returns
 *   DELETE: Removes file from filesystem
 *   LIST: Replies with JSON of the file and metadata map
 *   CREATE: Creates a file in a filesystem
 *   UPDATE: Sends a request to the origin node for an update
 *   UPDATED: Sends a JSON of the file and metadata map
 */
void Node::handleRequests(std::atomic<bool>& runServer) {
  while (runServer.load()) {
    std::vector<zmq::message_t> recv_msgs;

    auto ret =
        zmq::recv_multipart(serverSocket_, std::back_inserter(recv_msgs));
    if (!ret) {
      // std::cout << "Error accepting message (Handler)" << std::endl;
      continue;
    }
    // std::cout << "Got " << *ret << " messages" << std::endl;

    std::vector<std::string> messagesStr;

    for (zmq::message_t& msg : recv_msgs) {
      messagesStr.push_back(msg.to_string());
    }

    // for (std::string messageString : messagesStr) {
    //   std::cout << " Recieved (Handler): " << messageString << std::endl;
    // }

    // return flag
    zmq::message_t replyMsg(recv_msgs[0].data(), recv_msgs[0].size());
    auto res = serverSocket_.send(replyMsg, zmq::send_flags::sndmore);

    // // set reply message
    if (messagesStr[1] == "SEND") {
      std::string filename = messagesStr[2];
      if (std::filesystem::exists(rootDir_ / filename)) {
        std::ifstream file(rootDir_ / filename, std::ios::binary);
        // std::cout << "opening " << filename << std::endl;
        if (!file.is_open()) {
          std::cerr << "Error opening file: " << filename << std::endl;
          // todo error handling
        }
        char buffer[CHUNK_SIZE];

        while (true) {
          file.read(buffer, CHUNK_SIZE);
          std::streamsize bytes_read = file.gcount();
          if (!file.eof()) {
            // Send chunk (not the last one)
            // std::cout << "Sending Bytes: " << bytes_read << std::endl;
            zmq::message_t msg(buffer, bytes_read);
            auto res = serverSocket_.send(msg, zmq::send_flags::sndmore);
          } else {
            // Last chunk - send without ZMQ_SNDMORE flag
            // std::cout << "Sending Last Byte: " << bytes_read << std::endl;
            zmq::message_t msg(buffer, bytes_read);
            auto res = serverSocket_.send(msg, zmq::send_flags::none);
            break;
          }
        }
        file.close();
        // std::cout << "Done sending: " << filename << std::endl;
      } else {  // if file was not found
        zmq_msg_t replyMsg;
        std::string endFileStr = "FILE WAS NOT FOUND.";

        zmq::message_t msg(endFileStr.c_str(), endFileStr.length());
        auto res = serverSocket_.send(msg, zmq::send_flags::none);
      }
    }
    if (messagesStr[1] ==
        "DELETE") {  // will not be used (permissions not implemented)
      std::string reply;

      std::string deletedFile = fileSystem_.deleteFile(messagesStr[2]);
      if (deletedFile != "-1") {
        reply = "Deleted file: " + deletedFile;
      } else {
        reply = "Failed to delete file: " + messagesStr[2];
      }

      zmq::message_t msg(reply.c_str(), reply.length());

      // std::cout << "Sending " << msg.to_string() << std::endl;

      auto res = serverSocket_.send(msg, zmq::send_flags::none);
    }
    if (messagesStr[1] == "LIST") {
      Json::Value jsonMap(Json::objectValue);
      for (const auto& [filename, metadata] : myFileMdata) {
        Json::Value metadataJson(Json::objectValue);
        metadataJson["fileSize"] = metadata.fileSize;
        metadataJson["lastModified"] = metadata.lastModified;
        metadataJson["storedIpAddress"] = metadata.storedIpAddress;

        jsonMap[filename] = metadataJson;
      }
      // serialize JSON to string
      Json::StreamWriterBuilder builder;
      std::string jsonString = Json::writeString(builder, jsonMap);

      // send jsonstring
      zmq::message_t msg(jsonString.c_str(), jsonString.length());

      // std::cout << "Sending " << msg.to_string() << std::endl;

      auto res = serverSocket_.send(msg, zmq::send_flags::none);
    }
    if (messagesStr[1] ==
        "CREATE") {  // will not be used permissions not implemented
      fileSystem_.createFile(messagesStr[2]);

      std::string reply = "Created file: " + messagesStr[2];

      zmq::message_t msg(reply.c_str(), reply.length());

      // std::cout << "Sending " << msg.to_string() << std::endl;

      auto res = serverSocket_.send(msg, zmq::send_flags::none);
    }
    if (messagesStr[1] == "UPDATE") {
      sendRequest(FileOperation::UPDATED);
    }
    if (messagesStr[1] == "UPDATED") {
      Json::Value jsonMap(Json::objectValue);
      for (const auto& [filename, metadata] : myFileMdata) {
        Json::Value metadataJson(Json::objectValue);
        metadataJson["fileSize"] = metadata.fileSize;
        metadataJson["lastModified"] = metadata.lastModified;
        metadataJson["storedIpAddress"] = metadata.storedIpAddress;

        jsonMap[filename] = metadataJson;
      }
      // serialize JSON to string
      Json::StreamWriterBuilder builder;
      std::string jsonString = Json::writeString(builder, jsonMap);

      // send jsonstring
      zmq::message_t msg(jsonString.c_str(), jsonString.length());

      // std::cout << "Sending " << msg.to_string() << std::endl;

      auto res = serverSocket_.send(msg, zmq::send_flags::none);
    }
  }
}

// will send two messages, first with operation, second with file name
void Node::sendRequest(FileOperation operation, const std::string& fileName) {
  for (const auto& wrapper : clientSockets_) {
    std::string operationStr;

    switch (operation) {
      case FileOperation::LIST:
        operationStr = "LIST";
        break;
      case FileOperation::DELETE:
        operationStr = "DELETE";
        break;
      case FileOperation::SEND:
        operationStr = "SEND";
        break;
      case FileOperation::CREATE:
        operationStr = "CREATE";
        break;
      case FileOperation::UPDATE:
        operationStr = "UPDATE";
        break;
      case FileOperation::UPDATED:
        operationStr = "UPDATED";
        break;
      default:
        operationStr = "ERROR";
        break;
    }

    const int fileNameLength = fileName.length(),
              operationLength = operationStr.length();

    zmq::message_t operationMessage(operationStr.c_str(), operationLength),
        fileNameMessage(fileName.c_str(), fileNameLength);

    // std::cout << "Sending " << operationMessage.to_string() << " "
    //           << fileNameMessage.to_string() << std::endl;

    auto res =
        wrapper->getSocket()->send(operationMessage, zmq::send_flags::sndmore);
    res = wrapper->getSocket()->send(fileNameMessage, zmq::send_flags::none);

    zmq_pollitem_t items[] = {{*(wrapper->getSocket()), 0, ZMQ_POLLIN, 0}};
    int rc = zmq_poll(items, 1, std::chrono::milliseconds(TIMEOUT_MS).count());

    if (rc == -1) {
      // Error during polling
      std::cerr << "Error during polling: " << zmq_strerror(zmq_errno())
                << std::endl;
      return;
    } else if (rc == 0) {
      // Timeout reached, no response from server
      std::cerr << "Timeout waiting for" << wrapper->getIp()
                << "'s response. Proceeding." << std::endl;
      return;
    }
    // get reply
    std::vector<zmq::message_t> recv_msgs;

    const auto ret = zmq::recv_multipart(*wrapper->getSocket(),
                                         std::back_inserter(recv_msgs));
    if (!ret) std::cerr << "Error accepting message (Sender)" << std::endl;
    // std::cout << "Got " << *ret << " messages" << std::endl;

    std::vector<std::string> messagesStr;

    for (zmq::message_t& msg : recv_msgs) {
      messagesStr.push_back(msg.to_string());
    }

    // for (std::string messageString : messagesStr) {
    //   std::cout << " Recieved (Sender): " << messageString << std::endl;
    // }
    if (operationStr == "LIST") {
      // convert string to json then json to vector
      std::string received_data(static_cast<char*>(recv_msgs[0].data()),
                                recv_msgs[0].size());
      Json::CharReaderBuilder builder;
      Json::Value jsonMap;
      std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
      std::string errors;
      if (!reader->parse(received_data.c_str(),
                         received_data.c_str() + received_data.size(), &jsonMap,
                         &errors)) {
        std::cerr << "Failed to parse JSON: " << errors << std::endl;
        // todo error handling
      }
      std::map<std::string, NodeFileSystem::fileMetadata> fileMdata;
      for (const auto& key : jsonMap.getMemberNames()) {
        fileMdata[key] = NodeFileSystem::fileMetadata::fromJson(jsonMap[key]);
      }
      // insert into other map
      // todo check for collisions
      otherFileMData.insert(fileMdata.begin(), fileMdata.end());
    }

    //   if (operationStr == "DELETE") {
    //   }
    if (operationStr == "SEND") {
      // Open file to write
      // If file wasnt found do nothing!
      if (messagesStr[0] != "FILE WAS NOT FOUND.") {
        std::string fileNameCopy = "copyof" + fileName;
        std::ofstream file(rootDir_ / fileNameCopy, std::ios::binary);
        if (!file.is_open()) {
          std::cerr << "Failed to open file for writing.\n";
          // todo error handling
        }
        int count = 0;
        for (zmq::message_t& msg : recv_msgs) {
          // Access message data directly
          const char* msgData = static_cast<const char*>(msg.data());

          file.write(msgData, msg.size());
          count++;
        }
        file.close();
        // std::cout << fileName << " was successfully recieved." << std::endl;
        NodeFileSystem::fileMetadata tempMd;
        tempMd = fileSystem_.getFileMetaData("copyof" + fileName);
        tempMd.storedIpAddress = wrapper->getIp();
        myFileMdata["copyof" + fileName] = tempMd;
      }
    }
    if (operationStr == "UPDATED") {
      // convert string to json then json to vector
      std::string received_data(static_cast<char*>(recv_msgs[0].data()),
                                recv_msgs[0].size());
      Json::CharReaderBuilder builder;
      Json::Value jsonMap;
      std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
      std::string errors;
      if (!reader->parse(received_data.c_str(),
                         received_data.c_str() + received_data.size(), &jsonMap,
                         &errors)) {
        std::cerr << "Failed to parse JSON: " << errors << std::endl;
        // todo error handling
      }
      std::map<std::string, NodeFileSystem::fileMetadata> fileMdata;
      for (const auto& key : jsonMap.getMemberNames()) {
        fileMdata[key] = NodeFileSystem::fileMetadata::fromJson(jsonMap[key]);
      }
      // insert into other map

      for (auto entry : fileMdata) {
        otherFileMData[entry.first] = entry.second;
      }
    }
  }
}

std::map<std::string, NodeFileSystem::fileMetadata> Node::getMyFileData() {
  return myFileMdata;
}

std::map<std::string, NodeFileSystem::fileMetadata> Node::getOtherFileData() {
  return otherFileMData;
}

void Node::addFileData() {}

void Node::setFileData(
    std::map<std::string, NodeFileSystem::fileMetadata> fileMData) {
  myFileMdata = fileMData;
}

void Node::createFile(std::string fileName) {
  if (myFileMdata.find(fileName) != myFileMdata.end()) {
    std::cerr << fileName << " already exists. File was not created."
              << std::endl;
  } else {
    NodeFileSystem::fileMetadata fileMetadata =
        fileSystem_.createFile(fileName);
    fileMetadata.storedIpAddress = ipAddress_ + ":" + std::to_string(port_);
    myFileMdata.insert({fileName, fileMetadata});
  }
}

void Node::deleteFile(std::string fileName) {
  if (myFileMdata.find(fileName) == myFileMdata.end()) {
    std::cerr << fileName << " was not found. File was not deleted."
              << std::endl;
  } else {
    fileSystem_.deleteFile(fileName);
    auto it = myFileMdata.find(fileName);
    myFileMdata.erase(it);
  }
}

void Node::readFile(std::string fileName) {
  if (!std::filesystem::exists(rootDir_ / fileName)) {
    sendRequest(Node::FileOperation::SEND, fileName);
    std::string copyFileNameStr = "copyof" + fileName;
    if (std::filesystem::exists(rootDir_ / copyFileNameStr)) {
      fileSystem_.readFile(copyFileNameStr);
      fileSystem_.deleteFile(copyFileNameStr);
      return;
    }
  }
  if (std::filesystem::exists(rootDir_ / fileName)) {
    fileSystem_.readFile(fileName);
  } else {
    std::cerr << fileName
              << " does not exist on this node or any other online node."
              << std::endl;
  }
}

void Node::getFile(std::string fileName) {
  if (std::filesystem::exists(rootDir_ / fileName)) {
    std::cout
        << "File already exists within node. To read use the \"read\" command."
        << std::endl;
        return;
  } else if (!std::filesystem::exists(rootDir_ / fileName)) {
    sendRequest(Node::FileOperation::SEND, fileName);
    refresh();
    return;
  }

  std::cerr << fileName
            << " does not exist on this node or any other online node."
            << std::endl;
}

template <typename T>
void printElement(T t, const int& width) {
  const char separator = ' ';
  std::cout << std::left << std::setw(width) << std::setfill(separator) << t
            << separator;
}

void printLine(std::string fileName, NodeFileSystem::fileMetadata metaData,
               bool onNode) {
  const int smallWidth = 10;
  const int largeWidth = 20;
  printElement(fileName, largeWidth);
  if (onNode) {
    printElement("Yes", 10);
  } else {
    printElement("No", 10);
  }
  printElement(metaData.storedIpAddress, largeWidth);
  printElement(metaData.fileSize, smallWidth);
  printElement(metaData.lastModified, largeWidth);
}

// Format:
// Name | On Node | IP | File size | Last modified
void Node::listFiles() {
  otherFileMData.clear();
  sendRequest(Node::FileOperation::LIST, "No File Name");

  printElement("Name", 20);
  printElement("On Node", 10);
  printElement("Stored IP", 20);
  printElement("Size (kb)", 10);
  printElement("Last Modified", 20);
  std::cout << std::endl;
  for (auto [filename, metadata] : myFileMdata) {
    printLine(filename, metadata, true);
    std::cout << std::endl;
  }
  for (auto [filename, metadata] : otherFileMData) {
    printLine(filename, metadata, false);
    std::cout << std::endl;
  }
}

void Node::refresh() {
  myFileMdata.clear();
  for (const auto& entry : std::filesystem::directory_iterator(rootDir_)) {
    NodeFileSystem::fileMetadata tempMd;
    tempMd = fileSystem_.getFileMetaData(entry.path().filename());
    myFileMdata[entry.path().filename()] = tempMd;
  }
  for (auto& [filename, metadata] : myFileMdata) {
    metadata.storedIpAddress = ipAddress_ + ":" + std::to_string(port_);
  }
}

void Node::update() {
  refresh();
  sendRequest(Node::FileOperation::UPDATE, "");
}

SocketWrapper::SocketWrapper(zmq::socket_t* socket, std::string ip, int port)
    : socket_(socket) {
  ip_ = ip + ":" + std::to_string(port);
}

SocketWrapper::~SocketWrapper() { delete socket_; }

zmq::socket_t* SocketWrapper::getSocket() const { return socket_; }

std::string SocketWrapper::getIp() { return ip_; }
