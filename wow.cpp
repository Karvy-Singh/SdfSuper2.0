#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QString>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <QDebug>
#include <QSet>

#include <boost/asio.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;
static std::mutex dbMutex;

static const char *baseButtonQSS =
    "QPushButton {"
    "border: none;"
    "text-align: left;"
    "padding: 8px;"
    "background-color: transparent;"
    "font-size: 16px;"
    "border-radius: 4px;"
    "}"
    "QPushButton:hover { background: #e0e0e0; }";

static sqlite3 *openDb()
{
  static sqlite3 *db = nullptr;
  if (db)
    return db;

  if (sqlite3_open("chat.db", &db) != SQLITE_OK)
    qFatal("cannot open sqlite db: %s", sqlite3_errmsg(db));

  sqlite3_busy_timeout(db, 5000);
  sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

  const char *create = R"SQL(
      CREATE TABLE IF NOT EXISTS mess (
        id        INTEGER PRIMARY KEY,
        account   TEXT,
        sen_name  TEXT,
        rec_name  TEXT,
        mess      TEXT,
        type      TEXT,
        filename  TEXT,
        filedata  BLOB,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
      );
    )SQL";
  char *err = nullptr;
  if (sqlite3_exec(db, create, nullptr, nullptr, &err) != SQLITE_OK)
    qFatal("sqlite: %s", err);

  sqlite3_exec(db,
               "ALTER TABLE mess ADD COLUMN type     TEXT    DEFAULT 'text';",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "ALTER TABLE mess ADD COLUMN filename TEXT;",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "ALTER TABLE mess ADD COLUMN filedata BLOB;",
               nullptr, nullptr, nullptr);

  return db;
}

struct DbRow
{
  QString type;     
  QString txt;      
  QString filename; 
  QByteArray blob;  
  bool mine;
};

