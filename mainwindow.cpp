#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setMinimumSize(800, 600);

    QWidget::setWindowTitle("Zheludchenko Test Task");

    screenshotButton = new QPushButton("Зробити знімок екрану", this);
    connect(screenshotButton, &QPushButton::clicked, this, &MainWindow::onScreenshotButtonClicked);

    timerToggleButton = new QPushButton("Увімкнути таймер", this);
    connect(timerToggleButton, &QPushButton::clicked, this, &MainWindow::onTimerToggleClicked);

    currentScreenshotLabel = new QLabel(this);
    currentScreenshotLabel->setAlignment(Qt::AlignCenter);

    previousScreenshotLabel = new QLabel(this);
    previousScreenshotLabel->setAlignment(Qt::AlignCenter);

    similarityLabel = new QLabel("Зробіть два знімки екрану для розрахунку їх схожості", this);
    similarityLabel->setAlignment(Qt::AlignCenter);

    currentScreenshotTitleLabel = new QLabel("Поточний знімок екрану", this);
    currentScreenshotTitleLabel->setAlignment(Qt::AlignCenter);
    previousScreenshotTitleLabel = new QLabel("Попередній знімок екрану", this);
    previousScreenshotTitleLabel->setAlignment(Qt::AlignCenter);


    // Using QGridLayout to place elements
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

    screenshotWorker.moveToThread(&workerThread);
    connect(this, &MainWindow::startScreenshotComparison, &screenshotWorker, &ScreenshotWorker::processScreenshot);
    connect(&screenshotWorker, &ScreenshotWorker::comparisonResult, this, &MainWindow::onComparisonResult);
    workerThread.start();

    createDatabaseTable();
    loadScreenshotsFromDatabase();
}

/*===================================================ScreenShoot======================================================*/

// Slot for the "Take a screenshot" button click
void MainWindow::onScreenshotButtonClicked()
{
    takeScreenshoot();
}

// Function to calculate the hash of the screenshot
QByteArray MainWindow::calculateImageHash(const QPixmap &screenshot)
{
    if(screenshot.isNull())
        return 0;

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    screenshot.save(&buffer, "PNG");

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(byteArray);

    return hash.result();
}

// Function to get a screenshot
void MainWindow::takeScreenshoot()
{
    // Get the screen
    QScreen *screen = QGuiApplication::primaryScreen();

    if (screen) // If the screen was successfully captured
    {
        QPixmap newScreenshot = screen->grabWindow(0);
        double similarity = 0.0;

        // If we already have a previous screenshot, compare it with the new one
        if (!previousScreenshotPixmap.isNull())
        {
            emit startScreenshotComparison(previousScreenshotPixmap, newScreenshot);
        }
        else
            similarityLabel->setText("Зробіть ще один знімок екрану");

        // Display current and previous screenshots
        currentScreenshotLabel->setPixmap(newScreenshot.scaled(currentScreenshotLabel->size(), Qt::KeepAspectRatio));
        previousScreenshotLabel->setPixmap(previousScreenshotPixmap.scaled(previousScreenshotLabel->size(), Qt::KeepAspectRatio));

        previousScreenshotPixmap = newScreenshot;

        saveScreenshotToDatabase(newScreenshot, similarity);
    }
    else    // If failed to capture the screen
        QMessageBox::critical(this, "Помилка", "Не вдалося захопити екран.");
}

/*===================================================Timer============================================================*/

// Slot for timer timeout
void MainWindow::onTimerTimeout()
{
    takeScreenshoot();
}

// Function to toggle the timer on/off
void MainWindow::onTimerToggleClicked()
{
    if (isTimerRunning)
    {
        timer->stop();
        timerToggleButton->setText("Увімкнути таймер");
        isTimerRunning = false;
    }
    else
    {
        timer->start(60000);
        timerToggleButton->setText("Вимкнути таймер");
        isTimerRunning = true;
    }
}

/*===================================================DataBase=========================================================*/

// Function to load screenshots from the database
void MainWindow::loadScreenshotsFromDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen())
    {
        QMessageBox::critical(this, "Помилка", "Не вдалося відкрити базу даних.");
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT image FROM screenshots ORDER BY id DESC LIMIT 2");
    if (!query.exec())
    {
        QMessageBox::critical(this, "Помилка", "Не вдалося виконати запит до бази даних.");
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

        // Remember the last two screenshots
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

    // Display the similarity percentage of the current screenshot
    if (screenshotCount == 1)
    {
        similarityLabel->setText("Прошлого скріншота поки що немає");
    }
    else if (screenshotCount == 2)
    {
        // Perform similarity calculation in a separate thread
        loadScreenshotsWorker(previousScreenshot, currentScreenshot);
    }
}

// Difference calculation function for the database
void MainWindow::loadScreenshotsWorker(QPixmap previousScreenshot, QPixmap currentScreenshot)
{
    // Call the affinity calculation on the worker thread
    emit startScreenshotComparison(previousScreenshot, currentScreenshot);
}

// Function to save the screenshot to the database
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
        QMessageBox::critical(this, "Помилка", "Не вдалося відкрити базу даних.");
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO screenshots (image, hash, similarity_percentage) VALUES (?, ?, ?)");
    query.addBindValue(byteArray);
    query.addBindValue(hash.toHex());
    query.addBindValue(roundedPercentage);

    if (!query.exec())
    {
        QMessageBox::critical(this, "Помилка", "Не вдалося додати дані в базу даних.");
    }
    else
    {
        // Remove old records, leaving only the last two screenshots
        query.prepare("DELETE FROM screenshots WHERE id NOT IN (SELECT id FROM screenshots ORDER BY id DESC LIMIT 2)");
        if (!query.exec())
        {
            QMessageBox::critical(this, "Помилка", "Не вдалося видалити старі дані з бази даних.");
        }
    }
}

// Function to create the database table
void MainWindow::createDatabaseTable()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/screenshots.db"); // Database name

    if (!db.open())
    {
        QMessageBox::critical(this, "Помилка", "Не вдалося відкрити базу даних.");
        return;
    }

    QSqlQuery query;
    // Create the screenshots table containing columns for image (BLOB), hash (TEXT), and similarity percentage (REAL)
    QString createTableQuery = "CREATE TABLE IF NOT EXISTS screenshots ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "image BLOB NOT NULL,"
                               "hash TEXT NOT NULL,"
                               "similarity_percentage REAL NOT NULL)";
    if (!query.exec(createTableQuery))
    {
        QMessageBox::critical(this, "Помилка", "Не вдалося створити таблицю в базі даних.");
        return;
    }

    db.close();

    qDebug() << "Путь до бази даних:" << db.databaseName();
}

/*===================================================General==========================================================*/

// Function to change the field
void MainWindow::onComparisonResult(double similarity)
{
    QString similarityText = QString("Схожість: %1%").arg(similarity, 0, 'f', 2);
    similarityLabel->setText(similarityText);
}

// Destructor
MainWindow::~MainWindow()
{
    workerThread.quit();
    workerThread.wait();
    delete ui;
}
