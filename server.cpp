
#include <iostream>
#include <string>
#include <unordered_map>
#include <thread>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

// Global map: username -> socket
std::unordered_map<std::string, std::unique_ptr<tcp::socket>> connected_clients;

// Read a full line (until '\n') from a socket.
std::string read_line(tcp::socket &sock) {
    boost::asio::streambuf buffer;
    boost::system::error_code error;
    
    size_t bytes_transferred = boost::asio::read_until(sock, buffer, "\n", error);
    if (error) return ""; // Return empty if there's an error or disconnect.

    std::istream is(&buffer);
    std::string line;
    std::getline(is, line);
    return line;
}

// Handle a client session (runs in its own thread)
void client_session(std::unique_ptr<tcp::socket> sock) {
    try {
        // 1) Read username from the client
        std::string username = read_line(*sock);
        if (username.empty()) {
            std::cerr << "Invalid or empty username. Disconnecting client.\n";
            return;
        }

        // 2) If username already exists, close the old socket
        if (connected_clients.find(username) != connected_clients.end()) {
            std::cout << "User " << username << " reconnected. Closing old session.\n";
            connected_clients[username]->close();
            connected_clients.erase(username); // Remove old socket entry
        }

        // 3) Store the new socket
        connected_clients.emplace(username, std::move(sock));
        std::cout << "User " << username << " connected.\n";

        // Reference to the stored socket
        tcp::socket &client_socket = *connected_clients[username];

        // 4) Keep reading messages
        while (true) {
            std::string line = read_line(client_socket);
            if (line.empty()) {
                std::cout << "User '" << username << "' disconnected.\n";
                connected_clients.erase(username);
                return;
            }

            // 5) Expect "ReceiverName message"
            auto spacePos = line.find(' ');
            if (spacePos == std::string::npos) {
                continue; // Ignore invalid messages
            }

            std::string receiverName = line.substr(0, spacePos);
            std::string messageBody = line.substr(spacePos + 1);

            // 6) Look up the receiver's socket
            auto it = connected_clients.find(receiverName);
            if (it != connected_clients.end()) {
                std::string outgoing = username + ": " + messageBody + "\n";
                boost::asio::write(*it->second, boost::asio::buffer(outgoing));
            } else {
                std::string errorMsg = "Server: User '" + receiverName + "' not found.\n";
                boost::asio::write(client_socket, boost::asio::buffer(errorMsg));
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error in client session: " << e.what() << "\n";
    }
}

int main() {
    try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8888));

        std::cout << "Server started on port 8888\n";

        while (true) {
            auto client = std::make_unique<tcp::socket>(io);
            acceptor.accept(*client);

            std::thread(client_session, std::move(client)).detach();
        }
    } catch (const std::exception &e) {
        std::cerr << "Server exception: " << e.what() << "\n";
    }
    return 0;
}

