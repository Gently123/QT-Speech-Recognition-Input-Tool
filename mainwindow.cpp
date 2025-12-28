#include "mainwindow.h"


#include <QDebug>
#include <QMessageBox>
#include <QApplication>
#include <QAbstractSocket>
#include <QSslConfiguration>
#include <QSslError>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QAudioDeviceInfo>
#include <QJsonParseError>
#include <QJsonArray>
#include <QNetworkProxyFactory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDateTime>
#include <QUrl>
#include <QShortcut>
#include <QMouseEvent>
#include <QPixmap>
#include <QPainter>
// mainwindow.cpp 中修改构造函数
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_webSocket(nullptr),
    m_audioInput(nullptr),
    m_audioDevice(nullptr),
    m_isRecording(false),
    m_isFileMode(false),
    m_fileSampleRate(16000),
    m_totalSent(0),
    startHotkeyId(0),
    stopHotkeyId(0),
    trayIcon(nullptr),
    trayMenu(nullptr),
    showAction(nullptr),
    quitAction(nullptr),
    isTypingMode(false),  // 明确初始化
    isMiniMode(false),
        isDragging(false),
        miniModeLabel(nullptr),
        miniModeShortcut(nullptr),
        monitorAudioInput(nullptr),
        monitorDevice(nullptr),
        levelTimer(nullptr),  // 初始化定时器指针
        audioLevel(0.0),
        normalModeGeometry()
{
    // 设置窗口图标
        this->setWindowIcon(QIcon(":/logo.ico"));
    // 检查SSL支持
    qDebug() << "SSL支持:" << QSslSocket::supportsSsl();
    // 加载麦克风图片
        qDebug() << "开始加载麦克风图片...";
        for (int i = 0; i < 7; ++i) {
            QString imagePath = QString(":/mic%1.png").arg(i);
            QPixmap pixmap(imagePath);
            if (!pixmap.isNull()) {
                micPixmaps.append(pixmap);
                qDebug() << "成功加载图片: mic" << i << "，尺寸: " << pixmap.size();
            } else {
                qDebug() << "无法加载图片: " << imagePath;
                // 创建测试图片
                QPixmap testPixmap(200, 200);
                testPixmap.fill(QColor(50 + i*30, 50, 150, 200));
                micPixmaps.append(testPixmap);
            }
        }

        qDebug() << "总共加载了" << micPixmaps.size() << "张图片";

        // 确保加载了7张图片
        if (micPixmaps.size() < 7) {
            qDebug() << "警告：未能加载所有麦克风图片，只加载了" << micPixmaps.size() << "张";
        }
        // 创建音量监控定时器
            levelTimer = new QTimer(this);
            levelTimer->setInterval(100); // 每100毫秒更新一次

    setupUI();
    setupAudioFormat();
    updateUIState();

#ifdef Q_OS_WIN
    // 注册全局热键
    registerHotkeys();
#endif
    // 创建切换小窗口模式的快捷键 (Ctrl+Shift+M)
        miniModeShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_M), this);
        connect(miniModeShortcut, &QShortcut::activated, this, &MainWindow::toggleMiniMode);

        // 连接音量定时器
        connect(levelTimer, &QTimer::timeout, this, &MainWindow::updateMicrophoneLevel);
}

// mainwindow.cpp 中修改析构函数
MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    // 注销全局热键
    unregisterHotkeys();
#endif

    // 清理托盘图标
    if (trayIcon) {
        trayIcon->hide();
        delete trayIcon;
        trayIcon = nullptr;
    }
    // 停止麦克风监控
        stopMicrophoneMonitoring();
        // 清理定时器
            if (levelTimer) {
                levelTimer->stop();
                delete levelTimer;
                levelTimer = nullptr;
            }

            // 清理小窗口标签
            if (miniModeLabel) {
                delete miniModeLabel;
                miniModeLabel = nullptr;
            }

            // 清理快捷键
            if (miniModeShortcut) {
                delete miniModeShortcut;
                miniModeShortcut = nullptr;
            }

    if (m_webSocket) {
        m_webSocket->close();
        delete m_webSocket;
        m_webSocket = nullptr;
    }

    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }
}

