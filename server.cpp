#include <boost/asio.hpp>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <stdbool.h>

using boost::asio::ip::tcp;
using json = nlohmann::json;

class ChatServer;

class putinsqldb {
  std::string username;
  std::string password;

public:
  putinsqldb(std::string u, std::string p) : username(u), password(p) {}
  void create_and_insert();
};

static int sqlcallback(void *data, int argc, char **argv, char **azColName) {
  if (argc > 0 && argv[0]) {
    int count = std::stoi(argv[0]);
    bool *exists = static_cast<bool *>(data);
    *exists = (count > 0);
  }
  return 0;
}

bool userExists(sqlite3 *db, const std::string &username,
                const std::string &password) {
  std::string sql = "SELECT COUNT(*) FROM USER WHERE USERNAME = '" + username +
                    "' AND PASSWORD = '" + password + "';";

  bool exists = false;
  char *errMsg = nullptr;

  if (sqlite3_exec(db, sql.c_str(), sqlcallback, &exists, &errMsg) !=
      SQLITE_OK) {
    std::cerr << "Error: " << errMsg << std::endl;
    sqlite3_free(errMsg);
    return false;
  }
  return exists;
}

static bool userNameExists(sqlite3* db, const std::string& username) {
    const char* sql = "SELECT COUNT(*) FROM USER WHERE USERNAME = ?;";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        exists = (sqlite3_column_int(st, 0) > 0);
    }
    sqlite3_finalize(st);
    return exists;
}

void putinsqldb::create_and_insert() {
  sqlite3 *DB;
  char *messageError;
  int exit = sqlite3_open("user_data.db", &DB);
  if (exit != SQLITE_OK) {
    std::cerr << "Error opening database" << std::endl;
  }
  std::string createtable = "CREATE TABLE IF NOT EXISTS USER("
                            "USERNAME TEXT NOT NULL,"
                            "PASSWORD TEXT NOT NULL,"
                            "PRIMARY KEY USERNAME));";

  int tableStatus =
      sqlite3_exec(DB, createtable.c_str(), NULL, 0, &messageError);

  if (tableStatus != SQLITE_OK) {
    std::cerr << "Error creating table: " << messageError << std::endl;
    sqlite3_free(messageError);
  }

  if (!userExists(DB, username, password)) {
    std::string insert =
        std::format("INSERT INTO USER(USERNAME,PASSWORD) VALUES('{}','{}');",
                    username, password);

    int entryStatus = sqlite3_exec(DB, insert.c_str(), NULL, 0, &messageError);

    if (entryStatus != SQLITE_OK) {
      std::cerr << "Error inserting into table: " << messageError << std::endl;
      sqlite3_free(messageError);
    }
  }
}

class Connection : public std::enable_shared_from_this<Connection> {
private:
  tcp::socket socket_;
  ChatServer &server_;
  std::string username_;
  bool active_{false};
  std::array<char, 9> read_header_buf_;

  void doReadHeader();
  void doReadBody(std::size_t length, uint8_t type);
  bool validateMagic(const std::array<char, 4> &magic);
  void handleMessage(uint8_t type, const std::string &value);

public:
  Connection(boost::asio::io_context &io, ChatServer &server)
      : socket_(io), server_(server) {}

  tcp::socket &socket() { return socket_; }
  void start();
  void close();

  void setUsername(const std::string &name) { username_ = name; }
  std::string getUsername() { return username_; }

  void send(const std::vector<uint8_t> &data);
};

class ChatServer {
private:
  boost::asio::io_context &io_;
  tcp::acceptor acceptor_;
  std::map<std::string, std::shared_ptr<Connection>> connections_;
  void doAccept();

public:
  ChatServer(boost::asio::io_context &io, unsigned short port)
      : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)) {}
  void start() { doAccept(); };
  void onDisconnect(std::shared_ptr<Connection> conn);
  void handleLogin(std::shared_ptr<Connection> conn,
                   const std::string &username);
  void handleChatMessage(std::shared_ptr<Connection> sender,
                         const std::string &receiver,
                         const std::string &message);
  void sendPacket(std::shared_ptr<Connection> conn, uint8_t type,
                  const std::string &value);
};

// ChatServer Thingy

void ChatServer::onDisconnect(std::shared_ptr<Connection> conn) {
  auto username = conn->getUsername();
  if (!username.empty()) {
    auto it = connections_.find(username);
    if (it != connections_.end() && it->second == conn) {
      connections_.erase(it);
    }
  }
  conn->close();
}

void ChatServer::handleLogin(std::shared_ptr<Connection> conn,
                             const std::string &username) {
  auto it = connections_.find(username);
  if (it != connections_.end()) {
    it->second->close();
    connections_.erase(it);
  }
  conn->setUsername(username);
  connections_[username] = conn;
  // TODO: setup server messages (done)
  sendPacket(conn, 0xff, "Logged in successfully");
}

void ChatServer::handleChatMessage(std::shared_ptr<Connection> sender,
                                   const std::string &receiver,
                                   const std::string &message) {
  auto it = connections_.find(receiver);
  if (it != connections_.end()) {
    auto receiver_conn = it->second;
    std::string final_msg = sender->getUsername() + ": " + message;
    sendPacket(receiver_conn, 0x02, final_msg);
  } else {
    sendPacket(sender, 0xff, "Receiver " + receiver + " does not exist");
  }
  // TODO: setup server messages (send not found) (done)
}

