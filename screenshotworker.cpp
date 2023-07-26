#include "screenshotworker.h"

ScreenshotWorker::ScreenshotWorker(QObject *parent)
    : QObject{parent}
{

}

void ScreenshotWorker::processScreenshot(const QPixmap &previousScreenshot, const QPixmap &newScreenshot)
{
    double similarity = calculateImageDiff(previousScreenshot.toImage(), newScreenshot.toImage());
    emit comparisonResult(similarity);
}

double ScreenshotWorker::calculateImageDiff(const QImage &image1, const QImage &image2)
{
    int width = image1.width();
    int height = image1.height();

    int pixelCount = width * height;
    double mse = 0.0;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            QRgb pixel1 = image1.pixel(x, y);
            QRgb pixel2 = image2.pixel(x, y);

            int r1 = qRed(pixel1);
            int g1 = qGreen(pixel1);
            int b1 = qBlue(pixel1);

            int r2 = qRed(pixel2);
            int g2 = qGreen(pixel2);
            int b2 = qBlue(pixel2);

            int dr = r1 - r2;
            int dg = g1 - g2;
            int db = b1 - b2;

            mse += dr * dr + dg * dg + db * db;
        }
    }

    mse /= pixelCount;
    double similarity = 100.0 * (1.0 - sqrt(mse) / 255.0);

    return similarity;
}