void MainWindow::setupUI()
{
    // 设置窗口属性
        setWindowTitle("Qt ASR Client - 按Ctrl+Shift+M切换大/小窗口模式");
        setGeometry(100, 100, 800, 600);
    // 创建中央部件
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    mainLayout = new QVBoxLayout(centralWidget);

    // 连接部分布局
    connectionLayout = new QGridLayout();
    mainLayout->addLayout(connectionLayout);
    // 在连接部分布局之后添加
    autoTypeCheckBox = new QCheckBox("打字模式（输出到光标）", this);
    connectionLayout->addWidget(autoTypeCheckBox, 2, 0, 1, 2);
    connect(autoTypeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onTypingModeChanged(int)));

    // 初始化状态
    isTypingMode = false;

    // WebSocket地址
    labelWssIp = new QLabel("WebSocket地址:", this);
    connectionLayout->addWidget(labelWssIp, 0, 0);

    wssip = new QLineEdit(this);
    wssip->setText("wss:///ws/");
    connectionLayout->addWidget(wssip, 0, 1);

    btnConnect = new QPushButton("连接", this);
    connectionLayout->addWidget(btnConnect, 0, 2);
    connect(btnConnect, SIGNAL(clicked()), this, SLOT(on_btnConnect_clicked()));
    // 在连接按钮后面添加断开连接按钮
        btnDisconnect = new QPushButton("断开连接", this);
        connectionLayout->addWidget(btnDisconnect, 0, 3);
        connect(btnDisconnect, SIGNAL(clicked()), this, SLOT(on_btnDisconnect_clicked()));
        btnDisconnect->setEnabled(false);  // 初始状态禁用
    // 在连接按钮后面添加一个测试按钮
        btnTest = new QPushButton("测试连接", this);
        connectionLayout->addWidget(btnTest, 0, 4);
        connect(btnTest, SIGNAL(clicked()), this, SLOT(testConnection()));

    // 录音模式
    labelRecoderMode = new QLabel("录音模式:", this);
    connectionLayout->addWidget(labelRecoderMode, 1, 0);

    recoderMode = new QComboBox(this);
    recoderMode->addItem("麦克风");
    recoderMode->addItem("文件");
    connectionLayout->addWidget(recoderMode, 1, 1);
    connect(recoderMode, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_recoderMode_currentIndexChanged(int)));

    // 信息显示
        info_div = new QLabel("请点击连接 - 按Ctrl+Shift+M切换小窗口模式", this);
        connectionLayout->addWidget(info_div, 1, 2);

    // 麦克风模式组
    micModeWidget = new QGroupBox("麦克风模式", this);
    mainLayout->addWidget(micModeWidget);

    micModeLayout = new QGridLayout(micModeWidget);

    // ASR模式
    labelAsrMode = new QLabel("ASR模式:", this);
    micModeLayout->addWidget(labelAsrMode, 0, 0);

    asrMode = new QComboBox(this);
    asrMode->addItem("2pass");
    asrMode->addItem("online");
    asrMode->addItem("offline");
    micModeLayout->addWidget(asrMode, 0, 1);

    // ITN选项
    useITN = new QCheckBox("使用ITN", this);
    micModeLayout->addWidget(useITN, 0, 2);

    // 热词文本框
    varHot = new QPlainTextEdit(this);
    varHot->setPlaceholderText("热词（格式：词语 权重，每行一个）");
    varHot->setPlainText("阿里巴巴 20\nhello world 40");
    micModeLayout->addWidget(varHot, 1, 0, 1, 3);

    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    micModeLayout->addLayout(buttonLayout, 2, 0, 1, 3);

    // 开始录音按钮和快捷键说明
        QVBoxLayout *startLayout = new QVBoxLayout();
        btnStart = new QPushButton("开始录音 (Ctrl+Alt+L)", this);
        startLayout->addWidget(btnStart);
        QLabel *startHotkeyLabel = new QLabel("全局热键", this);
        startHotkeyLabel->setAlignment(Qt::AlignCenter);
        startHotkeyLabel->setStyleSheet("font-size: 10px; color: #666;");
        startLayout->addWidget(startHotkeyLabel);
        buttonLayout->addLayout(startLayout);

        connect(btnStart, SIGNAL(clicked()), this, SLOT(on_btnStart_clicked()));

        // 停止录音按钮和快捷键说明
        QVBoxLayout *stopLayout = new QVBoxLayout();
        btnStop = new QPushButton("停止录音 (Ctrl+Alt+D)", this);
        stopLayout->addWidget(btnStop);
        QLabel *stopHotkeyLabel = new QLabel("全局热键", this);
        stopHotkeyLabel->setAlignment(Qt::AlignCenter);
        stopHotkeyLabel->setStyleSheet("font-size: 10px; color: #666;");
        stopLayout->addWidget(stopHotkeyLabel);
        buttonLayout->addLayout(stopLayout);

        connect(btnStop, SIGNAL(clicked()), this, SLOT(on_btnStop_clicked()));

    btnClear = new QPushButton("清空", this);
    buttonLayout->addWidget(btnClear);
    connect(btnClear, SIGNAL(clicked()), this, SLOT(on_btnClear_clicked()));

    // 文件模式组
    fileModeWidget = new QGroupBox("文件模式", this);
    mainLayout->addWidget(fileModeWidget);
    fileModeWidget->setVisible(false);

    fileModeLayout = new QVBoxLayout(fileModeWidget);

    btnSelectFile = new QPushButton("选择音频文件", this);
    fileModeLayout->addWidget(btnSelectFile);
    connect(btnSelectFile, SIGNAL(clicked()), this, SLOT(on_btnSelectFile_clicked()));

    // 识别结果显示
    varArea = new QPlainTextEdit(this);
    varArea->setReadOnly(true);
    varArea->setPlaceholderText("识别结果将显示在这里");
    mainLayout->addWidget(varArea);
}

// 实现测试连接函数
void MainWindow::testConnection()
{
    QString url = wssip->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入WebSocket地址");
        return;
    }

    qDebug() << "=== 开始测试连接 ===";

    // 使用QNetworkAccessManager测试HTTP连接
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(onTestConnectionFinished(QNetworkReply*)));

    // 将wss://转换为https://进行测试
    QString httpUrl = url;
    httpUrl.replace("wss://", "https://");
    httpUrl.replace("ws://", "http://");

    QNetworkRequest request;
    request.setUrl(QUrl(httpUrl));
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    manager->get(request);

    info_div->setText("正在测试连接...");
}
// 在MainWindow中添加测试方法
void MainWindow::testSimpleWebSocket()
{
    QString url = wssip->text().trimmed();
    if (url.isEmpty()) return;

    qDebug() << "=== 开始简单WebSocket测试 ===";

    // 创建并立即测试，不保存引用
    SimpleWebSocketTester *tester = new SimpleWebSocketTester(url);

    // 5秒后自动清理
    QTimer::singleShot(5000, tester, SLOT(deleteLater()));
}
// 处理测试连接结果
void MainWindow::onTestConnectionFinished(QNetworkReply *reply)
{
    qDebug() << "测试连接结果:";
    qDebug() << "  状态码:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "  错误:" << reply->error();
    qDebug() << "  错误字符串:" << reply->errorString();
    qDebug() << "  响应头:" << reply->rawHeaderPairs();

    if (reply->error() == QNetworkReply::NoError) {
        info_div->setText("HTTP连接测试成功");
    } else {
        info_div->setText("HTTP连接测试失败: " + reply->errorString());
    }

    reply->deleteLater();
}

void MainWindow::setupAudioFormat()
{
    m_audioFormat.setSampleRate(16000);
    m_audioFormat.setChannelCount(1);
    m_audioFormat.setSampleSize(16);
    m_audioFormat.setCodec("audio/pcm");
    m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);
}

void MainWindow::updateUIState()
{
    bool isConnected = m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState;

    btnConnect->setEnabled(!isConnected);
    btnDisconnect->setEnabled(isConnected);  // 连接时才启用断开按钮
    wssip->setEnabled(!isConnected);
    btnStart->setEnabled(isConnected && !m_isRecording && !m_isFileMode);
    btnStop->setEnabled(isConnected && m_isRecording && !m_isFileMode);
    btnSelectFile->setEnabled(!isConnected && m_isFileMode);
    recoderMode->setEnabled(!isConnected);
    recoderMode->setEnabled(!isConnected);  // 断开连接后才能切换模式
    asrMode->setEnabled(!isConnected);     // 断开连接后才能切换ASR模式

}

