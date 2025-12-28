#include <mainwindow.h>
#include <QApplication>
#include <QProcess>
#include <QDebug>
#include <QMessageBox>
#include <QIcon>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 设置应用程序图标
        a.setWindowIcon(QIcon(":/logo.ico"));  // 从资源文件加载

    // 测试WebSocket服务器是否可达
    qDebug() << "测试WebSocket服务器连接...";

    // 使用curl测试（如果系统有curl）
    QProcess process;
    process.start("curl", QStringList() << "-I" << "https://ai-input.com");

    if (process.waitForFinished(5000)) {
        QByteArray output = process.readAllStandardOutput();
        qDebug() << "Curl输出:" << output;
    } else {
        qDebug() << "Curl测试超时或未找到curl";
    }

    try {
            // 设置应用程序信息
            QApplication::setApplicationName("语音输入工具");
            QApplication::setOrganizationName("MyCompany");

            MainWindow w;
            w.show();

            return a.exec();
        } catch (const std::exception &e) {
            qCritical() << "程序异常退出:" << e.what();
            QMessageBox::critical(nullptr, "程序错误",
                                QString("程序发生异常:\n%1").arg(e.what()));
            return 1;
        }

    //return a.exec();
}
