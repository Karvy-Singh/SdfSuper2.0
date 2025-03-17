#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;  

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

void writefile(std::string filename, std::string data);
std::string readfile(std::string &filename);

 class readjson{
    std::string recieved_msg;
    public:
      readjson(std::string r): recieved_msg(r){};
      friend void writefile(std::string filename, std::string data);
      void read_write_display(){
        auto pos= recieved_msg.find(':');
        std::cout<< recieved_msg.substr(0,pos)<<": ";
        json json_is= json::parse(recieved_msg.substr(pos+1));
        std::string type= json_is["type"];
        if(type=="text"){
          std::cout<< json_is["content"] << std::endl;
        }
        else if(type=="file"){
          writefile(json_is["filename"],json_is["content"]);
          std::cout<< "file saved !" << std::endl;
        }
      }
  };

class makejson{
    std::string receiver;
    std::string type;
    std::string msg;
    public:
      makejson(std::string r, std::string t, std::string m): receiver(r), type(t), msg(m){}
      std::string returnjson(){
        if(type=="text"){
          json data= {
            {"type", type},
            {"receiver", receiver},
            {"content",msg}
              };
          std::string json_str= data.dump();
          return json_str;   
        }
        else if (type=="file"){
          json data= {
            {"type", type},
            {"receiver", receiver},
            {"filename",msg},
            {"content", readfile(msg)}
              };
          std::string json_str= data.dump();
          return json_str;
        }
        else{
          return ""; 
        }
      }
      friend std::string readfile(std::string &filename);
  };

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string encoded;
    int i = 0, j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    size_t in_len = data.size();
    size_t index = 0;

    while (in_len--) {
        char_array_3[i++] = data[index++];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                encoded += base64_chars[char_array_4[i]];
            
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            encoded += base64_chars[char_array_4[j]];

        while (i++ < 3)
            encoded += '=';
    }

    return encoded;
}

std::string parse_outgoing(std::string value) {
  auto pos = value.find(' ');
  auto pos2 = value.find(' ', pos + 1);

  if (pos != std::string::npos && pos2 != std::string::npos) { 
      std::string receiver = value.substr(0, pos);
      std::string type = value.substr(pos + 1, pos2 - pos - 1);
      std::string msg = value.substr(pos2 + 1);

      makejson jsonobj(receiver, type, msg);
      std::string finaljson = jsonobj.returnjson();

      if (finaljson.empty()) {
          std::cerr << "Error: JSON output is empty for input: " << value << std::endl;
      } 

      return finaljson;
  } else {
      std::cerr << "Error: Invalid input format. Expected '<receiver> <type> <message>' but got: " << value << std::endl;
      return "";
  }
}

std::string readfile(std::string &filename){
  std::ifstream inFile(filename, std::ios::binary);
  if (!inFile) {
    std::cerr << "Error: Cannot open file for reading.\n";
  }
  auto file_contents= std::vector<uint8_t>(
  std::istreambuf_iterator<char>(inFile),   
  std::istreambuf_iterator<char>()        
  );
  return base64_encode(file_contents);
}
  
std::vector<uint8_t> base64_decode(const std::string& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<uint8_t> decoded_data;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_++];
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++)
                decoded_data.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; j < i - 1; j++)
            decoded_data.push_back(char_array_3[j]);
    }

    return decoded_data;
}

void parse_incoming(std::string value){
  readjson rjsonobj(value);
  rjsonobj.read_write_display();
}

void writefile(std::string filename, std::string data){
 std::vector<uint8_t> decoded_content= base64_decode(data);
 std::string destinationFile= "r"+filename;
 std::ofstream outFile(destinationFile, std::ios::binary);
 if (!outFile) {
   std::cerr << "Error: Cannot open file for writing.\n";
  }
 outFile.write(reinterpret_cast<const char*>(decoded_content.data()), decoded_content.size());
};

class ChatClient {
private:
  boost::asio::io_context io_;
  tcp::socket socket_;
  std::string host_;
  unsigned short int port_;
  bool running_{true};
  std::string username;
  std::string password;
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
    std::getline(std::cin, username);

    std::cout << "Enter your password: ";
    std::getline(std::cin, password);

    json user_json={
      {"username",username},
      {"password",password}
    };
    
    std::string user_= user_json.dump();

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

      sendPacket(0x02, parse_outgoing(line));
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
    parse_incoming(value);
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
