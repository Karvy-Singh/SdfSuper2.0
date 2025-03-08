// #include <iostream>
// #include <boost/asio.hpp>
// 
// using namespace boost::asio;
// using ip::tcp;
// using std::string;
// using std::cout;
// using std::endl;
// 
// string read_(tcp::socket & socket) {
//        boost::asio::streambuf buf;
//        boost::asio::read_until( socket, buf, "\n" );
//        string data = boost::asio::buffer_cast<const char*>(buf.data());
//        return data;
// }
// void send_(tcp::socket & socket, const string& message) {
//        const string msg = message ;
//        boost::asio::write( socket, boost::asio::buffer(message) );
// }
// 
// int main() {
//       boost::asio::io_service io_service;
// //listen for new connection
//       tcp::acceptor acceptor_(io_service, tcp::endpoint(tcp::v4(), 8888));
// //socket creation 
//       tcp::socket client1(io_service);
//       acceptor_.accept(client1);
// 
//       tcp::socket client2(io_service);
//       acceptor_.accept(client2);
// 
// 
// //waiting for connection
//      //  acceptor_.accept(client1);
//      //  acceptor_.accept(client2);
// while(true){
// //read operation
//       string message = read_(client1);
//       cout << message<<endl;
//       send_(client2, "yoo hello");
// 
// 
//       //cout << message << endl;
// //write operation
// //  if (message!=""){
// //        send_(client2, message);
// //  }
// //  else{
// //    message=read_(client2);
// //    send_(client1, message);
// //  }
// }
// 
//       //cout << "Servent sent Hello message to Client!" << endl;
//    return 0;
// }
#include <iostream>
#include <boost/asio.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

void handle_client(tcp::socket &sender, tcp::socket &receiver, std::string client_name) {
    char data[1024];
    boost::system::error_code error;
    
    // Read message from sender
    size_t length = sender.read_some(buffer(data), error);
    
    if (!error) {
        std::cout << client_name << " sent: " << std::string(data, length) << std::endl;

        // Forward message to receiver
        write(receiver, buffer(data, length), error);
    }
}

int main() {
    io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8888));

    std::cout << "Server started on port 8888\n";

    // Accept Client 1
    tcp::socket client1(io);
    acceptor.accept(client1);
    std::cout << "Client 1 connected\n";

    // Accept Client 2
    tcp::socket client2(io);
    acceptor.accept(client2);
    std::cout << "Client 2 connected\n";

    while (true) {
        handle_client(client1, client2, "Karvy");
        handle_client(client2, client1, "Harsh");
    }

    return 0;
}

