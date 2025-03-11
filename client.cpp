
#include <iostream>
#include <string>
#include <thread>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

// Thread function: continuously read lines from the server socket and print them.
void receive_messages(tcp::socket &socket)
{
    boost::system::error_code error;
    while (true)
    {
        boost::asio::streambuf buffer;
        // read_until will block until we get a newline or an error
        size_t bytes = boost::asio::read_until(socket, buffer, "\n", error);
        if (error) {
            std::cout << "Disconnected from server or error reading.\n";
            return; // ends the thread
        }
        
        std::istream is(&buffer);
        std::string line;
        std::getline(is, line);

        // Print out the message from server
        std::cout << line << std::endl;
    }
}

int main()
{
    try {
        // 1) Set up Boost.Asio
        boost::asio::io_context io;
        tcp::socket socket(io);

        // 2) Connect to the server
        socket.connect(tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 8888));
        std::cout << "Connected to server.\n";

        // 3) Ask for a username
        std::string username;
        std::cout << "Enter your username: ";
        std::getline(std::cin, username);

        // 4) Send username to server
        boost::asio::write(socket, boost::asio::buffer(username + "\n"));

        // 5) Start a thread to listen for incoming messages from the server
        std::thread receiverThread(receive_messages, std::ref(socket));

        // 6) Main loop: read lines from the user and send to the server
        std::cout << "Type messages in the format: receiverName message...\n";
        while (true)
        {
            std::string line;
            std::getline(std::cin, line);

            if (line.empty()) {
                // skip empty lines
                continue;
            }

            // Send line to server
            boost::asio::write(socket, boost::asio::buffer(line + "\n"));
        }

        // 7) (Unreachable in this example, but if you ever break, join the thread)
        receiverThread.join();

    } catch (std::exception &e) {
        std::cerr << "Client exception: " << e.what() << "\n";
    }
    return 0;
}