static void dbInsert(const QString &account,
                     const QString &sen,
                     const QString &rec,
                     const QString &msg,
                     const QString &type = QStringLiteral("text"),
                     const QString &filename = QString(),
                     const QByteArray &fileData = QByteArray())
{
  sqlite3 *db = openDb();
  qDebug() << "[dbInsert] called with:"
           << " account=" << account
           << " sen=" << sen
           << " rec=" << rec
           << " msg=" << msg
           << " type=" << type
           << " filename=" << filename
           << " fileData.size=" << fileData.size();

  std::lock_guard<std::mutex> guard(dbMutex);

  static const char *sql =
      "INSERT INTO mess"
      "(account, sen_name, rec_name, mess, type, filename, filedata) "
      "VALUES(?,?,?,?,?,?,?);";

  sqlite3_stmt *st = nullptr;
  int rc = sqlite3_prepare_v2(db, sql, -1, &st, nullptr);
  if (rc != SQLITE_OK)
  {
    qDebug() << "[dbInsert] prepare error:" << sqlite3_errmsg(db);
    return;
  }

  sqlite3_bind_text(st, 1, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, sen.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, rec.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 4, msg.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 5, type.toUtf8().constData(), -1, SQLITE_TRANSIENT);

  if (!filename.isEmpty())
  {
    sqlite3_bind_text(st, 6, filename.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  }
  else
  {
    sqlite3_bind_null(st, 6);
  }

  if (!fileData.isEmpty())
  {
    sqlite3_bind_blob(st, 7, fileData.constData(),
                      static_cast<int>(fileData.size()), SQLITE_TRANSIENT);
  }
  else
  {
    sqlite3_bind_null(st, 7);
  }

  rc = sqlite3_step(st);
  if (rc != SQLITE_DONE)
  {
    qDebug() << "[dbInsert] step error:" << sqlite3_errmsg(db)
             << "(rc=" << rc << ")";
  }
  else
  {
    qDebug() << "[dbInsert] success – row inserted";
  }

  sqlite3_finalize(st);
}

static QList<DbRow> dbLoadChat(const QString &account,
                               const QString &partner)
{
  QList<DbRow> out;
  sqlite3 *db = openDb();
  const char *sql =
      "SELECT sen_name, mess, type, filename, filedata "
      "FROM mess "
      "WHERE account=? AND "
      "  ((sen_name=? AND rec_name=?) OR (sen_name=? AND rec_name=?)) "
      "ORDER BY id ASC;";

  sqlite3_stmt *st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK)
    return out;

  sqlite3_bind_text(st, 1, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 2, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, partner.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 4, partner.toUtf8().constData(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 5, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);

  while (sqlite3_step(st) == SQLITE_ROW)
  {
    QString sender = QString::fromUtf8(
        reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
    QString text = QString::fromUtf8(
        reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
    QString type = QString::fromUtf8(
        reinterpret_cast<const char *>(sqlite3_column_text(st, 2)));

    const char *fn = reinterpret_cast<const char *>(
        sqlite3_column_text(st, 3));
    QByteArray blob;
    const void *data = sqlite3_column_blob(st, 4);
    int sz = sqlite3_column_bytes(st, 4);
    if (data && sz > 0)
      blob = QByteArray(reinterpret_cast<const char *>(data), sz);

    out << DbRow{type,
                 text,
                 fn ? QString::fromUtf8(fn) : QString(),
                 blob,
                 (sender == account)};
  }
  sqlite3_finalize(st);
  return out;
}

static QList<QString> dbRecentPartners(const QString &account)
{
  QList<QString> out;
  sqlite3 *db = openDb();
  const char *sql = "SELECT rec_name FROM mess "
                    "WHERE sen_name = ? "
                    "GROUP BY rec_name "
                    "ORDER BY MAX(timestamp) DESC;";

  sqlite3_stmt *st = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK)
    return out;

  sqlite3_bind_text(st, 1, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);

  while (sqlite3_step(st) == SQLITE_ROW)
    out << QString::fromUtf8(
        reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));

  sqlite3_finalize(st);
  return out;
}

class ClientConnection : public QObject
{
  Q_OBJECT
public:
  explicit ClientConnection(QObject *parent = nullptr)
      : QObject(parent), io_(), socket_(io_),
        work_(boost::asio::make_work_guard(io_))
  {
    thread_ = std::thread([this]
                          { io_.run(); });
  }
  ~ClientConnection() override
  {
    stop();
    if (thread_.joinable())
      thread_.join();
  }

  void connectTo(const std::string &host, uint16_t port)
  {
    post([=]
         {
      try {
        tcp::endpoint ep(boost::asio::ip::make_address(host), port);
        socket_.connect(ep);
        doReadHeader();
        emit connected();
      } catch (std::exception &e) {
        emit fatalError(QString::fromStdString(e.what()));
      } });
  }
  void login(const QString &user, const QString &pass)
  {
    username_ = user.toStdString();
    json j = {{"username", username_}, {"password", pass.toStdString()}};
    sendPacket(0x01, j.dump());
  }
  void sendText(const QString &receiver, const QString &text)
  {
    json j = {{"type", "text"},
              {"receiver", receiver.toStdString()},
              {"content", text.toStdString()}};
    sendPacket(0x02, j.dump());
  }

  void sendFile(const QString &receiver, const QString &path)
  {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
      return;

    QByteArray bytes = f.readAll();
    QByteArray b64 = bytes.toBase64(QByteArray::Base64UrlEncoding);
    QFileInfo info(f);

    json j = {{"type", "file"},
              {"receiver", receiver.toStdString()},
              {"filename", info.fileName().toStdString()},
              {"data", QString::fromUtf8(b64).toStdString()}};

    sendPacket(0x02, j.dump());
  }

signals:
  void connected();
  void loginOK();
  void loginFailed(const QString &reason);
  void incomingText(const QString &from, const QString &message);
  void incomingStatus(const QString &user, const QString &status);
  void serverNotice(const QString &text);
  void fatalError(const QString &what);
  void incomingFile(const QString &from, const QString &filename,
                    const QByteArray &content);

private:
  void stop()
  {
    post([&]
         {
      boost::system::error_code ec;
      socket_.shutdown(tcp::socket::shutdown_both, ec);
      socket_.close(ec);
      work_.reset(); });
  }
  template <typename F>
  void post(F &&fn)
  {
    boost::asio::post(io_, std::forward<F>(fn));
  }

  void doReadHeader()
  {
    boost::asio::async_read(
        socket_, boost::asio::buffer(read_header_),
        [this](auto ec, std::size_t bytes)
        {
          if (ec || bytes != read_header_.size())
          {
            emit fatalError("Read error");
            return;
          }
          if (!validateMagic())
          {
            emit fatalError("Bad magic");
            return;
          }
          type_ = static_cast<uint8_t>(read_header_[4]);
          length_ = (static_cast<uint8_t>(read_header_[5]) << 24) |
                    (static_cast<uint8_t>(read_header_[6]) << 16) |
                    (static_cast<uint8_t>(read_header_[7]) << 8) |
                    (static_cast<uint8_t>(read_header_[8]));
          if (length_ == 0)
          {
            handlePacket(type_, "");
            doReadHeader();
          }
          else
          {
            body_.resize(length_);
            doReadBody();
          }
        });
  }
  void doReadBody()
  {
    boost::asio::async_read(
        socket_, boost::asio::buffer(body_),
        [this](auto ec, std::size_t bytes)
        {
          if (ec || bytes != body_.size())
          {
            emit fatalError("Read body failed");
            return;
          }
          handlePacket(type_, std::string(body_.begin(), body_.end()));
          doReadHeader();
        });
  }
  bool validateMagic()
  {
    return read_header_[0] == 'J' && read_header_[1] == 'I' &&
           read_header_[2] == 'I' && read_header_[3] == 'T';
  }

  void handlePacket(uint8_t type, const std::string &value)
  {
    switch (type)
    {
    case 0xff:
      if (value == "Logged in successfully")
        emit loginOK();
      else
        emit serverNotice(QString::fromStdString(value));
      break;
    case 0x02:
    {
      auto pos = value.find(':');
      if (pos == std::string::npos)
        return;

      QString from = QString::fromStdString(value.substr(0, pos));
      json j = json::parse(value.substr(pos + 1));

      if (j["type"] == "text")
      {
        emit incomingText(from, QString::fromStdString(j["content"]));
      }
      else if (j["type"] == "file")
      {
        QString fname = QString::fromStdString(j["filename"]);
        QByteArray raw =
            QByteArray::fromBase64(QString::fromStdString(j["data"]).toUtf8(),
                                   QByteArray::Base64UrlEncoding);

        emit incomingFile(from, fname, raw);
      }
    }
    break;

    case 0x03:
    { 
      json j = json::parse(value);
      QString user = QString::fromStdString(j["user"]);
      QString status = QString::fromStdString(j["status"]);
      emit incomingStatus(user, status);
    }
    break;

    default:
      break;
    }
  }

  void sendPacket(uint8_t type, const std::string &value)
  {
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'J', 'I', 'I', 'T'});
    buf.push_back(type);
    uint32_t len = value.size();
    buf.push_back((len >> 24) & 0xff);
    buf.push_back((len >> 16) & 0xff);
    buf.push_back((len >> 8) & 0xff);
    buf.push_back(len & 0xff);
    buf.insert(buf.end(), value.begin(), value.end());

    auto pktPtr = std::make_shared<std::vector<uint8_t>>(std::move(buf));

    post([this, pktPtr] 
         { boost::asio::async_write(
               socket_, boost::asio::buffer(pktPtr->data(), pktPtr->size()),
               [this, pktPtr](auto ec, auto) 
               {
                 if (ec)
                   emit fatalError("Write failed");
               }); });
  }

  boost::asio::io_context io_;
  tcp::socket socket_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work_;

  std::thread thread_;

  std::string username_;
  std::array<char, 9> read_header_{};
  uint32_t length_{};
  uint8_t type_{};
  std::vector<uint8_t> body_;
};
struct Msg
{
  QString text;
  bool mine;
};
class LoginWindow : public QWidget
{
  Q_OBJECT
public:
  explicit LoginWindow(ClientConnection *conn, QWidget *parent = nullptr)
      : QWidget(parent), conn_(conn)
  {
    setWindowTitle("Login/Register");
    setFixedSize(860, 640);
    setStyleSheet("background-color: lightyellow;");

    user_ = new QLineEdit(this);
    user_->setPlaceholderText("Enter Username...");
    user_->setGeometry(280, 275, 300, 40);
    user_->setFixedSize(300, 40);
    user_->setStyleSheet("background-color: white;  border: 2px solid black;");
    pass_ = new QLineEdit(this);

    pass_->setPlaceholderText("Enter Password...");
    pass_->setGeometry(280, 325, 300, 40);
    pass_->setFixedSize(300, 40);
    pass_->setStyleSheet("background-color: white;  border: 2px solid black;");
    pass_->setEchoMode(QLineEdit::Password);
    QPushButton *btn = new QPushButton("Login", this);
    btn->setGeometry(280, 375, 300, 40);
    btn->setFixedSize(300, 40);

    connect(btn, &QPushButton::clicked, this, &LoginWindow::doLogin);
    connect(user_, &QLineEdit::returnPressed, this, &LoginWindow::doLogin);
    connect(pass_, &QLineEdit::returnPressed, this, &LoginWindow::doLogin);

    connect(conn_, &ClientConnection::loginOK, this, &LoginWindow::loginGood);
    connect(conn_, &ClientConnection::serverNotice, this,
            &LoginWindow::loginBad);
    connect(conn_, &ClientConnection::fatalError, this, &LoginWindow::loginBad);
  }

signals:
  void loginSuccess();

private slots:
  void doLogin() { conn_->login(user_->text(), pass_->text()); }
  void loginGood()
  {
    emit loginSuccess();
    close();
  }
  void loginBad(const QString &why)
  {
    pass_->clear();
    pass_->setPlaceholderText("Login failed: " + why);
  }

private:
  ClientConnection *conn_;
  QLineEdit *user_;
  QLineEdit *pass_;
};

