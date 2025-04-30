#include <QApplication>
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
#include <QString>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QThread>
#include <QTimer>
#include <sqlite3.h>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using boost::asio::ip::tcp;
using json = nlohmann::json;

static sqlite3 *openDb()
{
    static sqlite3 *db = nullptr;
    if (db)
        return db;

    if (sqlite3_open("chat.db", &db) != SQLITE_OK)
        qFatal("cannot open sqlite db: %s", sqlite3_errmsg(db));

    const char *create =
        "CREATE TABLE IF NOT EXISTS mess ("
        " id INTEGER PRIMARY KEY,"
        " account   TEXT," 
        " sen_name  TEXT,"
        " rec_name  TEXT,"
        " mess      TEXT,"
        " timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    char *err = nullptr;
    if (sqlite3_exec(db, create, nullptr, nullptr, &err) != SQLITE_OK)
        qFatal("sqlite: %s", err);

    sqlite3_exec(db,
                 "ALTER TABLE mess ADD COLUMN account TEXT;", nullptr, nullptr, nullptr);

    return db;
}

struct DbRow
{
    QString txt;
    bool mine;
};

static void dbInsert(const QString &account,
                     const QString &sen,
                     const QString &rec,
                     const QString &msg)
{
    sqlite3 *db = openDb();
    const char *sql =
        "INSERT INTO mess(account,sen_name,rec_name,mess) VALUES(?,?,?,?);";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK)
        return;
    sqlite3_bind_text(st, 1, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, sen.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, rec.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, msg.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

static QList<DbRow> dbLoadChat(const QString &account,
                               const QString &partner)
{
    QList<DbRow> out;
    sqlite3 *db = openDb();
    const char *sql =
        "SELECT sen_name,mess FROM mess "
        "WHERE account=? AND "
        "  ((sen_name=? AND rec_name=?) OR (sen_name=? AND rec_name=?)) "
        "ORDER BY timestamp;";
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
        out << DbRow{text, sender == account};
    }
    sqlite3_finalize(st);
    return out;
}

static QList<QString> dbRecentPartners(const QString &account)
{
    QList<QString> out;
    sqlite3 *db = openDb();
    const char *sql =
        "SELECT partner FROM ("
        "  SELECT rec_name AS partner, timestamp FROM mess "
        "   WHERE account=? "
        "  UNION ALL "
        "  SELECT sen_name AS partner, timestamp FROM mess "
        "   WHERE account=? ) "
        "GROUP BY partner ORDER BY MAX(timestamp) DESC;";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK)
        return out;

    sqlite3_bind_text(st, 1, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, account.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(st) == SQLITE_ROW)
        out << QString::fromUtf8(reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
    sqlite3_finalize(st);
    return out;
}

class ClientConnection : public QObject
{
    Q_OBJECT
public:
    explicit ClientConnection(QObject *parent = nullptr)
        : QObject(parent),
          io_(),
          socket_(io_),
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

signals:
    void connected();
    void loginOK();
    void loginFailed(const QString &reason);
    void incomingText(const QString &from, const QString &message);
    void serverNotice(const QString &text);
    void fatalError(const QString &what);

private:
    void stop()
    {
        post([&]
             {
                 boost::system::error_code ec;
                 socket_.shutdown(tcp::socket::shutdown_both, ec);
                 socket_.close(ec);
                 work_.reset(); 
             });
    }
    template <typename F>
    void post(F &&fn) { boost::asio::post(io_, std::forward<F>(fn)); }

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
                length_ = (static_cast<uint8_t>(read_header_[5]) << 24) | (static_cast<uint8_t>(read_header_[6]) << 16) | (static_cast<uint8_t>(read_header_[7]) << 8) | (static_cast<uint8_t>(read_header_[8]));
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
            QString jstr = QString::fromStdString(value.substr(pos + 1));
            json j = json::parse(jstr.toStdString());
            if (j["type"] == "text")
            {
                emit incomingText(from, QString::fromStdString(j["content"]));
            }
        }
        break;
        default:
            break;
        }
    }

    void sendPacket(uint8_t type, const std::string &value)
    {
        std::vector<uint8_t> pkt;
        pkt.insert(pkt.end(), {'J', 'I', 'I', 'T'});
        pkt.push_back(type);
        uint32_t len = value.size();
        pkt.push_back((len >> 24) & 0xff);
        pkt.push_back((len >> 16) & 0xff);
        pkt.push_back((len >> 8) & 0xff);
        pkt.push_back(len & 0xff);
        pkt.insert(pkt.end(), value.begin(), value.end());

        post([data = std::move(pkt), this]
             { boost::asio::async_write(
                   socket_, boost::asio::buffer(data),
                   [this](auto ec, auto)
                   {
                       if (ec)
                           emit fatalError("Write failed");
                   }); });
    }

    boost::asio::io_context io_;
    tcp::socket socket_;
    boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>
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

        
        connect(conn_, &ClientConnection::loginOK, this, &LoginWindow::loginGood);
        connect(conn_, &ClientConnection::serverNotice, this, &LoginWindow::loginBad);
        connect(conn_, &ClientConnection::fatalError, this, &LoginWindow::loginBad);
    }