void ChatServer::sendPacket(std::shared_ptr<Connection> conn, uint8_t type,
                            const std::string &value) {
  std::vector<uint8_t> packet;
  // Magic (yeah)
  packet.push_back('J');
  packet.push_back('I');
  packet.push_back('I');
  packet.push_back('T');
  // Type (idk)
  packet.push_back(type);
  // Length (big-endian) (big-baby)
  uint32_t len = static_cast<uint32_t>(value.size());
  packet.push_back((len >> 24) & 0xFF);
  packet.push_back((len >> 16) & 0xFF);
  packet.push_back((len >> 8) & 0xFF);
  packet.push_back(len & 0xFF);
  // Value
  for (char c : value) {
    packet.push_back(static_cast<uint8_t>(c));
  }
  conn->send(packet);
}

void ChatServer::doAccept() {
  auto new_conn = std::make_shared<Connection>(io_, *this);
  acceptor_.async_accept(new_conn->socket(),
                         [this, new_conn](const boost::system::error_code &ec) {
                           if (!ec) {
                             new_conn->start();
                           }
                           doAccept();
                         });
}


int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <port>\n";
    return 1;
  }

  unsigned short port = static_cast<unsigned short>(std::stoi(argv[1]));

  try {
    boost::asio::io_context io_context;
    ChatServer server(io_context, port);
    server.start();

    std::cout << "[Server] Listening on port " << port << "...\n";
    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "[Server] Exception: " << e.what() << "\n";
  }

  return 0;
}

void Connection::start() {
  active_ = true;
  doReadHeader();
}

void Connection::close() {
  if (!active_)
    return;
  active_ = false;
  boost::system::error_code ec;
  socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
  socket_.close(ec);
}

void Connection::send(const std::vector<uint8_t> &data) {
  if (!active_)
    return;

  auto pkt = std::make_shared<std::vector<uint8_t>>(std::move(data));
  auto self = shared_from_this();

  boost::asio::async_write(
      socket_, boost::asio::buffer(*pkt),
      [this, self, pkt](const boost::system::error_code &ec, std::size_t) {
        if (ec)
          server_.onDisconnect(self);
      });
}

void Connection::doReadHeader() {
  auto self = shared_from_this();
  boost::asio::async_read(
      socket_, boost::asio::buffer(read_header_buf_),
      [this, self](const boost::system::error_code &ec,
                   std::size_t bytes_read) {
        if (ec || bytes_read != read_header_buf_.size()) {
          server_.onDisconnect(self);
          return;
        }

        std::array<char, 4> magic;
        std::memcpy(magic.data(), read_header_buf_.data(), 4);
        if (!validateMagic(magic)) {
          server_.onDisconnect(self);
          return;
        }

        uint8_t type = static_cast<uint8_t>(read_header_buf_[4]);

        uint32_t length = 0;
        length |= (static_cast<unsigned char>(read_header_buf_[5]) << 24);
        length |= (static_cast<unsigned char>(read_header_buf_[6]) << 16);
        length |= (static_cast<unsigned char>(read_header_buf_[7]) << 8);
        length |= (static_cast<unsigned char>(read_header_buf_[8]));

        if (length == 0) {
          handleMessage(type, "");
          doReadHeader();
        } else {
          doReadBody(length, type);
        }
      });
}

void Connection::doReadBody(std::size_t length, uint8_t type) {
  auto self = shared_from_this();
  auto body_buf = std::make_shared<std::vector<char>>(length);

  boost::asio::async_read(
      socket_, boost::asio::buffer(*body_buf),
      [this, self, body_buf, type](const boost::system::error_code &ec,
                                   std::size_t bytes_read) {
        if (ec || bytes_read != body_buf->size()) {
          // error or partial read => disconnect
          server_.onDisconnect(self);
          return;
        }

        std::string value(body_buf->data(), body_buf->size());
        handleMessage(type, value);
        doReadHeader();
      });
}

bool Connection::validateMagic(const std::array<char, 4> &magic) {
  return (magic[0] == 'J' && magic[1] == 'I' && magic[2] == 'I' &&
          magic[3] == 'T');
}

void Connection::handleMessage(uint8_t type, const std::string &value) {
  json jsonobj = json::parse(value);
  switch (type) {
  case 0x01: {  
      sqlite3* udb = nullptr;
      if (sqlite3_open("user_data.db", &udb) != SQLITE_OK) {
          server_.sendPacket(shared_from_this(), 0xff,"Server error");
          break;
      }

      auto uname = jsonobj["username"].get<std::string>();
      auto pwd   = jsonobj["password"].get<std::string>();

      if (!userNameExists(udb, uname)) {
          putinsqldb reg(uname, pwd);
          reg.create_and_insert();
          server_.handleLogin(shared_from_this(), uname);
      } else if (userExists(udb, uname, pwd)) {
          server_.handleLogin(shared_from_this(), uname);
      } else {
          server_.sendPacket(shared_from_this(), 0xff,"Login failed: invalid password");
      }
      sqlite3_close(udb);
  } break;


  case 0x02: 
  {
    std::string receiver = jsonobj["receiver"];
    server_.handleChatMessage(shared_from_this(), receiver, value);
  } break;

  default: {
    server_.onDisconnect(shared_from_this());
  } break;
  }
}