// 修改 mainwindow.cpp 中的 on_btnConnect_clicked 函数
void MainWindow::on_btnConnect_clicked()
{
    QString url = wssip->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入WebSocket地址");
        return;
    }

    // 检查URL格式
    if (!url.startsWith("ws://") && !url.startsWith("wss://")) {
        QMessageBox::warning(this, "错误", "URL必须以ws://或wss://开头");
        return;
    }

    qDebug() << "正在连接到:" << url;
    qDebug() << "当前时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    // 清理旧的连接
    if (m_webSocket) {
        m_webSocket->close();
        delete m_webSocket;
        m_webSocket = nullptr;
    }

    // 使用带QNetworkRequest的open方法，可以设置更多请求头
    QNetworkRequest request;
    request.setUrl(QUrl(url));

    // 设置User-Agent，模拟浏览器
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    // 设置Origin头（重要！WebSocket握手通常需要）
    request.setRawHeader("Origin", "https://ai-input.com");

    // 设置其他可能的必要头
    request.setRawHeader("Sec-WebSocket-Version", "13");
    request.setRawHeader("Sec-WebSocket-Protocol", "chat, superchat");

    m_webSocket = new QWebSocket();

    // 配置SSL（忽略所有SSL错误，仅用于测试）
    QSslConfiguration sslConfig = m_webSocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    m_webSocket->setSslConfiguration(sslConfig);

    // 连接信号槽
    connect(m_webSocket, SIGNAL(connected()), this, SLOT(onWebSocketConnected()));
    connect(m_webSocket, SIGNAL(disconnected()), this, SLOT(onWebSocketDisconnected()));
    connect(m_webSocket, SIGNAL(textMessageReceived(QString)), this, SLOT(onWebSocketTextMessageReceived(QString)));
    connect(m_webSocket, SIGNAL(binaryMessageReceived(QByteArray)), this, SLOT(onWebSocketBinaryMessageReceived(QByteArray)));
    connect(m_webSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onWebSocketError(QAbstractSocket::SocketError)));

    // SSL错误信号
    connect(m_webSocket, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslErrors(QList<QSslError>)));

    // 添加关于状态变化的信号槽
    connect(m_webSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
            this, SLOT(onWebSocketStateChanged(QAbstractSocket::SocketState)));

    // 打开连接
    qDebug() << "开始连接...";
    m_webSocket->open(request);

    info_div->setText("正在连接ASR服务器，请等待...");
    updateUIState();
}
void MainWindow::on_btnDisconnect_clicked()
{
    qDebug() << "手动断开连接...";

    // 停止录音
    if (m_isRecording) {
        on_btnStop_clicked();
    }

    // 关闭WebSocket连接
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->close();
        // 注意：这里不要立即删除m_webSocket，因为onWebSocketDisconnected会处理
        info_div->setText("正在断开连接...");
    } else {
        // 如果WebSocket不存在或已断开，仍然更新UI
        info_div->setText("连接已断开");
        updateUIState();
    }
}
// 添加状态变化槽函数
void MainWindow::onWebSocketStateChanged(QAbstractSocket::SocketState state)
{
    QString stateStr;
    switch (state) {
    case QAbstractSocket::UnconnectedState:
        stateStr = "未连接";
        break;
    case QAbstractSocket::HostLookupState:
        stateStr = "正在查找主机";
        break;
    case QAbstractSocket::ConnectingState:
        stateStr = "正在连接";
        break;
    case QAbstractSocket::ConnectedState:
        stateStr = "已连接";
        break;
    case QAbstractSocket::BoundState:
        stateStr = "已绑定";
        break;
    case QAbstractSocket::ListeningState:
        stateStr = "正在监听";
        break;
    case QAbstractSocket::ClosingState:
        stateStr = "正在关闭";
        break;
    default:
        stateStr = "未知状态";
    }
    qDebug() << "WebSocket状态改变:" << stateStr;
}
void MainWindow::on_btnStart_clicked()
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        QMessageBox::warning(this, "警告", "请先连接到服务器");
        return;
    }

    // 发送初始请求（麦克风模式）
    sendInitialRequest();

    // 开始录音前发送is_speaking: true
    QJsonObject speakingObj;
    speakingObj["is_speaking"] = true;
    QJsonDocument doc(speakingObj);
    m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    qDebug() << "发送: is_speaking = true (开始录音)";

    // 开始音频采集
    QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (devices.isEmpty()) {
        QMessageBox::warning(this, "警告", "未找到可用的麦克风设备");
        return;
    }

    // 检查音频格式是否支持
    if (!devices.first().isFormatSupported(m_audioFormat)) {
        QMessageBox::warning(this, "警告", "不支持的音频格式");
        return;
    }

    if (!m_audioInput) {
        m_audioInput = new QAudioInput(m_audioFormat, this);
        m_audioDevice = m_audioInput->start();
        if (m_audioDevice) {
            connect(m_audioDevice, SIGNAL(readyRead()), this, SLOT(onAudioReadyRead()));
        }
    }

    m_isRecording = true;
    m_asrText.clear();
    m_offlineText.clear();
    varArea->clear();
    m_totalSent = 0;
    info_div->setText("录音中...");

    updateUIState();
}
void MainWindow::on_btnStop_clicked()
{
    qDebug() << "停止录音...";

    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
        m_audioDevice = nullptr;
    }

    m_isRecording = false;

    // 发送停止说话标记
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        // 发送 is_speaking: false
        QJsonObject speakingObj;
        speakingObj["is_speaking"] = false;
        QJsonDocument doc(speakingObj);
        m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        qDebug() << "发送: is_speaking = false";

        // 150ms 后发送 finalize 帧
        QTimer::singleShot(150, this, [this]() {
            if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
                QJsonObject finalizeObj;
                finalizeObj["finalize"] = true;
                QJsonDocument doc(finalizeObj);
                m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                qDebug() << "发送: finalize = true";
            }
        });
    }

    info_div->setText("录音停止，等待识别结果...");
    updateUIState();
}

void MainWindow::on_btnClear_clicked()
{
    varArea->clear();
    m_asrText.clear();
    m_offlineText.clear();
}

