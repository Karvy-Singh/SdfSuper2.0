#include <iostream>
#include <string>
#include <thread>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class ChatClient {
private:
  boost::asio::io_context io_;
  tcp::socket socket_;
  std::string host_;
  unsigned short int port_;
  bool running_{true};
  std::string user_;
  std::array<char, 9> read_header_buf_;

  void doReadHeader();
  void doReadBody(std::size_t length, uint8_t type);
  bool validateMagic(const std::array<char, 4> &magic);
  void handleServerMessage(uint8_t type, const std::string &value);
  void sendPacket(uint8_t type, const std::string &value);

public:
  ChatClient(const std::string &host, unsigned short int port)
      : socket_(io_), host_(host), port_(port) {}
  bool connect() {
    try {
      socket_ = tcp::socket(io_);
      socket_.connect(
          tcp::endpoint(boost::asio::ip::make_address(host_), port_));
    } catch (std::exception &e) {
      std::cout << "[Client] ERROR: " << e.what() << std::endl;
      return false;
    }

    doReadHeader();
    return true;
  }

  void run() {
    std::cout << "Enter your username: ";
    std::getline(std::cin, user_);

    sendPacket(0x01, user_);

    std::thread io_thread([this]() { io_.run(); });

    while (running_) {
      std::string line;
      if (!std::getline(std::cin, line)) {
        running_ = false;
        break;
      }
      if (line.empty())
        continue;

      sendPacket(0x02, line);
    }
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    io_.stop();
    if (io_thread.joinable())
      io_thread.join();
  }
};

void ChatClient::doReadHeader() {
  auto self = this;
  boost::asio::async_read(
      socket_, boost::asio::buffer(read_header_buf_),
      [this, self](const boost::system::error_code &ec,
                   std::size_t bytes_read) {
        if (ec || bytes_read != read_header_buf_.size()) {
          running_ = false;
          return;
        }

        // Parse magic (it not fscking magic)
        std::array<char, 4> magic;
        std::memcpy(magic.data(), read_header_buf_.data(), 4);
        if (!validateMagic(magic)) {
          std::cerr << "[Client] Invalid Magic. Disconnecting.\n";
          running_ = false;
          return;
        }

        // Type (no)
        uint8_t type = static_cast<uint8_t>(read_header_buf_[4]);

        // Length (very big)
        uint32_t length = 0;
        length |= (static_cast<unsigned char>(read_header_buf_[5]) << 24);
        length |= (static_cast<unsigned char>(read_header_buf_[6]) << 16);
        length |= (static_cast<unsigned char>(read_header_buf_[7]) << 8);
        length |= (static_cast<unsigned char>(read_header_buf_[8]));

        if (length == 0) {
          handleServerMessage(type, "");
          if (running_)
            doReadHeader();
        } else {
          doReadBody(length, type);
        }
      });
}

void ChatClient::doReadBody(std::size_t length, uint8_t type) {
  auto self = this;
  auto body_buf = std::make_shared<std::vector<char>>(length);
  boost::asio::async_read(
      socket_, boost::asio::buffer(*body_buf),
      [this, self, body_buf, type](const boost::system::error_code &ec,
                                   std::size_t bytes_read) {
        if (ec || bytes_read != body_buf->size()) {
          running_ = false;
          return;
        }

        std::string value(body_buf->data(), body_buf->size());
        handleServerMessage(type, value);

        if (running_)
          doReadHeader();
      });
}

bool ChatClient::validateMagic(const std::array<char, 4> &magic) {
  return (magic[0] == 'J' && magic[1] == 'I' && magic[2] == 'I' &&
          magic[3] == 'T');
}

void ChatClient::sendPacket(uint8_t type, const std::string &value) {
  std::vector<uint8_t> packet;
  // magic (sleek)
  packet.push_back('J');
  packet.push_back('I');
  packet.push_back('I');
  packet.push_back('T');
  // type
  packet.push_back(type);
  // length (very big)
  uint32_t len = static_cast<uint32_t>(value.size());
  packet.push_back((len >> 24) & 0xFF);
  packet.push_back((len >> 16) & 0xFF);
  packet.push_back((len >> 8) & 0xFF);
  packet.push_back(len & 0xFF);
  // value
  for (char c : value) {
    packet.push_back(static_cast<uint8_t>(c));
  }

  boost::system::error_code ec;
  boost::asio::write(socket_, boost::asio::buffer(packet), ec);
  if (ec) {
    std::cerr << "[Client] Send failed: " << ec.message() << std::endl;
    running_ = false;
  }
}

void ChatClient::handleServerMessage(uint8_t type, const std::string &value) {
  switch (type) {
  case 0x02: // chat message
    std::cout << value << std::endl;
    break;
  case 0xff: // server messages
    std::cout << "[Server] " << value << std::endl;
    break;
  default:
    std::cout << "[Client] ERROR: Server sent a packet which this version of "
                 "client does not understand."
              << std::endl;
    break;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
    return 1;
  }

  const std::string host = argv[1];
  const unsigned short port = static_cast<unsigned short>(std::stoi(argv[2]));

  try {
    ChatClient client(host, port);
    if (!client.connect()) {
      std::cerr << "[Client] Failed to connect to " << host << ":" << port
                << std::endl;
      return 1;
    }

    std::cout << "[Client] Connected to " << host << ":" << port << std::endl;
    client.run();
  } catch (std::exception &e) {
    std::cerr << "[Client] Exception: " << e.what() << "\n";
  }

  return 0;
}
