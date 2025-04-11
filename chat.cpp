#include <QApplication>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdlib>
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

QFrame *createMessageBubble(const QString &text, bool isUserMessage) {
  // Create a frame to hold the message label
  QFrame *bubbleFrame = new QFrame();
  bubbleFrame->setFrameShape(QFrame::NoFrame);
  QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
  shadow->setBlurRadius(5.0); // how soft the shadow is
  bubbleFrame->setGraphicsEffect(shadow);
  // Basic bubble style: rounded corners, border, padding
  // Different background colors for user vs. other
  if (isUserMessage) {
    bubbleFrame->setStyleSheet("QFrame { "
                               "  border-radius: 10px; "
                               "  background-color: #D9FDD3; "
                               "}");
    shadow->setOffset(-1.0, 1.0);
  } else {
    bubbleFrame->setStyleSheet("QFrame { "
                               "  border-radius: 10px; "
                               "  background-color: #FFFFFF; "
                               "}");
    shadow->setOffset(1.0, 1.0);
  }

  // Create label for message text
  QLabel *msgLabel = new QLabel(text);
  msgLabel->setStyleSheet("QLabel {border:none;}");
  msgLabel->setWordWrap(true);

  // Put label in a layout so we can control internal padding
  QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleFrame);
  bubbleLayout->setContentsMargins(10, 10, 10, 10); // Padding inside the bubble
  bubbleLayout->addWidget(msgLabel);

  return bubbleFrame;
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
    QLabel *headerLabel = new QLabel("Alice");
    headerLabel->setStyleSheet("font-weight: bold; font-size: 14pt;");
    headerLayout->addWidget(headerLabel);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    for (int i = 0; i < 100; i++) {
      QHBoxLayout *msgLayout = new QHBoxLayout();
      bool isUser = rand() % 2;

      // Create bubble
      QFrame *bubble =
          createMessageBubble(QString("Message %1").arg(i), isUser);

      if (isUser) {
        msgLayout->addStretch();
        msgLayout->addWidget(bubble);
      } else {
        msgLayout->addWidget(bubble);
        msgLayout->addStretch();
      }
      scrollLayout->addLayout(msgLayout);
    }
    scrollLayout->addStretch();

    scrollWidget->setLayout(scrollLayout);
    scrollArea->setWidget(scrollWidget);

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

    QScrollBar *msgScrollBar = scrollArea->verticalScrollBar();
    msgScrollBar->setStyleSheet(
        "QScrollBar:vertical {"
        "background: transparent;"
        "width: 5px;"
        "margin: 0;"
        "}"
        ""
        "QScrollBar::handle:vertical {"
        "background: #C3C0BB;"
        "border-radius: 2.4px;"
        "}"
        ""
        "QScrollBar::add-line:vertical {"
        "height: 0px;"
        "}"
        ""
        "QScrollBar::sub-line:vertical {"
        "height: 0px;"
        "}"
        ""
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "height: 0px;"
        "}");
    msgScrollBar->setValue(msgScrollBar->maximum());

    rightLayout->addWidget(scrollArea, 1);
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