signals:
    void loginSuccess();

private slots:
    void doLogin()
    {
        conn_->login(user_->text(), pass_->text());
    }
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
    ChatWindow(ClientConnection *conn, const QString &me, QWidget *parent = nullptr)
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
        msgEdit_ = new QLineEdit;
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
        connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::sendMsg);

        connect(conn_, &ClientConnection::incomingText, this, &ChatWindow::gotMsg);
        connect(conn_, &ClientConnection::serverNotice, this, &ChatWindow::info);
    }

private slots:
    void addChat()
    {
        QString p = newChatEdit_->text().trimmed();
        if (p.isEmpty() || chatItems_.contains(p))
            return;
        chatItems_[p]; 
        chatBtns_[p] = makeChatButton(p);
        newChatEdit_->clear();
        select(p);
    }
    void openChat() 
    {
        select(static_cast<QPushButton *>(sender())->text());
    }
    void sendMsg()
    {
        if (cur_.isEmpty())
            return;
        QString txt = msgEdit_->text().trimmed();
        if (txt.isEmpty())
            return;
        appendBubble(txt, true);
        chatItems_[cur_] << Msg{txt, true};
        dbInsert(me_, me_, cur_, txt);
        msgEdit_->clear();
        conn_->sendText(cur_, txt);
    }
    void gotMsg(const QString &from, const QString &txt)
    {
        if (!chatItems_.contains(from))
        {
            chatItems_[from];
            chatBtns_[from] = makeChatButton(from);
        }
        chatItems_[from] << Msg{txt, false};
        dbInsert(me_, from, me_, txt);
        if (from == cur_)
            appendBubble(txt, false);
    }

private:
    QHBoxLayout *wrap(QWidget *w)
    {
        auto l = new QHBoxLayout;
        l->addWidget(w);
        return l;
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

        chatItems_[cur_].clear();
        for (const DbRow &r : dbLoadChat(me_, cur_))
            chatItems_[cur_] << Msg{r.txt, r.mine};

        for (const Msg &m : chatItems_[cur_])
            appendBubble(m.text, m.mine);

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
    void info(const QString &m) { std::cout << "[Server] " << m.toStdString() << '\n'; }

    ClientConnection *conn_;
    QString me_;
    QLabel *header_;
    QLineEdit *newChatEdit_;
    QLineEdit *msgEdit_;
    QScrollArea *scrollArea_;
    QVBoxLayout *scrollLay_;
    QVBoxLayout *chatBtnsLay_;
    QHash<QString, QList<Msg>> chatItems_;
    QHash<QString, QPushButton *> chatBtns_;
    QString cur_;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto conn = std::make_unique<ClientConnection>();
    QObject::connect(conn.get(), &ClientConnection::fatalError,
                     [](const QString &e)
                     { std::cerr << e.toStdString() << '\n'; });

    LoginWindow login(conn.get());
    login.show();

    QObject::connect(&login, &LoginWindow::loginSuccess, [&]
                     {
         QString me = login.findChild<QLineEdit *>()->text();
         ChatWindow *chat = new ChatWindow(conn.get(), me);
         chat->show(); });

    conn->connectTo("127.0.0.1", 8080);

    return app.exec();
}
#include "wow.moc"
