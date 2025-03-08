// #include <iostream>
// #include <boost/asio.hpp>
// #include <string>
// using namespace boost::asio;
// using ip::tcp;
// using std::string;
// using std::cout;
// using std::cin;
// using std::endl;
// 
// int main() {
//      boost::asio::io_service io_service;
// //socket creation
//      tcp::socket socket(io_service);
// //connection
//      socket.connect( tcp::endpoint( boost::asio::ip::address::from_string("127.0.0.1"),8888));
// // request/message from client
// //  
//   while (true){
//      string msg ;
//      getline(cin,msg);
//      msg+='\n';
//      boost::system::error_code error;
//      boost::asio::write( socket, boost::asio::buffer(msg), error );
// //     if( !error ) {
// //        cout << "Client sent hello message!" << endl;
// //     }
//      if(error) {
//         cout << "send failed: " << error.message() << endl;
//      }
//  // getting response from server
//     boost::asio::streambuf receive_buffer;
//     boost::asio::read(socket, receive_buffer, boost::asio::transfer_all(), error);
//     if( error && error != boost::asio::error::eof ) {
//         cout << "receive failed: " << error.message() << endl;
//     }
//     else {
//         const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());
//         cout << data << endl;
//     }}
//     return 0;
// }
//
//

// #include <iostream>
// #include <boost/asio.hpp>
// 
// using namespace boost::asio;
// using namespace boost::asio::ip;
// 
// int main() {
//     io_context io;
//     tcp::socket socket(io);
//     socket.connect(tcp::endpoint(address::from_string("127.0.0.1"), 8888));
// 
//     std::cout << "Connected to server. Type messages:\n";
// 
//     while (true) {
//          std::string data_recieved;
//         boost::system::error_code error;
//         size_t length = socket.read_some(buffer(data_recieved), error);
//         std::cout <<data_recieved<< std::endl;
//         if (error) break;
//         if(data_recieved !=""){
//         std::cout << "Received: " << std::string(data_recieved, length) << std::endl;}
//     
//         else{
//           std::cout<< "sent:" ;
//         std::string message;
//         std::getline(std::cin, message);
//         
//         //if (message.empty()) continue;
// 
//         // Send message to server
//         write(socket, buffer(message + "\n"));
//         }}
//         // Receive response from server
//         
//     return 0;
// }
// 
//
#include <iostream>
#include <boost/asio.hpp>
#include <thread>

using namespace boost::asio;
using namespace boost::asio::ip;

void receive_messages(tcp::socket &socket) {
    char data[1024];
    boost::system::error_code error;
    
    while (true) {
        size_t length = socket.read_some(buffer(data), error);
        if (error) break;

        std::cout << "\nReceived: " << std::string(data, length) << "\n> ";
        std::cout.flush();
    }
}

int main() {
    io_context io;
    tcp::socket socket(io);
    socket.connect(tcp::endpoint(address::from_string("127.0.0.1"), 8888));

    std::cout << "Connected to server. Waiting for messages...\n";

    // Start a separate thread for receiving messages
    std::thread receive_thread(receive_messages, std::ref(socket));

    while (true) {
        std::string message;
        std::cout << "> ";
        std::getline(std::cin, message);
        
        if (message.empty()) continue;

        // Send message to server
        write(socket, buffer(message + "\n"));
    }

    receive_thread.join();  // Join thread before exiting
    return 0;
}