void MainWindow::on_btnSelectFile_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择音频文件", "",
                                                    "音频文件 (*.wav *.pcm *.mp3);;所有文件 (*.*)");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        m_fileData = file.readAll();
        file.close();

        QFileInfo fileInfo(filePath);
        m_fileExt = fileInfo.suffix().toLower();

        // 解析WAV文件头获取采样率
        if (m_fileExt == "wav" && m_fileData.size() >= 44) {
            // WAV文件头结构：偏移24-27是采样率（小端）
            const unsigned char* data = reinterpret_cast<const unsigned char*>(m_fileData.constData());
            m_fileSampleRate = data[24] | (data[25] << 8) | (data[26] << 16) | (data[27] << 24);
            qDebug() << "WAV文件采样率:" << m_fileSampleRate << "Hz";
        } else {
            // 其他格式默认16000Hz
            m_fileSampleRate = 16000;
        }

        info_div->setText(QString("已选择文件: %1，大小: %2 KB，采样率: %3 Hz")
                          .arg(fileInfo.fileName())
                          .arg(m_fileData.size() / 1024)
                          .arg(m_fileSampleRate));

        btnConnect->setEnabled(true);
    } else {
        QMessageBox::warning(this, "错误", "无法打开文件");
    }
}

void MainWindow::on_recoderMode_currentIndexChanged(int index)
{
    m_isFileMode = (index == 1);
    fileModeWidget->setVisible(m_isFileMode);
    micModeWidget->setVisible(!m_isFileMode);

    // 清除文件数据
    if (!m_isFileMode) {
        m_fileData.clear();
    }

    updateUIState();
}

void MainWindow::onWebSocketConnected()
{
    qDebug() << "WebSocket连接成功!";
    info_div->setText("连接成功!");

    // 如果是文件模式，立即发送初始请求并开始发送数据
    if (m_isFileMode && !m_fileData.isEmpty()) {
        qDebug() << "文件模式：立即发送初始请求";

        // 先发送初始请求
        QJsonObject request;
        QJsonArray chunkSizeArray;
        chunkSizeArray.append(5);
        chunkSizeArray.append(10);
        chunkSizeArray.append(5);
        request["chunk_size"] = chunkSizeArray;
        request["wav_name"] = "qt_file";
        request["is_speaking"] = true;  // 文件模式也设置为true
        request["chunk_interval"] = 10;
        request["itn"] = getUseITN();
        request["mode"] = "offline";  // 文件模式固定为offline

        // 设置文件格式
        if (m_fileExt == "wav") {
            request["wav_format"] = "PCM";
            request["audio_fs"] = m_fileSampleRate;
        } else {
            request["wav_format"] = m_fileExt;
        }

        // 热词
        QString hotwords = getHotwords();
        if (!hotwords.isEmpty()) {
            request["hotwords"] = hotwords;
        }

        QJsonDocument doc(request);
        QString requestJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        m_webSocket->sendTextMessage(requestJson);

        qDebug() << "发送文件模式初始请求:" << requestJson;

        // 等待100ms后开始发送文件数据
        info_div->setText("开始发送文件数据...");
        QTimer::singleShot(100, this, SLOT(sendFileData()));
    } else if (!m_isFileMode) {
        // 麦克风模式：启用开始按钮
        btnStart->setEnabled(true);
        btnStop->setEnabled(false);
    }

    updateUIState();
}
void MainWindow::onWebSocketDisconnected()
{
    qDebug() << "WebSocket断开连接";
    info_div->setText("连接已断开");

    // 停止录音
    if (m_isRecording) {
        on_btnStop_clicked();
    }

    // 清理WebSocket
    if (m_webSocket) {
        m_webSocket->deleteLater();
        m_webSocket = nullptr;
    }

    updateUIState();
}

void MainWindow::onWebSocketTextMessageReceived(const QString &message)
{
    qDebug() << "收到文本消息:" << message;

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析错误:" << error.errorString();
        return;
    }

    QJsonObject jsonResponse = doc.object();

    QString rectxt = jsonResponse["text"].toString();
    QString asrmodel = jsonResponse["mode"].toString();
    bool is_final = jsonResponse["is_final"].toBool();

    if (asrmodel == "2pass-offline" || asrmodel == "offline") {
        m_offlineText = rectxt;
        m_asrText = m_offlineText;
    } else {
        m_asrText += rectxt;
    }
    // 同时更新文本框
    varArea->setPlainText(m_asrText);

    // 如果是打字模式，输出到光标位置
    if (isTypingMode && !rectxt.isEmpty()) {
        typeToCursor(rectxt);
        // 在状态栏显示提示
            statusBar()->showMessage(QString("已输入: %1").arg(rectxt), 2000);
    } else {
        // 正常模式：显示在文本框中
        varArea->setPlainText(m_asrText);
    }

    if (is_final) {
        if (isTypingMode) {
            info_div->setText("识别完成，已输出到光标位置");
            // 打字模式不需要显示结果，也不需要等待
            QTimer::singleShot(1000, this, [this]() {
                if (m_webSocket) {
                    m_webSocket->close();
                }
            });
        } else {
            info_div->setText("识别完成!");
            // 正常模式等待3秒后断开连接
            QTimer::singleShot(3000, this, [this]() {
                if (m_webSocket) {
                    info_div->setText("识别完成，正在断开连接...");
                    m_webSocket->close();
                }
            });
        }
    } else {
        info_div->setText("正在识别中...");
    }
}

void MainWindow::onWebSocketBinaryMessageReceived(const QByteArray &message)
{
    // 如果需要处理二进制消息，可以在这里添加
    // qDebug() << "收到二进制消息，长度:" << message.size();
    Q_UNUSED(message);
}

void MainWindow::onWebSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString errorMsg = m_webSocket ? m_webSocket->errorString() : "未知错误";
    qDebug() << "WebSocket错误:" << errorMsg;

    QMessageBox::critical(this, "错误", QString("连接失败: %1").arg(errorMsg));
    info_div->setText("连接失败: " + errorMsg);
    updateUIState();
}

// 修改onAudioReadyRead函数，同时用于录音和监控
void MainWindow::onAudioReadyRead()
{
    // 处理录音数据
    if (m_audioDevice && m_isRecording && !m_isFileMode &&
        m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        QByteArray audioData = m_audioDevice->readAll();
        if (!audioData.isEmpty()) {
            m_webSocket->sendBinaryMessage(audioData);
            m_totalSent += audioData.size();

            // 计算音量等级（用于UI显示）
            calculateAudioLevel(audioData);

            // 更新状态显示
            if (m_totalSent % (16000 * 2) == 0) {
                int seconds = m_totalSent / (16000 * 2);
                info_div->setText(QString("录音中... 已录制: %1秒").arg(seconds));
            }
        }
    }

    // 处理监控数据
    if (monitorDevice && isMiniMode) {
        QByteArray audioData = monitorDevice->readAll();
        if (!audioData.isEmpty()) {
            calculateAudioLevel(audioData);
        }
    }
}


