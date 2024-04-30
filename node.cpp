#include "node.hpp"

#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <zmq.hpp>

#include "node_filesystem.hpp"

const size_t CHUNK_SIZE = 1024;  // 1 kb

Node::Node(const std::filesystem::path& rootDir,
           const std::vector<std::pair<std::string, int>>& initialTargetNodes,
           const std::string ipAddress, int port)
    : rootDir_(rootDir),
      targetNodes_(initialTargetNodes),
      fileSystem_(NodeFileSystem(rootDir)),
      ipAddress_(ipAddress),
      port_(port) {
  myFileMdata = fileSystem_.getFileMetadata();
  for (auto& [filename, metadata] : myFileMdata) {
    metadata.storedIpAddress = ipAddress_;
  }
  Node::initialize();
}

Node::~Node() {
  clientSocket_.close();
  serverSocket_.close();
}

void Node::initialize() {
  // todo change to fix ip address
  std::cout << "Initializing Node to bind to tcp://localhost:" << port_ << "."
            << std::endl;
  context_ = zmq::context_t(1);
  serverSocket_ = zmq::socket_t(context_, zmq::socket_type::router);
  // todo
  // replace with {public ip}:{port}
  serverSocket_.bind("tcp://*:" + std::to_string(port_));
  clientSocket_ = zmq::socket_t(context_, zmq::socket_type::dealer);

  for (const auto& target : targetNodes_) {
    std::string targetIp = target.first;
    int targetPort = target.second;
    // {public ip}:{port}
    clientSocket_.connect(fmt::format("tcp://{}:{}", targetIp, targetPort));
  }
}

/*
 * Listens to requests infinitely
 * Requests Handled:
 *   READ: Returns
 *   DELETE: Removes file from filesystem
 *   LIST: Replies with JSON of the file and metadata map
 *   CREATE: Creates a file in a filesystem
 *   WROTE: Tells other servers a file has been edited and send a request for it
 *   WRITE: Sends the file content to replace written files
 */
void Node::handleRequests() {
  std::cout << " Beginning loops " << std::endl;
  // zmq::pollitem_t items[] = {{serverSocket_, 0, ZMQ_POLLIN, 0}};
  while (true) {
    std::vector<zmq::message_t> recv_msgs;

    const auto ret =
        zmq::recv_multipart(serverSocket_, std::back_inserter(recv_msgs));
    if (!ret) std::cout << "Error accepting message (Handler)" << std::endl;
    std::cout << "Got " << *ret << " messages" << std::endl;

    std::vector<std::string> messagesStr;

    for (zmq::message_t& msg : recv_msgs) {
      messagesStr.push_back(msg.to_string());
    }

    for (std::string messageString : messagesStr) {
      std::cout << " Recieved (Handler): " << messageString << std::endl;
    }

    // return flag
    zmq::message_t replyMsg(recv_msgs[0].data(), recv_msgs[0].size());
    auto res = serverSocket_.send(replyMsg, zmq::send_flags::sndmore);

    // const int msglength = msg.length();

    // zmq::message_t msg(operationStr.c_str(), operationLength);

    // std::cout << "Sending " << msg.to_string() << std::endl'

    // auto res = serverSocket_.send(operationMessage,
    // zmq::send_flags::sndmore);

    // std::string replyStr = "I AM REPLYING HI HI!";
    // zmq::message_t msg(replyStr.c_str(), replyStr.length());
    //  res = serverSocket_.send(msg, zmq::send_flags::none);

    // // set reply message
    if (messagesStr[1] == "SEND") {
      std::string filename = messagesStr[2];
      if (std::filesystem::exists(rootDir_ / filename)) {
        std::ifstream file(rootDir_ / filename, std::ios::binary);
        std::cout << "opening " << filename << std::endl;
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
            std::cout << "Sending Bytes: " << bytes_read << std::endl;
            zmq::message_t msg(buffer, bytes_read);
            auto res = serverSocket_.send(msg, zmq::send_flags::sndmore);
          } else {
            // Last chunk - send without ZMQ_SNDMORE flag
            std::cout << "Sending Last Byte: " << bytes_read << std::endl;
            zmq::message_t msg(buffer, bytes_read);
            auto res = serverSocket_.send(msg, zmq::send_flags::none);
            break;
          }
        }
        file.close();
        std::cout << "Done sending: " << filename << std::endl;
      } else {  // if file was not found
        zmq_msg_t replyMsg;
        std::string endFileStr = "FILE WAS NOT FOUND.";

        zmq::message_t msg(endFileStr.c_str(), endFileStr.length());
        auto res = serverSocket_.send(msg, zmq::send_flags::none);
      }
    }
    if (messagesStr[1] == "DELETE") {
      std::string reply;

      std::string deletedFile = fileSystem_.deleteFile(messagesStr[2]);
      if (deletedFile != "-1") {
        reply = "Deleted file: " + deletedFile;
      } else {
        reply = "Failed to delete file: " + messagesStr[2];
      }

      zmq::message_t msg(reply.c_str(), reply.length());

      std::cout << "Sending " << msg.to_string() << std::endl;

      auto res = serverSocket_.send(msg, zmq::send_flags::none);
    }
    if (messagesStr[1] == "LIST") {
      Json::Value jsonMap(Json::objectValue);
      for (const auto& [filename, metadata] : myFileMdata) {
        Json::Value metadataJson(Json::objectValue);
        // Add each metadata field (owner, creationTime, size, etc.) to the
        // metadataJson object
        metadataJson["fileSize"] = metadata.fileSize;
        metadataJson["lastModified"] = metadata.lastModified;
        metadataJson["fileStoredIp"] = metadata.storedIpAddress;
        // ... add other metadata fields ...

        jsonMap[filename] = metadataJson;
      }
      // serialize JSON to string
      Json::StreamWriterBuilder builder;
      std::string jsonString = Json::writeString(builder, jsonMap);

      // send jsonstring
      zmq::message_t msg(jsonString.c_str(), jsonString.length());

      std::cout << "Sending " << msg.to_string() << std::endl;

      auto res = serverSocket_.send(msg, zmq::send_flags::none);
    }
    if (messagesStr[1] == "CREATE") {
      fileSystem_.createFile(messagesStr[2]);

      std::string reply = "Created file: " + messagesStr[2];

      zmq::message_t msg(reply.c_str(), reply.length());

      std::cout << "Sending " << msg.to_string() << std::endl;

      auto res = serverSocket_.send(msg, zmq::send_flags::none);
    }
  }
}