QFrame *createLine(Qt::Orientation o)
{
  QFrame *f = new QFrame;
  f->setFrameShape(o == Qt::Horizontal ? QFrame::HLine : QFrame::VLine);
  f->setFrameShadow(QFrame::Sunken);
  f->setLineWidth(1);
  return f;
}
QFrame *createMessageBubble(const QString &txt, bool self)
{
  QFrame *b = new QFrame;
  b->setFrameShape(QFrame::NoFrame);
  QGraphicsDropShadowEffect *sh = new QGraphicsDropShadowEffect;
  sh->setBlurRadius(5);
  b->setGraphicsEffect(sh);
  if (self)
  {
    b->setStyleSheet("QFrame {border-radius:10px;background:#D9FDD3;}");
    sh->setOffset(-1, 1);
  }
  else
  {
    b->setStyleSheet("QFrame {border-radius:10px;background:#FFFFFF;}");
    sh->setOffset(1, 1);
  }
  QLabel *lbl = new QLabel(txt);
  lbl->setWordWrap(true);
  QVBoxLayout *bl = new QVBoxLayout(b);
  bl->setContentsMargins(10, 10, 10, 10);
  bl->addWidget(lbl);
  return b;
}
class IconButton : public QPushButton
{
public:
  explicit IconButton(const QString &iconPath, QWidget *parent = nullptr)
      : QPushButton(QString(), parent)
  {
    setIcon(QIcon(iconPath));
    setIconSize(QSize(20, 20));
    setStyleSheet("QPushButton {"
                  "border:none;"
                  "padding:5px;"
                  "background:transparent;"
                  "border-radius:4px;}"
                  "QPushButton:hover {background:#e0e0e0;}");
  }

protected:
  void resizeEvent(QResizeEvent *e) override
  {
    QPushButton::resizeEvent(e);
    setFixedWidth(height());
  }
};
class ChatWindow : public QWidget
{
  Q_OBJECT
public:
  ChatWindow(ClientConnection *conn, const QString &me,
             QWidget *parent = nullptr)
      : QWidget(parent), conn_(conn), me_(me)
  {
    auto left = new QVBoxLayout;
    auto userLbl = new QLabel(me_);
    userLbl->setStyleSheet("font-weight: bold; font-size: 14pt;");
    left->addWidget(userLbl);
    left->addWidget(createLine(Qt::Horizontal));
    auto newChatLay = new QHBoxLayout;
    newChatEdit_ = new QLineEdit;
    newChatEdit_->setPlaceholderText("Start a new chat...");

    auto addBtn = new IconButton("add.svg");

    newChatLay->addWidget(newChatEdit_);
    newChatLay->addWidget(addBtn);
    left->addLayout(newChatLay);

    chatBtnsLay_ = new QVBoxLayout;
    left->addLayout(chatBtnsLay_);
    for (const QString &p : dbRecentPartners(me_))
    {
      if (!chatBtns_.contains(p))
        chatBtns_[p] = makeChatButton(p);
    }

    if (!chatBtns_.isEmpty())
      QTimer::singleShot(0, [this]
                         { select(chatBtns_.keys().first()); });

    left->addStretch();

    auto right = new QVBoxLayout;
    header_ = new QLabel;
    header_->setStyleSheet("font-weight: bold; font-size: 14pt;");
    right->addLayout(wrap(header_));
    right->addWidget(createLine(Qt::Horizontal));

    scrollArea_ = new QScrollArea;
    scrollArea_->setWidgetResizable(true);
    QWidget *scrollW = new QWidget;
    scrollLay_ = new QVBoxLayout(scrollW);
    scrollLay_->addStretch();
    scrollArea_->setWidget(scrollW);
    right->addWidget(scrollArea_, 1);

    right->addWidget(createLine(Qt::Horizontal));

    auto inpLay = new QHBoxLayout;

    msgEdit_ = new QTextEdit;
    msgEdit_->setFixedHeight(msgEdit_->fontMetrics().height() + 10);
    msgEdit_->setPlaceholderText("Type a message…");

    auto attachBtn = new IconButton("attach.svg");
    auto sendBtn = new IconButton("send.svg");
    inpLay->addWidget(msgEdit_);
    inpLay->addWidget(attachBtn);
    inpLay->addWidget(sendBtn);
    right->addLayout(inpLay);

    auto main = new QHBoxLayout(this);
    main->addLayout(left, 1);
    main->addWidget(createLine(Qt::Vertical));
    main->addLayout(right, 3);
    setFixedSize(1200, 900);
    setStyleSheet("background: lightyellow;");

    connect(addBtn, &QPushButton::clicked, this, &ChatWindow::addChat);
    connect(newChatEdit_, &QLineEdit::returnPressed, this,
            &ChatWindow::addChat);

    connect(attachBtn, &QPushButton::clicked, this, &ChatWindow::attachFile);
    connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::sendMsg);
    msgEdit_->installEventFilter(this);
    connect(msgEdit_, &QTextEdit::textChanged, this, [=]()
            {
      int lineHeight = msgEdit_->fontMetrics().lineSpacing();
      int docHeight = msgEdit_->document()->size().height();

      int newHeight =
          std::ceil(docHeight * lineHeight / msgEdit_->fontMetrics().height()) +
          10;

      int maxHeight = 80;
      msgEdit_->setFixedHeight(qMin(newHeight, maxHeight)); });

