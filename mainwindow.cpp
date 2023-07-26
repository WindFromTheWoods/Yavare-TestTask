#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setMinimumSize(800, 600);

    QWidget::setWindowTitle("Zheludchenko Test Task");

    screenshotButton = new QPushButton("Сделать скриншот", this);
    connect(screenshotButton, &QPushButton::clicked, this, &MainWindow::onScreenshotButtonClicked);

    timerToggleButton = new QPushButton("Включить таймер", this);
    connect(timerToggleButton, &QPushButton::clicked, this, &MainWindow::onTimerToggleClicked);

    currentScreenshotLabel = new QLabel(this);
    currentScreenshotLabel->setAlignment(Qt::AlignCenter);

    previousScreenshotLabel = new QLabel(this);
    previousScreenshotLabel->setAlignment(Qt::AlignCenter);

    similarityLabel = new QLabel("Сделайте два скриншота для расчета их сходства", this);
    similarityLabel->setAlignment(Qt::AlignCenter);

    currentScreenshotTitleLabel = new QLabel("Текущий скриншот", this);
    currentScreenshotTitleLabel->setAlignment(Qt::AlignCenter);
    previousScreenshotTitleLabel = new QLabel("Прошлый скриншот", this);
    previousScreenshotTitleLabel->setAlignment(Qt::AlignCenter);


    // Используем QGridLayout для размещения элементов
    layout = new QGridLayout;
    layout->addWidget(currentScreenshotTitleLabel,  0, 0);
    layout->addWidget(currentScreenshotLabel,       1, 0);
    layout->addWidget(previousScreenshotTitleLabel, 0, 1);
    layout->addWidget(previousScreenshotLabel,      1, 1);
    layout->addWidget(similarityLabel,              2, 0);
    layout->addWidget(screenshotButton,             3, 0);
    layout->addWidget(timerToggleButton,            3, 1);

    centralWidget = new QWidget(this);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::onTimerTimeout);
    isTimerRunning = false;

    createDatabaseTable();
    loadScreenshotsFromDatabase();

    screenshotWorker.moveToThread(&workerThread);
    connect(this, &MainWindow::startScreenshotComparison, &screenshotWorker, &ScreenshotWorker::processScreenshot);
    connect(&screenshotWorker, &ScreenshotWorker::comparisonResult, this, &MainWindow::onComparisonResult);
    workerThread.start();
}

// Функция включения/выключения таймера
void MainWindow::onTimerToggleClicked()
{
    if (isTimerRunning)
    {
        timer->stop();
        timerToggleButton->setText("Включить таймер");
        isTimerRunning = false;
    }
    else
    {
        timer->start(60000);
        timerToggleButton->setText("Выключить таймер");
        isTimerRunning = true;
    }
}

// Функция получения скриншота
void MainWindow::takeScreenshoot()
{
    // Получаем экран
    QScreen *screen = QGuiApplication::primaryScreen();

    if (screen) // Если удалось захватить экран
    {
        QPixmap newScreenshot = screen->grabWindow(0);
        double similarity = 0.0;

        // Если у нас уже есть скриншот - сравниваем его с прошлым
        if (!previousScreenshotPixmap.isNull())
        {
            emit startScreenshotComparison(previousScreenshotPixmap, newScreenshot);
        }
        else
            similarityLabel->setText("Сделайте еще один скриншот");

        // Отображаем текущий и предыдущий скриншоты
        currentScreenshotLabel->setPixmap(newScreenshot.scaled(currentScreenshotLabel->size(), Qt::KeepAspectRatio));
        previousScreenshotLabel->setPixmap(previousScreenshotPixmap.scaled(previousScreenshotLabel->size(), Qt::KeepAspectRatio));

        previousScreenshotPixmap = newScreenshot;

        saveScreenshotToDatabase(newScreenshot, similarity);
    }
    else    // Если не удалось захватить экран
        QMessageBox::critical(this, "Ошибка", "Не удалось захватить экран.");
}

// Функция расчета разницы между скриншотами
double MainWindow::calculateImageDiff(const QImage &image1, const QImage &image2)
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

