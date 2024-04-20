// Hello World server
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <unistd.h>

int main() {
    // Prepare the context and the socket
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REP);
    socket.bind("tcp://*:5555");

    while (true) {
        zmq::message_t request;

        // Wait for next request from the client
        socket.recv(request);
        std::string recvStr(static_cast<char*>(request.data()), request.size());
        std::cout << "Received " << recvStr << std::endl;

        // Do some 'work'
        sleep(1);

        // Send reply back to the client
        zmq::message_t reply(5);
        memcpy(reply.data(), "World", 5);
        socket.send(reply);

        if (recvStr.find("9") != std::string::npos) {
           break;
        }

    }
    
    socket.close();
    context.close();

    return 0;
}