void MainWindow::onSslErrors(const QList<QSslError> &errors)
{
    qDebug() << "SSL错误发生:";
    for (const QSslError &error : errors) {
        qDebug() << "  -" << error.errorString();
    }

    // 忽略所有SSL错误（仅用于测试，生产环境应该验证证书）
    if (m_webSocket) {
        m_webSocket->ignoreSslErrors();
        qDebug() << "已忽略SSL错误";
    }
}
void MainWindow::sendInitialRequest()
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState)
        return;

    // 这个函数只用于麦克风模式
    if (!m_isFileMode && !m_isRecording) {
        QJsonObject request;
        QJsonArray chunkSizeArray;
        chunkSizeArray.append(5);
        chunkSizeArray.append(10);
        chunkSizeArray.append(5);
        request["chunk_size"] = chunkSizeArray;
        request["wav_name"] = "qt_mic";
        request["is_speaking"] = false;  // 开始录音时才设置为true
        request["chunk_interval"] = 10;
        request["itn"] = getUseITN();
        request["mode"] = getAsrMode();

        QString hotwords = getHotwords();
        if (!hotwords.isEmpty()) {
            request["hotwords"] = hotwords;
        }

        QJsonDocument doc(request);
        QString requestJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        m_webSocket->sendTextMessage(requestJson);

        qDebug() << "发送麦克风模式初始请求:" << requestJson;
    }
    // 文件模式的初始请求已经在onWebSocketConnected中发送了
}
void MainWindow::sendFileData()
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "发送文件数据时连接已断开";
        info_div->setText("连接已断开，无法发送文件数据");
        return;
    }

    qDebug() << "开始发送文件数据";
    qDebug() << "文件大小:" << m_fileData.size() << "字节";
    qDebug() << "文件格式:" << m_fileExt;
    qDebug() << "采样率:" << m_fileSampleRate;

    // 如果是wav文件，跳过44字节的文件头
    int startPos = 0;
    if (m_fileExt == "wav" && m_fileData.size() >= 44) {
        startPos = 44;  // 跳过WAV文件头
        qDebug() << "WAV文件，跳过44字节文件头";
    }

    // 按照网页版本的块大小：960字节
    const int chunkSize = 960;
    m_totalSent = 0;

    // 先发送一个开始说话的标记（但网页版本没有这个，只在初始请求中设置了is_speaking:true）
    // 实际上，我们在初始请求中已经设置了is_speaking:true，所以这里不需要再发送

    // 开始发送数据
    for (int pos = startPos; pos < m_fileData.size(); pos += chunkSize) {
        if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
            qDebug() << "发送过程中连接断开";
            break;
        }

        int sendSize = qMin(chunkSize, m_fileData.size() - pos);
        QByteArray chunk = m_fileData.mid(pos, sendSize);

        m_webSocket->sendBinaryMessage(chunk);
        m_totalSent += sendSize;

        // 更新进度
        if (pos % (1024 * 10) == 0) {  // 每发送10KB更新一次
            int percent = (pos * 100) / m_fileData.size();
            info_div->setText(QString("发送文件数据: %1%").arg(percent));

            // 让UI有机会更新
            QCoreApplication::processEvents();

            // 小延迟，避免发送太快
            QThread::msleep(1);
        }
    }

    qDebug() << "文件数据发送完成，共发送:" << m_totalSent << "字节";

    // 发送结束标记（与网页版本一致）
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject stopObj;
        stopObj["is_speaking"] = false;
        QJsonDocument doc(stopObj);
        m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        qDebug() << "发送: is_speaking = false";

        // 150ms后发送finalize
        QTimer::singleShot(150, this, [this]() {
            if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
                QJsonObject finalizeObj;
                finalizeObj["finalize"] = true;
                QJsonDocument doc(finalizeObj);
                m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                qDebug() << "发送: finalize = true";

                info_div->setText("文件数据发送完成，等待识别结果...");
            }
        });
    }
}
void MainWindow::sendFileDataChunk(int chunkSize, int pos)
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    if (pos < m_fileData.size()) {
        int sendSize = qMin(chunkSize, m_fileData.size() - pos);
        QByteArray chunk = m_fileData.mid(pos, sendSize);

        m_webSocket->sendBinaryMessage(chunk);
        pos += sendSize;
        m_totalSent += sendSize;

        // 更新进度
        int percent = (pos * 100) / m_fileData.size();
        info_div->setText(QString("发送文件数据: %1% (%2/%3 KB)")
                          .arg(percent)
                          .arg(pos / 1024)
                          .arg(m_fileData.size() / 1024));

        // 继续发送下一块（延迟一小段时间，避免阻塞UI）
        QTimer::singleShot(10, this, [this, chunkSize, pos]() {
            this->sendFileDataChunk(chunkSize, pos);
        });
    } else {
        // 文件发送完成
        qDebug() << "文件数据发送完成，共发送:" << m_totalSent << "字节";
        info_div->setText("文件数据发送完成，等待识别结果...");

        // 发送停止说话标记
        QJsonObject speakingObj;
        speakingObj["is_speaking"] = false;
        QJsonDocument doc(speakingObj);
        m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        qDebug() << "发送: is_speaking = false";

        // 150ms后发送finalize
        QTimer::singleShot(150, this, [this]() {
            if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
                QJsonObject finalizeObj;
                finalizeObj["finalize"] = true;
                QJsonDocument doc(finalizeObj);
                m_webSocket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                qDebug() << "发送: finalize = true";
            }
        });
    }
}
QString MainWindow::getAsrMode()
{
    if (m_isFileMode) {
        return "offline";
    }
    return asrMode->currentText();
}

bool MainWindow::getUseITN()
{
    return useITN->isChecked();
}

