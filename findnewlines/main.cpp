#include <QtCore>
class Thread : public QThread
{
    Q_OBJECT
public:
    Thread(const QString &file, QObject *parent = 0)
        : QThread(parent), fileName(file)
    {}

    void run()
    {
        QTime timer;
        timer.start();
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
            return;
        QVector<int> newLines;
        newLines.reserve(file.size() / 50);
        newLines.append(0);
        int pos = 0;
        QTextStream ts(&file);
        QChar ch;
        const QChar newline('\n');
        while (!ts.atEnd()) {
            ts >> ch;
            if (ch == newline) {
                newLines.append(pos);
            }
            ++ pos;
        }
        qDebug() << timer.elapsed() << newLines.size() << newLines.capacity();
        newLines.squeeze();
        qDebug() << newLines.capacity();

    }
private:
    const QString fileName;
};

#include "main.moc"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);
    Thread t("../biglong");
    t.start();
    t.wait();
    return 0;
}
