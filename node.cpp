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
           std::string ipAddress, int port)
    : rootDir_(rootDir),
      targetNodes_(initialTargetNodes),
      fileSystem_(NodeFileSystem(rootDir)),
      ipAddress_(ipAddress),
      port_(port) {
  myFileMdata = fileSystem_.getFileMetadata();
  for (auto& [filename, metadata] : myFileMdata) {
    metadata.storedIpAddress = ipAddress_;
  }
}

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
  // todo
  // replace with {public ip}:{port}
  serverSocket_.bind("tcp://*:" + std::to_string(port_));
  clientSocket_ = zmq::socket_t(context_, ZMQ_DEALER);

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

  zmq::pollitem_t items[] = {{serverSocket_, 0, ZMQ_POLLIN, 0}};
  while (true) {
    int more;
    size_t more_size = sizeof(more);
    // message is {id of socket? (very likely), operation, filename}
    std::vector<zmq_msg_t> messages;
    do {
      /* Create an empty 0MQ message to hold the message part */
      zmq_msg_t part;
      int rc = zmq_msg_init(&part);
      assert(rc == 0);
      /* Block until a message is available to be received from socket */
      rc = zmq_msg_recv(&part, serverSocket_, 0);
      assert(rc != -1);

      // Add the string to the vector
      messages.push_back(part);

      /* Determine if more message parts are to follow */
      rc = zmq_getsockopt(serverSocket_, ZMQ_RCVMORE, &more, &more_size);
      assert(rc == 0);
      zmq_msg_close(&part);
    } while (more);

    // messageStr[0] will always be the reply tag, operation then filename
    std::vector<std::string> messagesStr;

    for (zmq_msg_t msg : messages) {
      std::string messageStr(static_cast<char*>(zmq_msg_data(&msg)),
                             zmq_msg_size(&msg));
      messagesStr.push_back(messageStr);
    }

    for (std::string messageString : messagesStr) {
      std::cout << " Recieved (Handler): " << messageString << std::endl;
    }

    // set reply message
    if (messagesStr[1] == "SEND") {
      int rc = zmq_msg_send(&messages[0], serverSocket_, ZMQ_SNDMORE);
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
          zmq_msg_t replyMsg;
          if (!file.eof()) {
            // Send chunk (not the last one)
            int rc = zmq_msg_init_size(&replyMsg, bytes_read);
            assert(rc == 0);
            memcpy(zmq_msg_data(&replyMsg), buffer, bytes_read);
            rc = zmq_msg_send(&replyMsg, serverSocket_, ZMQ_SNDMORE);
            assert(rc != -1);
          } else {
            // Last chunk - send without ZMQ_SNDMORE flag
            int rc = zmq_msg_init_size(&replyMsg, bytes_read);
            assert(rc == 0);
            memcpy(zmq_msg_data(&replyMsg), buffer, bytes_read);
            rc = zmq_msg_send(&replyMsg, serverSocket_, 0);
            assert(rc != -1);
            zmq_msg_close(&replyMsg);
            break;
          }
          zmq_msg_close(&replyMsg);
        }
        file.close();

        std::cout << "Done sending: " << filename << std::endl;
      } else {  // if file was not found
        zmq_msg_t replyMsg;
        std::string endFileStr = "FILE WAS NOT FOUND.";

        char endFileCStr[endFileStr.length() + 1];
        strcpy(endFileCStr, endFileStr.c_str());

        rc = zmq_msg_init_size(&replyMsg, endFileStr.length());
        memcpy(zmq_msg_data(&replyMsg), endFileCStr, endFileStr.length());
        rc = zmq_msg_send(&replyMsg, serverSocket_, 0);
        zmq_msg_close(&replyMsg);
      }
    } else if (messagesStr[1] == "DELETE") {
      int rc = zmq_msg_send(&messages[0], serverSocket_, ZMQ_SNDMORE);
      zmq_msg_t replyMsg;
      std::string reply;

      std::string deletedFile = fileSystem_.deleteFile(messagesStr[2]);
      if (deletedFile != "-1") {
        reply = "Deleted file: " + deletedFile;
      } else {
        reply = "Failed to delete file: " + messagesStr[2];
      }
      char replyCStr[reply.length() + 1];
      strcpy(replyCStr, reply.c_str());

      rc = zmq_msg_init_size(&replyMsg, reply.length());
      assert(rc == 0);
      memcpy(zmq_msg_data(&replyMsg), replyCStr, reply.length());
      rc = zmq_msg_send(&replyMsg, serverSocket_, 0);
      zmq_msg_close(&replyMsg);

    } else if (messagesStr[1] == "LIST") {
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

      std::cout << "JSON String: " << jsonString << std::endl;

      int rc = zmq_msg_send(&messages[0], serverSocket_, ZMQ_SNDMORE);

      zmq_msg_t replyMsg;

      char jsonCStr[jsonString.length() + 1];
      strcpy(jsonCStr, jsonString.c_str());

      rc = zmq_msg_init_size(&replyMsg, jsonString.length());
      assert(rc == 0);

      memcpy(zmq_msg_data(&replyMsg), jsonCStr, jsonString.length());

      rc = zmq_msg_send(&replyMsg, serverSocket_, 0);
      zmq_msg_close(&replyMsg);

    } else if (messagesStr[1] == "CREATE") {
      fileSystem_.createFile(messagesStr[2]);
      int rc = zmq_msg_send(&messages[0], serverSocket_, ZMQ_SNDMORE);
      zmq_msg_t replyMsg;

      std::string reply = "Created file: " + messagesStr[2];
      char replyCStr[reply.length() + 1];
      strcpy(replyCStr, reply.c_str());

      rc = zmq_msg_init_size(&replyMsg, reply.length());
      assert(rc == 0);
      rc = zmq_msg_send(&replyMsg, serverSocket_, 0);
      zmq_msg_close(&replyMsg);
    }

    for (zmq_msg_t msg : messages) {
      zmq_msg_close(&msg);
    }
  }
}