// will send two messages, first with operation, second with file name
void Node::sendRequest(FileOperation operation, const std::string& fileName) {
  for (auto node : targetNodes_) {
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
      default:
        operationStr = "ERROR";
        break;
    }

    const int fileNameLength = fileName.length(),
              operationLength = operationStr.length();

    zmq::message_t operationMessage(operationStr.c_str(), operationLength),
        fileNameMessage(fileName.c_str(), fileNameLength);

    std::cout << "Sending " << operationMessage.to_string() << " "
              << fileNameMessage.to_string() << std::endl;

    auto res = clientSocket_.send(operationMessage, zmq::send_flags::sndmore);
    res = clientSocket_.send(fileNameMessage, zmq::send_flags::none);

    // get reply
    std::vector<zmq::message_t> recv_msgs;

    const auto ret =
        zmq::recv_multipart(clientSocket_, std::back_inserter(recv_msgs));
    if (!ret) std::cout << "Error accepting message (Sender)" << std::endl;
    std::cout << "Got " << *ret << " messages" << std::endl;

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

      // std::string outputString;
      // for (const auto& [filename, metadata] : otherFileMData) {
      //   // Format the output string for each entry
      //   outputString += "Filename: " + filename + "\n";
      //   outputString +=
      //       "  Stored IP Address: " + metadata.storedIpAddress + "\n";
      //   outputString +=
      //       "  File Size: " + std::to_string(metadata.fileSize) + "bytes\n";
      //   outputString += "  Last Modified: " + metadata.lastModified + "\n\n";
      // }

      // // Print the accumulated output string
      // std::cout << "output string is \n" << outputString << std::endl;
    }

    //   if (operationStr == "DELETE") {
    //   }
    if (operationStr == "SEND") {
      // Open file to write
      // todo change for testing
      std::ofstream file("testrecieved.bmp", std::ios::binary);
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
      std::cout << "Got here 5" << std::endl;
      file.close();
      std::cout << "Got here 6" << std::endl;
    }
    if (operationStr == "CREATE") {
    }
  }
}
