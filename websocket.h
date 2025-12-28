#ifndef WEBSOCKET_H
#define WEBSOCKET_H


#include <QMainWindow>
#include <QAudioInput>
#include <QAudioFormat>
#include <QIODevice>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
// 创建一个简单的测试WebSocket类
class SimpleWebSocketTester : public QObject
{
    Q_OBJECT

public:
    SimpleWebSocketTester(const QString &url) {
        m_webSocket = new QWebSocket();

        // 最小化配置
        QSslConfiguration sslConfig = m_webSocket->sslConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        m_webSocket->setSslConfiguration(sslConfig);

        connect(m_webSocket, SIGNAL(connected()), this, SLOT(onConnected()));
        connect(m_webSocket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
        connect(m_webSocket, SIGNAL(error(QAbstractSocket::SocketError)),
                this, SLOT(onError(QAbstractSocket::SocketError)));

        qDebug() << "简单测试: 连接到" << url;
        m_webSocket->open(QUrl(url));
    }

    ~SimpleWebSocketTester() {
        if (m_webSocket) {
            m_webSocket->close();
            delete m_webSocket;
        }
    }

private slots:
    void onConnected() {
        qDebug() << "简单测试: 连接成功!";
        // 立即发送一个简单的ping消息
        m_webSocket->sendTextMessage("ping");
    }

    void onDisconnected() {
        qDebug() << "简单测试: 断开连接";
    }

    void onError(QAbstractSocket::SocketError error) {
        qDebug() << "简单测试: 错误" << error << m_webSocket->errorString();
    }

private:
    QWebSocket *m_webSocket;
};

#endif // WEBSOCKET_H
