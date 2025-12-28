#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "websocket.h"
#include <QMainWindow>
#include <QAudioInput>
#include <QAudioFormat>
#include <QIODevice>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
//#include <QNetworkProxyFactory>
#include <QNetworkAccessManager>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QStatusBar>
#include <QClipboard>
//#include <QHotkey>
#include <QIcon>  // 添加这行
#include <QTimer>
#include <QShortcut>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QPlainTextEdit;
class QGroupBox;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    // 添加这些成员函数
protected:
    #ifdef Q_OS_WIN
        bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

    #endif
        void mousePressEvent(QMouseEvent *event) override;
            void mouseMoveEvent(QMouseEvent *event) override;
            void mouseReleaseEvent(QMouseEvent *event) override;
private slots:
    void on_btnConnect_clicked();
    void on_btnDisconnect_clicked();
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void on_btnClear_clicked();
    void on_btnSelectFile_clicked();
    void on_recoderMode_currentIndexChanged(int index);

    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketTextMessageReceived(const QString &message);
    void onWebSocketBinaryMessageReceived(const QByteArray &message);
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onAudioReadyRead();
    void onSslErrors(const QList<QSslError> &errors);
    void testConnection();  // 添加这个
    void testSimpleWebSocket();
    void onTestConnectionFinished(QNetworkReply *reply);
    void onWebSocketStateChanged(QAbstractSocket::SocketState state);
   void sendFileDataChunk(int chunkSize, int pos);
   void sendFileData();
   void onTypingModeChanged(int state);
     void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
     void toggleMiniMode();  // 切换小窗口模式
     void updateMicrophoneLevel();  // 更新麦克风音量显示
     void calculateAudioLevel(const QByteArray &data);  // 计算音频等级



private:

    void setupUI();
    void setupAudioFormat();
    void updateUIState();
    void sendInitialRequest();
    // 添加方法
    void setupTrayIcon();
        void typeToCursor(const QString &text);
 void simulateKeyPress(int key, bool ctrl = false, bool shift = false);
 void simulateStringInput(const QString &text);
 void registerHotkeys();
 void unregisterHotkeys();
 void handleHotkey(int hotkeyId);
 void enterMiniMode();  // 进入小窗口模式
 void exitMiniMode();   // 退出小窗口模式
 void setupMicrophoneMonitoring();  // 设置麦克风监控
 void stopMicrophoneMonitoring();   // 停止麦克风监控
 QPixmap adjustPixmapBrightness(const QPixmap& pixmap, double brightnessFactor);

    QString getAsrMode();
    bool getUseITN();
    QString getHotwords();

    // WebSocket相关
    QWebSocket *m_webSocket;
    QAudioInput *m_audioInput;
    QIODevice *m_audioDevice;
    QAudioFormat m_audioFormat;
   QNetworkAccessManager *m_networkManager;

    // 录音状态
    bool m_isRecording;
    bool m_isFileMode;
    QByteArray m_fileData;
    QString m_fileExt;
    int m_fileSampleRate;
    qint64 m_totalSent;
    // 文本处理
    QString m_asrText;
    QString m_offlineText;

    // UI控件声明
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QGridLayout *connectionLayout;
    QGridLayout *micModeLayout;
    QVBoxLayout *fileModeLayout;

    QLabel *startHotkeyLabel;  // 开始录音快捷键标签
    QLabel *stopHotkeyLabel;   // 停止录音快捷键标签
    QLabel *labelWssIp;
    QLineEdit *wssip;
    QPushButton *btnConnect;
    QPushButton *btnDisconnect;
    QLabel *labelRecoderMode;
    QComboBox *recoderMode;
    QLabel *info_div;

    QGroupBox *micModeWidget;
    QLabel *labelAsrMode;
    QComboBox *asrMode;
    QCheckBox *useITN;
    QPlainTextEdit *varHot;
    QPushButton *btnStart;
    QPushButton *btnStop;
    QPushButton *btnClear;
    QPushButton *btnTest;

    QGroupBox *fileModeWidget;
    QPushButton *btnSelectFile;
    QPlainTextEdit *varArea;

    // 添加这些成员变量  打字模式和托盘相关
//    QHotkey *startHotkey;
//        QHotkey *stopHotkey;
        QCheckBox *autoTypeCheckBox;
        QSystemTrayIcon *trayIcon;
        QMenu *trayMenu;
        QAction *showAction;
        QAction *quitAction;
        bool isTypingMode;  // 是否为打字输出模式

     // Windows热键ID
     int startHotkeyId;
     int stopHotkeyId;
     // 小窗口模式相关
         bool isMiniMode;               // 是否是小窗口模式
         bool isDragging;               // 是否正在拖拽窗口
         QPoint dragPosition;           // 拖拽位置
         QLabel *miniModeLabel;         // 小窗口中的标签（显示麦克风图片）
         QShortcut *miniModeShortcut;   // 切换小窗口模式的快捷键

         // 麦克风监控相关
         QAudioInput *monitorAudioInput;
         QIODevice *monitorDevice;
         QTimer *levelTimer;            // 定时器，用于定期检查音量
         double audioLevel;             // 当前音频等级 (0.0 - 1.0)
         QList<QPixmap> micPixmaps;     // 存储7个麦克风图片
         QPixmap currentMicPixmap;      // 当前显示的麦克风图片

         // 窗口位置记忆（用于恢复）
         QRect normalModeGeometry;      // 正常模式下的窗口位置和大小

};

#endif // MAINWINDOW_H