// will send two messages, first with operation, second with file name
void Node::sendRequest(FileOperation operation, const std::string& fileName) {
  for (auto node : targetNodes_) {
    std::string operationStr;
    zmq_msg_t operationMessage, fileNameMessage;

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

    int rc = zmq_msg_init_size(&operationMessage, operationLength);
    assert(rc == 0);
    rc = zmq_msg_init_size(&fileNameMessage, fileNameLength);
    assert(rc == 0);

    char operationCStr[operationLength + 1];
    strcpy(operationCStr, operationStr.c_str());

    char fileNameCStr[fileNameLength + 1];
    strcpy(fileNameCStr, fileName.c_str());

    memcpy(zmq_msg_data(&operationMessage), operationCStr, operationLength);
    memcpy(zmq_msg_data(&fileNameMessage), fileNameCStr, fileNameLength);

    std::cout << "Sending " << operationCStr << " " << fileNameCStr
              << std::endl;
    rc = zmq_msg_send(&operationMessage, clientSocket_, ZMQ_SNDMORE);
    assert(rc == strlen(operationCStr));

    zmq_msg_close(&operationMessage);

    rc = zmq_msg_send(&fileNameMessage, clientSocket_, 0);
    assert(rc == strlen(fileNameCStr));

    zmq_msg_close(&fileNameMessage);

    // get reply

    int more;
    size_t more_size = sizeof(more);
    std::vector<zmq_msg_t> messages;
    int counter = 0;
    do {
      zmq_msg_t part;
      rc = zmq_msg_init(&part);
      rc = zmq_msg_init_size(&part, CHUNK_SIZE);
      /* Block until a message is available to be received from socket */
      rc = zmq_msg_recv(&part, clientSocket_, 0);
      assert(rc != -1);

      // Add the string to the vector
      messages.push_back(part);
      /* Determine if more message parts are to follow */
      rc = zmq_getsockopt(clientSocket_, ZMQ_RCVMORE, &more, &more_size);
      assert(rc == 0);
      std::cout << "part size: " << zmq_msg_size(&part) << std::endl;
      zmq_msg_close(&part);
    } while (more);

    for (zmq_msg_t msg : messages) {
      std::cout << "message size: " << zmq_msg_size(&msg) << std::endl;
    }
    if (operationStr == "LIST") {
      // convert string to json then json to vector
      std::string received_data(static_cast<char*>(zmq_msg_data(&messages[0])),
                                zmq_msg_size(&messages[0]));
      Json::CharReaderBuilder builder;
      Json::Value jsonMap;
      std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
      std::string errors;
      if (!reader->parse(received_data.c_str(),
                         received_data.c_str() + received_data.size(), &jsonMap,
                         &errors)) {
        std::cerr << "Failed to parse JSON: " << errors << std::endl;
        // todo error handling
        std::map<std::string, NodeFileSystem::fileMetadata> fileMdata;
        for (const auto& key : jsonMap.getMemberNames()) {
          fileMdata[key] = NodeFileSystem::fileMetadata::fromJson(jsonMap[key]);
        }
        // insert into other map
        // todo check for collisions
        otherFileMData.insert(fileMdata.begin(), fileMdata.end());
      }
    }
    if (operationStr == "DELETE") {
    }
    if (operationStr == "SEND") {
      // Open file to write
      // todo change for testing
      std::ofstream file("testrecieved.txt", std::ios::binary);
      if (!file.is_open()) {
        std::cerr << "Failed to open file for writing.\n";
        // todo error handling
      }
      char buffer[CHUNK_SIZE * 2];  // 2 kb of data for buffer
      int count = 0;
      std::cout << "Size of vector: " << messages.size() << std::endl;
      for (zmq_msg_t msg : messages) {
        std::cout << "Iteration: " << count << std::endl;
        size_t message_size = zmq_msg_size(&msg);
        // Access message data directly
        if (message_size > CHUNK_SIZE * 2) {
          std::cerr
              << "Error: Message size exceeds buffer capacity. Message size: "
              << message_size << "Buffer size: " << CHUNK_SIZE * 2 << std::endl;
          continue;
        }

        memcpy(buffer, zmq_msg_data(&msg), message_size);
        file.write(buffer, message_size);
        memset(buffer, 0, CHUNK_SIZE * 2);
        count++;
      }
      file.close();
    }
    if (operationStr == "CREATE") {
    }
    std::vector<std::string> messagesStr;
    // Convert message to string
    for (zmq_msg_t msg : messages) {
      // std::string messageStr(static_cast<char*>(zmq_msg_data(&msg)),
      //                        zmq_msg_size(&msg));
      // messagesStr.push_back(messageStr);
      zmq_msg_close(&msg);
    }

    // for (std::string msg : messagesStr) {
    //   std::cout << " Recieved (sender): " << msg << std::endl;
    // }
  }
}