void MainWindow::onComparisonResult(double similarity)
{
    QString similarityText = QString("Сходство: %1%").arg(similarity, 0, 'f', 2);
    similarityLabel->setText(similarityText);
}

//
void MainWindow::createDatabaseTable()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/screenshots.db"); // Имя базы данных

    if (!db.open())
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть базу данных.");
        return;
    }

    QSqlQuery query;
    // Создаем таблицу screenshots, содержащую столбцы для изображения (BLOB), хэша (TEXT) и процента сходства (REAL)
    QString createTableQuery = "CREATE TABLE IF NOT EXISTS screenshots ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "image BLOB NOT NULL,"
                               "hash TEXT NOT NULL,"
                               "similarity_percentage REAL NOT NULL)";
    if (!query.exec(createTableQuery))
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать таблицу в базе данных.");
        return;
    }

    db.close();

    qDebug() << "Путь к базе данных:" << db.databaseName();
}

void MainWindow::saveScreenshotToDatabase(const QPixmap &screenshot, double similarityPercentage)
{
    if (screenshot.isNull())
        return;

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    screenshot.save(&buffer, "PNG");

    QByteArray hash = calculateImageHash(screenshot);
    double roundedPercentage = qRound(similarityPercentage * 100.0) / 100.0;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть базу данных.");
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO screenshots (image, hash, similarity_percentage) VALUES (?, ?, ?)");
    query.addBindValue(byteArray);
    query.addBindValue(hash.toHex());
    query.addBindValue(roundedPercentage);

    if (!query.exec())
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось добавить данные в базу данных.");
    }
    else
    {
        // Удаляем старые записи, чтобы оставить только два последних скриншота
        query.prepare("DELETE FROM screenshots WHERE id NOT IN (SELECT id FROM screenshots ORDER BY id DESC LIMIT 2)");
        if (!query.exec())
        {
            QMessageBox::critical(this, "Ошибка", "Не удалось удалить старые данные из базы данных.");
        }
    }
}

QByteArray MainWindow::calculateImageHash(const QPixmap &screenshot)
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    screenshot.save(&buffer, "PNG");

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(byteArray);

    return hash.result();
}

void MainWindow::loadScreenshotsFromDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть базу данных.");
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT image FROM screenshots ORDER BY id DESC LIMIT 2");
    if (!query.exec())
    {
        QMessageBox::critical(this, "Ошибка", "Не удалось выполнить запрос к базе данных.");
        return;
    }

    QPixmap previousScreenshot;
    QPixmap currentScreenshot;

    int screenshotCount = 0;
    while (query.next())
    {
        QByteArray imageByteArray = query.value(0).toByteArray();
        QPixmap screenshot;
        screenshot.loadFromData(imageByteArray, "PNG");

        // Запоминаем два последних скриншота
        if (screenshotCount == 0)
        {
            currentScreenshot = screenshot;
            currentScreenshotLabel->setPixmap(currentScreenshot.scaled(currentScreenshotLabel->size(), Qt::KeepAspectRatio));
        }
        else if (screenshotCount == 1)
        {
            previousScreenshot = screenshot;
            previousScreenshotLabel->setPixmap(previousScreenshot.scaled(previousScreenshotLabel->size(), Qt::KeepAspectRatio));
        }

        screenshotCount++;
    }

    // Отображаем процент сходства текущего скриншота
    if (screenshotCount == 1)
    {
        similarityLabel->setText("Прошлого скриншота пока нет");
    }
    else if (screenshotCount == 2)
    {
        double similarity = calculateImageDiff(previousScreenshot.toImage(), currentScreenshot.toImage());
        QString similarityText = QString("Сходство: %1%").arg(similarity, 0, 'f', 2);
        similarityLabel->setText(similarityText);
    }
}

// Слот нажатия на кнопку "Сделать скриншот"
void MainWindow::onScreenshotButtonClicked()
{
    takeScreenshoot();
}

// Слот срабатывания таймера
void MainWindow::onTimerTimeout()
{
    takeScreenshoot();
}

// Деструктор
MainWindow::~MainWindow()
{
    workerThread.quit();
    workerThread.wait();
    delete ui;
}