    connect(conn_, &ClientConnection::incomingText, this, &ChatWindow::gotMsg);

    connect(conn_, &ClientConnection::incomingStatus, this, &ChatWindow::updateStatus);

    connect(conn_, &ClientConnection::incomingFile, this, &ChatWindow::gotFile);
    connect(conn_, &ClientConnection::serverNotice, this, &ChatWindow::info);
  }

private slots:
  void addChat()
  {
    const QString p = newChatEdit_->text().trimmed();
    if (p.isEmpty())
      return;

    ensureChatEntry(p);
    newChatEdit_->clear();
    select(p);
  }
  void openChat()
  {
    QString text = static_cast<QPushButton *>(sender())->text();
    if (text.startsWith('['))
    {
      int idx = text.indexOf(']');
      if (idx != -1 && text.size() > idx + 2)
        text = text.mid(idx + 2);
    }
    select(text);
  }

  bool isImageFile(const QString &filename)
  {
    QStringList exts = {"png", "jpg", "jpeg", "bmp", "gif", "webp"};
    QString ext = QFileInfo(filename).suffix().toLower();
    return exts.contains(ext);
  }

  void sendMsg()
  {
    if (cur_.isEmpty())
      return;
    QString txt = msgEdit_->toPlainText().trimmed();
    if (txt.isEmpty())
      return;
    appendBubble(txt, true);
    chatItems_[cur_] << Msg{txt, true};
    dbInsert(me_, me_, cur_, txt, "text");
    msgEdit_->clear();
    conn_->sendText(cur_, txt);
  }

  void attachFile()
  {
    if (cur_.isEmpty())
      return;

    QString path = QFileDialog::getOpenFileName(this, "Select a file to send");
    if (path.isEmpty())
      return;

    conn_->sendFile(cur_, path);

    QFileInfo fi(path);
    QByteArray content;

    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
    {
      content = f.readAll();
    }

    if (isImageFile(path))
    {
      dbInsert(me_, me_, cur_, "[img]" + path, "image", fi.fileName(), content);
      appendImageBubble(fi.fileName(), content, true);
    }
    else
    {
      appendFileBubble(fi.fileName(), fi.size(), true, QByteArray());
      dbInsert(me_, me_, cur_, "[FILE]" + path, "file", fi.fileName(), content);
    }

  }

  void gotMsg(const QString &from, const QString &txt)
  {
    const bool online = onlineUsers_.contains(from);
    if (!online) {
      updateStatus(from, "online");
    }
    ensureChatEntry(from); 
    chatItems_[from] << Msg{txt, false};
    dbInsert(me_, from, me_, txt, "text");

    if (from == cur_)
      appendBubble(txt, false);
  }

  void gotFile(const QString &from, const QString &filename,
               const QByteArray &data)
  {
    if (!chatItems_.contains(from))
    {
      chatItems_[from];
      chatBtns_[from] = makeChatButton(from);
    }
    chatItems_[from] << Msg{filename, false};


    if (isImageFile(filename))
    {
      dbInsert(me_, from, me_, "[img]" + filename, "image", filename, data);
      appendImageBubble(filename, data, false);
    }
    else
    {
      dbInsert(me_, from, me_, "[file]" + filename, "file", filename, data);
      appendFileBubble(filename, data.size(), false, data);
    }
  }

  void appendFileBubble(const QString &fname, qint64 size, bool mine,
                        const QByteArray &payload)
  {
    QLabel *link = new QLabel(QStringLiteral("<a href=\"#\">%1</a> (%2 kB)")
                                  .arg(fname)
                                  .arg(size / 1024.0, 0, 'f', 1));
    link->setTextFormat(Qt::RichText);
    link->setTextInteractionFlags(Qt::TextBrowserInteraction);
    link->setOpenExternalLinks(false);

    if (!payload.isEmpty()) 
    {
      connect(link, &QLabel::linkActivated, this,
              [payload, fname](const QString &)
              {
                QString dst = QFileDialog::getSaveFileName(
                    nullptr, "Save file as", fname);
                if (dst.isEmpty())
                  return;
                QFile out(dst);
                if (out.open(QIODevice::WriteOnly))
                  out.write(payload);
              });
    }

    auto h = new QHBoxLayout;
    if (mine)
      h->addStretch();
    QFrame *bubble = createMessageBubble(QString(), mine);
    bubble->layout()->addWidget(link);
    h->addWidget(bubble);
    if (!mine)
      h->addStretch();
    scrollLay_->insertLayout(scrollLay_->count() - 1, h);
  }

  void appendImageBubble(const QString &filename,
                         const QByteArray &data,
                         bool mine)
  {
    QLabel *imageLabel = new QLabel;
    QPixmap pix;
    pix.loadFromData(data);
    imageLabel->setPixmap(pix.scaledToWidth(200, Qt::SmoothTransformation)); 
    imageLabel->setScaledContents(true);

    auto bubble = createMessageBubble(QString(), mine);
    bubble->layout()->addWidget(imageLabel);

    auto h = new QHBoxLayout;
    if (mine)
      h->addStretch();
    h->addWidget(bubble);
    if (!mine)
      h->addStretch();
    scrollLay_->insertLayout(scrollLay_->count() - 1, h);
  }

  bool eventFilter(QObject *obj, QEvent *event)
  {
    if (obj == msgEdit_ && event->type() == QEvent::KeyPress)
    {
      QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
      if (keyEvent->key() == Qt::Key_Return &&
          !(keyEvent->modifiers() & Qt::ShiftModifier))
      {
        sendMsg();
        return true;
      }
    }
    return QWidget::eventFilter(obj, event);
  }