QString MainWindow::getHotwords()
{
    QString hotwordsText = varHot->toPlainText().trimmed();
    if (hotwordsText.isEmpty())
        return QString();

    QStringList lines = hotwordsText.split("\n", QString::SkipEmptyParts);
    QJsonObject hotwords;

    foreach (const QString &line, lines) {
        QStringList parts = line.split(" ", QString::SkipEmptyParts);
        if (parts.size() >= 2) {
            bool ok;
            int weight = parts.last().toInt(&ok);
            if (ok && weight >= 0 && weight <= 100) {
                QString word = parts.mid(0, parts.size() - 1).join(" ");
                hotwords[word] = weight;
            }
        }
    }

    if (hotwords.isEmpty())
        return QString();

    QJsonDocument doc(hotwords);
    // 返回紧凑格式的JSON字符串
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}
// mainwindow.cpp 中修改 onTypingModeChanged 函数
void MainWindow::onTypingModeChanged(int state)
{
    isTypingMode = (state == Qt::Checked);

    if (isTypingMode) {
        info_div->setText("打字模式：识别结果将输出到光标位置");

        // 检查系统是否支持托盘图标
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            setupTrayIcon();
        } else {
            QMessageBox::warning(this, "警告", "当前系统不支持托盘图标功能");
            info_div->setText("当前系统不支持托盘图标");
        }
    } else {
        info_div->setText("正常模式：识别结果显示在本窗口");

        // 清理托盘图标
        if (trayIcon) {
            trayIcon->hide();
            delete trayIcon;
            trayIcon = nullptr;
        }

        // 清理菜单（父对象会自动清理子对象，但显式清理更安全）
        if (trayMenu) {
            delete trayMenu;
            trayMenu = nullptr;
        }
    }
}

// 设置托盘图标
// mainwindow.cpp 中修改 setupTrayIcon 函数
void MainWindow::setupTrayIcon()
{
    if (trayIcon) {
        return; // 已经创建
    }

    // 检查系统托盘支持
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug() << "系统不支持托盘图标";
        return;
    }

    trayIcon = new QSystemTrayIcon(this);

    // 使用自定义图标
        QIcon appIcon(":/logo.ico");
        if (!appIcon.isNull()) {
            trayIcon->setIcon(appIcon);
        } else {
            // 备用图标
            trayIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
        }

    trayIcon->setToolTip("语音输入工具 - 打字模式");

    trayMenu = new QMenu(this);
    showAction = new QAction("显示窗口", this);
    quitAction = new QAction("退出", this);

    trayMenu->addAction(showAction);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);

    trayIcon->setContextMenu(trayMenu);

    // 连接信号槽 - 使用新语法避免运行时错误
    connect(showAction, &QAction::triggered, this, &MainWindow::showNormal);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    connect(trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);

    trayIcon->show();
    trayIcon->showMessage("语音输入工具", "打字模式已启动",
                         QSystemTrayIcon::Information, 3000);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        this->showNormal();
        this->activateWindow();
    }
}
#ifdef Q_OS_WIN
#include <windows.h>
#endif

void MainWindow::typeToCursor(const QString &text)
{
    if (!isTypingMode || text.isEmpty()) {
        return;
    }

    // 打字模式：输出到当前活动窗口
    simulateStringInput(text);

    // 显示提示信息
    if (trayIcon && trayIcon->isSystemTrayAvailable()) {
        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(QSystemTrayIcon::Information);
        trayIcon->showMessage("语音输入完成",
                             QString("已输入：%1").arg(text.left(50)),
                             icon, 3000);
    }
}

#ifdef Q_OS_WIN
// Windows平台实现
void MainWindow::simulateKeyPress(int key, bool ctrl, bool shift)
{
    INPUT input[6] = {0};
    int inputCount = 0;

    // 如果按下Ctrl键
    if (ctrl) {
        input[inputCount].type = INPUT_KEYBOARD;
        input[inputCount].ki.wVk = VK_CONTROL;
        input[inputCount].ki.dwFlags = 0;
        inputCount++;
    }

    // 如果按下Shift键
    if (shift) {
        input[inputCount].type = INPUT_KEYBOARD;
        input[inputCount].ki.wVk = VK_SHIFT;
        input[inputCount].ki.dwFlags = 0;
        inputCount++;
    }

    // 按下目标键
    input[inputCount].type = INPUT_KEYBOARD;
    input[inputCount].ki.wVk = key;
    input[inputCount].ki.dwFlags = 0;
    inputCount++;

    // 释放目标键
    input[inputCount].type = INPUT_KEYBOARD;
    input[inputCount].ki.wVk = key;
    input[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
    inputCount++;

    // 释放Shift键
    if (shift) {
        input[inputCount].type = INPUT_KEYBOARD;
        input[inputCount].ki.wVk = VK_SHIFT;
        input[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
        inputCount++;
    }

    // 释放Ctrl键
    if (ctrl) {
        input[inputCount].type = INPUT_KEYBOARD;
        input[inputCount].ki.wVk = VK_CONTROL;
        input[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
        inputCount++;
    }

    SendInput(inputCount, input, sizeof(INPUT));
}

void MainWindow::simulateStringInput(const QString &text)
{
    // 获取当前焦点窗口
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        qDebug() << "无法获取焦点窗口";
        return;
    }

    // 保存当前剪贴板内容
    QString oldClipboardText = QApplication::clipboard()->text();

    // 设置要输入的文本到剪贴板
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);

        // 等待一小段时间确保剪贴板已更新
        QThread::msleep(50);

        // 模拟Ctrl+V粘贴
        simulateKeyPress('V', true, false);  // Ctrl+V，不要Shift

        // 等待粘贴完成
        QThread::msleep(100);

        qDebug() << "已模拟Ctrl+V粘贴文本:" << text;

        // 恢复剪贴板内容
        if (!oldClipboardText.isEmpty()) {
            QTimer::singleShot(500, [oldClipboardText]() {
                QApplication::clipboard()->setText(oldClipboardText);
            });
        }
    }
}
#else
// 其他平台（macOS/Linux）实现
void MainWindow::simulateKeyPress(int key, bool ctrl, bool shift)
{
    Q_UNUSED(key);
    Q_UNUSED(ctrl);
    Q_UNUSED(shift);
    qDebug() << "此功能在Windows平台外需要额外实现";
}

