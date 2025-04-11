#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <string>
#include <vector>

QFrame *createLine(Qt::Orientation orientation) {
  QFrame *line = new QFrame;
  if (orientation == Qt::Horizontal)
    line->setFrameShape(QFrame::HLine);
  else
    line->setFrameShape(QFrame::VLine);

  line->setFrameShadow(QFrame::Sunken);
  line->setLineWidth(1);
  return line;
}

std::vector<std::string> get_contacts() {
  std::vector<std::string> a;
  a.push_back("Alice");
  a.push_back("Bob");
  a.push_back("Nea");
  a.push_back("Arthur");
  a.push_back("Curve");
  return a;
}

class IconButton : public QPushButton {
public:
  explicit IconButton(const QString &text, QWidget *parent = nullptr)
      : QPushButton("", parent) {

    QIcon attachIcon(text); // from Qt resource or file path
    setIcon(attachIcon);
    setIconSize(QSize(20, 20)); // adjust icon size as you like
    setStyleSheet("QPushButton {"
                  "border: none;"
                  "padding: 5px;"
                  "background-color: transparent;"
                  "border-radius: 4px;"
                  "}"
                  "QPushButton:hover {"
                  "background-color: #e0e0e0;"
                  "}");
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QPushButton::resizeEvent(event);
    int side = height();
    setFixedWidth(side);
  }
};

class TextEdit : public QLineEdit {
public:
  TextEdit() : QLineEdit() {
    setMinimumHeight(30);
    QFont f = font();
    f.setPointSize(11);
    setFont(f);
  }
};

class ChatUI : public QWidget {
public:
  ChatUI() { setupUI(); }

private:
  void setupUI() {

    QVBoxLayout *leftLayout = new QVBoxLayout;

    QHBoxLayout *newChatLayout = new QHBoxLayout();
    TextEdit *newChatEdit = new TextEdit();
    newChatEdit->setPlaceholderText("Start a new chat...");
    QPushButton *newChatBtn = new IconButton("add.svg");
    newChatLayout->addWidget(newChatEdit);
    newChatLayout->addWidget(newChatBtn);
    leftLayout->addLayout(newChatLayout);

    QVBoxLayout *chatButtonsLayout = new QVBoxLayout();
    auto contacts = get_contacts();
    for (auto cnt : contacts) {
      QPushButton *contactBtn = new QPushButton(cnt.c_str());
      contactBtn->setStyleSheet("QPushButton {"
                                "border: none;"
                                "text-align: left;"
                                "padding: 8px;"
                                "background-color: transparent;"
                                "font-size: 16px;"
                                "border-radius: 4px"
                                "}"
                                "QPushButton:hover {"
                                "background-color: #e0e0e0;"
                                "}");
      contactBtn->setCursor(Qt::PointingHandCursor);
      // contactBtn->setFixedSize(400, 40);
      chatButtonsLayout->addWidget(contactBtn);
    }

    leftLayout->addLayout(chatButtonsLayout);
    leftLayout->addStretch();
    QVBoxLayout *rightLayout = new QVBoxLayout;

    QHBoxLayout *headerLayout = new QHBoxLayout;
    QLabel *headerLabel = new QLabel("Chat with Alice");
    headerLayout->addWidget(headerLabel);

    QHBoxLayout *messageLayout = new QHBoxLayout;
    QLabel *messageLabel = new QLabel("Message area");
    messageLayout->addWidget(messageLabel);

    QHBoxLayout *inputLayout = new QHBoxLayout;
    TextEdit *typemsg = new TextEdit();
    typemsg->setPlaceholderText("Type a Message...");
    // typemsg->setStyleSheet(
    //     "background-color: white;  border: 1px solid black;");
    IconButton *attachBtn = new IconButton("attach.svg");
    IconButton *sendBtn = new IconButton("send.svg");

    inputLayout->addWidget(typemsg);
    inputLayout->addWidget(attachBtn);
    inputLayout->addWidget(sendBtn);

    rightLayout->addLayout(headerLayout, 0);
    rightLayout->addWidget(createLine(Qt::Horizontal));
    rightLayout->addLayout(messageLayout, 15);
    rightLayout->addWidget(createLine(Qt::Horizontal));
    rightLayout->addLayout(inputLayout);

    QHBoxLayout *mainLayout = new QHBoxLayout;

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addWidget(createLine(Qt::Vertical));
    mainLayout->addLayout(rightLayout, 3);

    setLayout(mainLayout);
    setFixedSize(1200, 900);
    setStyleSheet("background-color: lightyellow;");
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  ChatUI window;
  window.show();

  return app.exec();
}
