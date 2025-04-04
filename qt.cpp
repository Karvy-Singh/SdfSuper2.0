#include <QApplication>
#include <QWidget>
#include <QLineEdit>
#include <QVBoxLayout>

int main(int argc, char *argv[]) {

    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Login/Register");

    QLineEdit *username = new QLineEdit(&window);
    username->setPlaceholderText("Enter Username...");
    username->setGeometry(280,275,300,40);
    username->setFixedSize(300,40);
    username->setStyleSheet("background-color: white;  border: 2px solid black;");


    QLineEdit *password = new QLineEdit(&window);
    password->setPlaceholderText("Enter Password...");
    password->setGeometry(280,325,300,40);
    password->setFixedSize(300,40);
    password->setStyleSheet("background-color: white;  border: 2px solid black;");
   
    window.setFixedSize(860,640);
    window.show();

    window.setStyleSheet("background-color: lightyellow;");

    return app.exec();
}