void MainWindow::simulateStringInput(const QString &text)
{
    // 在其他平台使用Qt的模拟事件
    Q_UNUSED(text);
    qDebug() << "打字输出功能目前主要支持Windows平台";
}
#endif
#ifdef Q_OS_WIN

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);

        if (msg->message == WM_HOTKEY) {
            int hotkeyId = msg->wParam;
            handleHotkey(hotkeyId);
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::registerHotkeys()
{
    startHotkeyId = 1;
    stopHotkeyId = 2;

    // 注册 Ctrl+Alt+S 为开始录音热键
    //RegisterHotKey((HWND)winId(), startHotkeyId, MOD_CONTROL | MOD_ALT, 'S');//原版

    //RegisterHotKey((HWND)winId(), startHotkeyId, MOD_CONTROL , 'QA');
    //RegisterHotKey((HWND)winId(), startHotkeyId, 'A', 'S');
    //RegisterHotKey((HWND)winId(), startHotkeyId,  MOD_ALT, 'S');
    RegisterHotKey((HWND)winId(), startHotkeyId, MOD_CONTROL| MOD_ALT , 'L');



    // 注册 Ctrl+Alt+X 为停止录音热键
    RegisterHotKey((HWND)winId(), stopHotkeyId, MOD_CONTROL | MOD_ALT, 'D');
}

void MainWindow::unregisterHotkeys()
{
    UnregisterHotKey((HWND)winId(), startHotkeyId);
    UnregisterHotKey((HWND)winId(), stopHotkeyId);
}

void MainWindow::handleHotkey(int hotkeyId)
{
    if (hotkeyId == startHotkeyId) {
        // 开始录音
        if (btnStart->isEnabled()) {
            on_btnStart_clicked();
        }
    } else if (hotkeyId == stopHotkeyId) {
        // 停止录音
        if (btnStop->isEnabled()) {
            on_btnStop_clicked();
        }
    }
}

#endif // Q_OS_WIN
// 切换小窗口模式
void MainWindow::toggleMiniMode()
{
    if (isMiniMode) {
        exitMiniMode();
    } else {
        enterMiniMode();
    }
}

// 进入小窗口模式
void MainWindow::enterMiniMode()
{
    // 保存正常模式下的窗口位置和大小
    normalModeGeometry = this->geometry();

    // 停止当前的所有操作
    if (m_isRecording) {
        on_btnStop_clicked();
    }

    // 设置窗口为无边框、置顶
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // 隐藏所有控件
    centralWidget->hide();

    // 删除旧的标签（如果存在）
    if (miniModeLabel) {
        delete miniModeLabel;
        miniModeLabel = nullptr;
    }

    // 创建并显示麦克风图片标签
    miniModeLabel = new QLabel(this);
    miniModeLabel->setAlignment(Qt::AlignCenter);
    miniModeLabel->setScaledContents(false);  // 改为false，我们自己控制缩放

    // 设置窗口大小（正方形）
    int windowSize = 120;  // 稍微小一点，看起来更精致
    setFixedSize(windowSize, windowSize);

    // 设置窗口背景为半透明深色
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("MainWindow {"
                  "background-color: rgba(30, 30, 40, 180);"
                  "border-radius: 8px;"
                  "border: 1px solid rgba(80, 80, 100, 150);"
                  "}");

    // 设置标签充满整个窗口（减去一些边距）
    int margin = 5;
    int labelSize = windowSize - 2 * margin;
    miniModeLabel->setGeometry(margin, margin, labelSize, labelSize);
    miniModeLabel->setStyleSheet("background-color: transparent;");

    // 设置初始图片
    if (!micPixmaps.isEmpty() && !micPixmaps[0].isNull()) {
        // 初始使用最低音量图片
        updateMicrophoneLevel();  // 这会设置图片和亮度
    } else {
        qDebug() << "警告：无法加载麦克风图片";
        // 创建一个简单的测试图片
        QPixmap testPixmap(labelSize, labelSize);
        testPixmap.fill(QColor(80, 120, 200, 200));
        miniModeLabel->setPixmap(testPixmap);
    }

    miniModeLabel->show();

    // 显示窗口
    show();
    isMiniMode = true;

    // 开始监控麦克风音量
    setupMicrophoneMonitoring();

    qDebug() << "进入小窗口模式，窗口大小：" << windowSize << "x" << windowSize;
}
// 退出小窗口模式
void MainWindow::exitMiniMode()
{
    // 停止麦克风监控
    stopMicrophoneMonitoring();

    // 恢复窗口标志
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint & ~Qt::WindowStaysOnTopHint);

    // 清除样式表
    setStyleSheet("");
    setAttribute(Qt::WA_TranslucentBackground, false);

    // 隐藏麦克风标签
    if (miniModeLabel) {
        miniModeLabel->hide();
        delete miniModeLabel;
        miniModeLabel = nullptr;
    }

    // 显示所有控件
    centralWidget->show();

    // 恢复窗口位置和大小
    setGeometry(normalModeGeometry);

    // 恢复窗口大小策略
    setMinimumSize(800, 600);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    // 重要：重新设置窗口标志后需要重新显示窗口
    show();

    isMiniMode = false;

    qDebug() << "退出小窗口模式";
}
// 设置麦克风监控
void MainWindow::setupMicrophoneMonitoring()
{
    // 如果已经有监控，先停止
    stopMicrophoneMonitoring();

    // 设置音频格式（与录音相同）
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    // 检查是否有可用的麦克风
    QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (devices.isEmpty()) {
        qDebug() << "未找到可用的麦克风设备";
        return;
    }

    // 检查格式是否支持
    if (!devices.first().isFormatSupported(format)) {
        qDebug() << "不支持的音频格式";
        return;
    }

    // 创建音频输入用于监控
    monitorAudioInput = new QAudioInput(devices.first(), format, this);
    monitorDevice = monitorAudioInput->start();

    if (monitorDevice) {
        connect(monitorDevice, SIGNAL(readyRead()), this, SLOT(onAudioReadyRead()));
    }

    // 启动音量更新定时器
    if (levelTimer) {
        levelTimer->start();
    }

    qDebug() << "开始监控麦克风音量";
}

// 停止麦克风监控
void MainWindow::stopMicrophoneMonitoring()
{
    if (levelTimer && levelTimer->isActive()) {
        levelTimer->stop();
    }

    if (monitorAudioInput) {
        monitorAudioInput->stop();
        delete monitorAudioInput;
        monitorAudioInput = nullptr;
        monitorDevice = nullptr;
    }

    qDebug() << "停止监控麦克风音量";
}

