#include <QApplication>
#include <QWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

QFrame* createLine(Qt::Orientation orientation) {
    QFrame* line = new QFrame;
    if (orientation == Qt::Horizontal)
        line->setFrameShape(QFrame::HLine);
    else
        line->setFrameShape(QFrame::VLine);

    line->setFrameShadow(QFrame::Sunken);
    line->setLineWidth(1);
    return line;
}

class ChatUI : public QWidget {
public:
    ChatUI() {
        setupUI();
    }

private:
    void setupUI() {

        QVBoxLayout* leftLayout = new QVBoxLayout;

        QPushButton* contactBtn = new QPushButton("Alice");
        contactBtn->setStyleSheet(
            "QPushButton {"
            "border: none;"
            "text-align: left;"
            "padding: 8px;"
            "background-color: transparent;"
            "font-size: 16px;"
            "}"
            "QPushButton:hover {"
            "background-color: #e0e0e0;"
            "}"
        );
        contactBtn->setCursor(Qt::PointingHandCursor);
        contactBtn->setFixedSize(400, 40);
        leftLayout->addWidget(contactBtn);

        QVBoxLayout* rightLayout = new QVBoxLayout;

        QHBoxLayout* headerLayout = new QHBoxLayout;
        QLabel* headerLabel = new QLabel("Chat with Alice");
        headerLayout->addWidget(headerLabel);

        QHBoxLayout* messageLayout = new QHBoxLayout;
        QLabel* messageLabel = new QLabel("Message area");
        messageLayout->addWidget(messageLabel);

        QHBoxLayout* inputLayout = new QHBoxLayout;
        QLineEdit *typemsg = new QLineEdit();
        typemsg->setPlaceholderText("Type a Message...");   
        typemsg->setStyleSheet("background-color: white;  border: 1px solid black;");
        inputLayout->addWidget(typemsg);



        rightLayout->addLayout(headerLayout, 1);
        rightLayout->addWidget(createLine(Qt::Horizontal));
        rightLayout->addLayout(messageLayout, 15);
        rightLayout->addWidget(createLine(Qt::Horizontal));
        rightLayout->addLayout(inputLayout, 1);

        QHBoxLayout* mainLayout = new QHBoxLayout;
        mainLayout->addLayout(leftLayout, 1);
        mainLayout->addWidget(createLine(Qt::Vertical));
        mainLayout->addLayout(rightLayout, 2);

        setLayout(mainLayout);
        setFixedSize(1200, 900);
        setStyleSheet("background-color: lightyellow;");
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    ChatUI window;
    window.show();

    return app.exec();
}