public slots:
  void updateStatus(const QString &user,
                    const QString &status)
  {

    if (user == me_) 
      return;
   

    const bool online = (status == "online");
    if (online)
      onlineUsers_.insert(user);
    else
      onlineUsers_.remove(user);


    applyPresenceLabel(user, online);
  }

private:
  QHBoxLayout *wrap(QWidget *w)
  {
    auto l = new QHBoxLayout;
    l->addWidget(w);
    return l;
  }
  void applyPresenceLabel(const QString &user, bool online)
  {
    QPushButton *btn = chatBtns_.value(user, nullptr);
    if (!btn)
      return;

    btn->setText(QString("[%1] %2")
                     .arg(online ? "online" : "offline", user));

    btn->setStyleSheet(baseButtonQSS +
                       QStringLiteral(" QPushButton { color:%1; }")
                           .arg(online ? "black" : "gray"));
  }

  QPushButton *makeChatButton(const QString &name)
  {
    auto b = new QPushButton(name);
    b->setStyleSheet("QPushButton {"
                     "border: none;"
                     "text-align: left;"
                     "padding: 8px;"
                     "background-color: transparent;"
                     "font-size: 16px;"
                     "border-radius: 4px}"
                     "QPushButton:hover {background-color: #e0e0e0;}");
    b->setCursor(Qt::PointingHandCursor);
    connect(b, &QPushButton::clicked, this, &ChatWindow::openChat);
    chatBtnsLay_->addWidget(b);
    return b;
  }

  void ensureChatEntry(const QString &user)
  {
    if (!chatItems_.contains(user))
      chatItems_[user]; 

    if (!chatBtns_.contains(user))
      chatBtns_[user] = makeChatButton(user); 

    const bool online = onlineUsers_.contains(user);
    applyPresenceLabel(user, online);
  }

  void select(const QString &who)
  {
    if (cur_ == who)
      return;
    cur_ = who;
    header_->setText(who);
    rebuild();
  }

  void rebuild()
  {
    clearLayout(scrollLay_);

    for (const DbRow &r : dbLoadChat(me_, cur_))
    {
      auto h = new QHBoxLayout;
      if (r.mine)
        h->addStretch();
      if (r.type == "text")
      {
        appendBubble(r.txt, r.mine);
      }
      else if (r.type == "file")
      {
        appendFileBubble(
            r.filename,
            /*size=*/r.blob.size(),
            /*mine=*/r.mine,
            /*payload=*/r.blob);
      }
      else if (r.type == "image")
      {
        appendImageBubble(
            r.filename,
            /*data=*/r.blob,
            /*mine=*/r.mine);
      }
      if (!r.mine)
        h->addStretch();
      scrollLay_->addLayout(h);
    }
    scrollLay_->addStretch();
    QTimer::singleShot(0, [sb = scrollArea_->verticalScrollBar()]
                       { sb->setValue(sb->maximum()); });
  }

  void appendBubble(const QString &txt, bool mine)
  {
    auto h = new QHBoxLayout;
    if (mine)
      h->addStretch();
    h->addWidget(createMessageBubble(txt, mine));
    if (!mine)
      h->addStretch();

    scrollLay_->insertLayout(scrollLay_->count() - 1, h);

    QTimer::singleShot(0, [sb = scrollArea_->verticalScrollBar()]
                       { sb->setValue(sb->maximum()); });
  }

  void clearLayout(QLayout *lay)
  {
    while (auto it = lay->takeAt(0))
    {
      if (it->layout())
        clearLayout(it->layout());
      if (it->widget())
        it->widget()->deleteLater();
      delete it;
    }
  }
  void info(const QString &m)
  {
    std::cout << "[Server] " << m.toStdString() << '\n';
  }

  ClientConnection *conn_;
  QString me_;
  QLabel *header_;
  QLineEdit *newChatEdit_;
  QTextEdit *msgEdit_;
  QScrollArea *scrollArea_;
  QVBoxLayout *scrollLay_;
  QVBoxLayout *chatBtnsLay_;
  QHash<QString, QList<Msg>> chatItems_;
  QHash<QString, QPushButton *> chatBtns_;
  QString cur_;
  QSet<QString> onlineUsers_;
};

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  auto conn = std::make_unique<ClientConnection>();

  QObject::connect(conn.get(), &ClientConnection::fatalError,
                   [](const QString &e)
                   { std::cerr << e.toStdString() << '\n'; });

  using Presence = QPair<QString, QString>; 
  QVector<Presence> presBuffer;             
  ChatWindow *chat = nullptr;               

  QObject::connect(conn.get(), &ClientConnection::incomingStatus,
                   [&](const QString &user, const QString &status)
                   {
                     if (chat)
                     {
                       QMetaObject::invokeMethod(chat, "updateStatus", Qt::QueuedConnection,
                                                 Q_ARG(QString, user), Q_ARG(QString, status));
                     }
                     else
                     {
                       presBuffer.append({user, status});
                     }
                   });

  LoginWindow login(conn.get());
  login.show();

  QObject::connect(&login, &LoginWindow::loginSuccess, [&]()
                   {
        QString me = login.findChild<QLineEdit*>()->text();
        chat = new ChatWindow(conn.get(), me);
        chat->show();

        for (const Presence& p : std::as_const(presBuffer))
            chat->updateStatus(p.first, p.second);
        presBuffer.clear(); });

  conn->connectTo("129.154.252.177", 6969);

  return app.exec();
}

#include "wow.moc"