// 更新麦克风音量显示
void MainWindow::updateMicrophoneLevel()
{
    if (!isMiniMode || !miniModeLabel || micPixmaps.isEmpty()) {
        return;
    }

    // 计算图片索引 (0-6)，使用更宽松的映射
    // 使用指数函数让中等音量就能达到高等级
    double mappedLevel = audioLevel;

    // 增强映射，让低音量也显示变化
    if (mappedLevel < 0.3) {
        // 低音量区域：线性映射
        mappedLevel = mappedLevel * 2.0;
    } else {
        // 中高音量区域：指数增长，更容易达到高等级
        mappedLevel = 0.6 + (mappedLevel - 0.3) * 1.5;
    }

    // 限制在0-1范围
    mappedLevel = qBound(0.0, mappedLevel, 1.0);

    // 计算图片索引
    int index = qBound(0, static_cast<int>(mappedLevel * 6), 6);

    // 确保索引在有效范围内
    if (index >= micPixmaps.size()) {
        index = 0;
    }

    // 获取对应的图片
    QPixmap basePixmap = micPixmaps[index];
    if (basePixmap.isNull()) {
        return;
    }

    // 计算亮度因子（根据音量动态调整）
    // 调整亮度范围，让最低亮度也不太暗
    double brightnessFactor = 0.5 + audioLevel * 0.5; // 范围：0.5 - 1.0

    // 获取当前窗口大小
    int windowSize = qMin(width(), height());

    // 直接使用原图，让标签的setScaledContents(true)处理缩放
    if (!basePixmap.isNull()) {
        // 创建临时图片并调整亮度
        QPixmap adjustedPixmap = adjustPixmapBrightness(basePixmap, brightnessFactor);

        // 设置图片，标签会自动缩放填充
        miniModeLabel->setPixmap(adjustedPixmap);

        // 确保标签充满整个窗口
        miniModeLabel->setGeometry(0, 0, windowSize, windowSize);
    }

    // 调试信息（每30次更新打印一次）
    static int counter = 0;
    if (counter++ % 30 == 0) {
        qDebug() << "更新麦克风显示 - 原始音量:" << audioLevel
                 << ", 映射后:" << mappedLevel
                 << ", 索引:" << index
                 << ", 亮度:" << brightnessFactor;
    }
}

// 计算音频等级
void MainWindow::calculateAudioLevel(const QByteArray &data)
{
    if (data.isEmpty()) {
        audioLevel = 0.0;
        return;
    }

    // 将字节数据转换为16位整数
    const qint16 *samples = reinterpret_cast<const qint16*>(data.constData());
    int sampleCount = data.size() / sizeof(qint16);

    if (sampleCount == 0) {
        audioLevel = 0.0;
        return;
    }

    // 计算RMS（均方根）值
    double sum = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        double sample = samples[i] / 32768.0; // 归一化到[-1, 1]
        sum += sample * sample;
    }

    double rms = sqrt(sum / sampleCount);

    // 平滑处理（避免跳动太快）
    static double smoothedLevel = 0.0;
    smoothedLevel = smoothedLevel * 0.7 + rms * 0.3;

    // 调整音量映射，使最高音量等级更容易达到
    // 使用对数映射，但调整参数使其更加敏感
    if (smoothedLevel > 0.0) {
        // 转换成分贝（dB），但使用更宽松的范围
        double db = 20 * log10(smoothedLevel);

        // 调整映射范围，使正常说话就能达到较高等级
        // 原范围：-60dB到0dB 映射到 0.0-1.0
        // 新范围：-40dB到-10dB 映射到 0.0-1.0，这样更容易达到高等级
        audioLevel = qBound(0.0, (db + 40.0) / 60.0, 1.0);

        // 增加非线性映射，让低音量时变化慢，高音量时变化快
        // 使用平方根函数，让中等音量就能达到较高等级
        audioLevel = sqrt(audioLevel);

        // 确保最小值不为0，避免图片完全消失
        audioLevel = qMax(0.05, audioLevel);

        // 调试信息
        static int debugCounter = 0;
        if (debugCounter++ % 50 == 0) {
            qDebug() << "音量计算 - RMS:" << rms << ", dB:" << db
                     << ", 映射后:" << audioLevel << ", 平滑后:" << smoothedLevel;
        }
    } else {
        audioLevel = 0.05; // 最小基础值，避免完全消失
    }

    // 添加一些随机变化，使显示更生动（当音量很低时）
    if (audioLevel < 0.1) {
        static bool seeded = false;
        if (!seeded) {
            qsrand(QTime::currentTime().msec());
            seeded = true;
        }
        // 添加轻微随机变化，使图片不会完全静止
        audioLevel = qMax(0.05, audioLevel + (qrand() % 6 - 3) * 0.002);
    }
}
// 鼠标事件支持窗口拖拽
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (isMiniMode && event->button() == Qt::LeftButton) {
        isDragging = true;
        dragPosition = event->globalPos() - this->pos();
        event->accept();
    } else {
        QMainWindow::mousePressEvent(event);
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (isMiniMode && isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint newPos = event->globalPos() - dragPosition;
        move(newPos);
        event->accept();
    } else {
        QMainWindow::mouseMoveEvent(event);
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (isMiniMode && event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    } else {
        QMainWindow::mouseReleaseEvent(event);
    }
}
/// 辅助函数：调整图片亮度和对比度
QPixmap MainWindow::adjustPixmapBrightness(const QPixmap& pixmap, double brightnessFactor)
{
    if (pixmap.isNull()) {
        return pixmap;
    }

    // 获取窗口大小
    int windowSize = qMin(width(), height());

    // 缩放图片到窗口大小
    QPixmap scaledPixmap = pixmap.scaled(windowSize, windowSize,
                                         Qt::KeepAspectRatioByExpanding,  // 扩展填充
                                         Qt::SmoothTransformation);

    // 创建一个临时图片，填充背景色（可选）
    QPixmap adjustedPixmap(windowSize, windowSize);

    // 如果原图有透明区域，可以在这里填充一个背景色
    // 如果原图是完全不透明的，可以跳过填充
    QPainter painter(&adjustedPixmap);

    // 可选：填充一个微弱的背景色（如果图片有透明区域）
    // painter.fillRect(0, 0, windowSize, windowSize, QColor(30, 30, 50, 200));

    // 调整亮度并绘制
    painter.setOpacity(brightnessFactor);

    // 居中绘制图片
    int x = (windowSize - scaledPixmap.width()) / 2;
    int y = (windowSize - scaledPixmap.height()) / 2;
    painter.drawPixmap(x, y, scaledPixmap);

    painter.end();

    return adjustedPixmap;
}
