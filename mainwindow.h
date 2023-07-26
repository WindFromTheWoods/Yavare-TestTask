#ifndef MAINWINDOW_H
#define MAINWINDOW_H
// QT
#include <QMainWindow>
#include <QGridLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QLabel>
#include <QImage>
#include <QPixmap>
#include <QTime>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
// DataBase
#include <QSqlDatabase>
#include <QSqlError>
#include <QtSql>
// STL
#include <cmath>

#include "./screenshotworker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void takeScreenshoot();

private:
    Ui::MainWindow *ui;

    // General
    QGridLayout *layout;
    QWidget *centralWidget;

    // Buttons
    QPushButton *screenshotButton;
    QPushButton *timerToggleButton;

    // Screenshoot
    QLabel *currentScreenshotLabel;
    QLabel *previousScreenshotLabel;
    QLabel *similarityLabel;
    QPixmap previousScreenshotPixmap = QPixmap();
    QPixmap screenshotPixmap = QPixmap();
    QLabel *currentScreenshotTitleLabel; // Метка для надписи "Текущий скриншот"
    QLabel *previousScreenshotTitleLabel; // Метка для надписи "Прошлый скриншот"
    ScreenshotWorker screenshotWorker;
    QThread workerThread;

    // Timer
    QTimer *timer;
    bool isTimerRunning;

    // DataBase
    QSqlDatabase db;
    void createDatabaseTable();
    void saveScreenshotToDatabase(const QPixmap& screenshot, double similarityPercentage);
    QByteArray calculateImageHash(const QPixmap& screenshot);
    void loadScreenshotsFromDatabase();
    void loadScreenshotsWorker(QPixmap previousScreenshot, QPixmap currentScreenshot);

private slots:
    void onScreenshotButtonClicked();
    void onTimerToggleClicked();
    void onTimerTimeout();
    void onComparisonResult(double similarity);

signals:
    void startScreenshotComparison(const QPixmap& previousScreenshot, const QPixmap& newScreenshot);
};
#endif // MAINWINDOW_H
