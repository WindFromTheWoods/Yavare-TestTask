#ifndef SCREENSHOTWORKER_H
#define SCREENSHOTWORKER_H

#include <QObject>
#include <QThread>
#include <QPixmap>

class ScreenshotWorker : public QObject
{
    Q_OBJECT

public:
    ScreenshotWorker(QObject *parent = nullptr);


public slots:
    void processScreenshot(const QPixmap& previousScreenshot, const QPixmap& newScreenshot);

signals:
    void comparisonResult(double similarity);

private:
    double calculateImageDiff(const QImage& image1, const QImage& image2);
};

#endif // SCREENSHOTWORKER_H
