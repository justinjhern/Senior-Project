#include <zmq.hpp>
#include <iostream>
#include <string>

int main() {
  // Create a ZeroMQ context
  zmq::context_t context(1);

  // Create a request socket
  zmq::socket_t requester(context, ZMQ_REQ);

  // Connect the socket to the server
  requester.connect("tcp://localhost:5555");
  std::cout << "Connecting to hello world server..." << std::endl;

  // Send and receive messages 10 times
  for (int request_nbr = 0; request_nbr != 10; request_nbr++) {
    std::string message = "Hello " + std::to_string(request_nbr);
    std::cout << "Sending " << message << std::endl;

    // Send the message
    requester.send(zmq::buffer(message));

    // Receive the reply
    zmq::message_t reply;
    requester.recv(&reply);

    std::string reply_str(static_cast<char*>(reply.data()), reply.size());
    std::cout << "Received " << reply_str << std::endl;
  }

  // Close the socket and context
  requester.close();
  context.close();

  return 0;
}