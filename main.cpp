#include <QWidget>
#include <QString>
#include <QFont>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QApplication>
#include <QEvent>
#include "textedit.h"

// ### TODO ###
// ### Should clear selection when something else selects something.
// ### Line break. Could vastly simplify textlayout if not breaking lines.
// ### saving to same file. Need something there.
// ### could refactor chunks so that I only have one and split when I need. Not sure if it's worth it. Would
// ### need chunkData(int from = 0, int size = -1). The current approach makes caching easier since I can now cache an entire chunk.
// ### ensureCursorVisible(TextCursor) is not implemented
// ### add a command line switch to randomly do stuff. Insert, move around etc n times to see if I can reproduce a crash. Allow input of seed
// ### I still have some weird debris when scrolling in large documents
// ### use tests from Qt. E.g. for QTextCursor
// ### need to protect against undo with huge amounts of text in it. E.g. select all and delete. MsgBox asking?
// ### maybe refactor updatePosition so it can handle the margins usecase that textcursor needs.
// ### could keep undo/redo history in the textcursor and have an api on the cursor.
// ### block state in SyntaxHighlighter doesn't remember between
// ### highlights. In regular QText.* it does. Could store this info.
// ### Shouldn't be that much. Maybe base it off of position in
// ### document rather than line number since I don't know which line
// ### we're on
// ### TextCursor(const TextEdit *). Is this a good idea?
// ### caching stuff is getting a little convoluted
// ### what should TextDocument::read(documentSize + 1, 10) do? ASSERT?
// ### should I allow to write in a formatted manner? currentTextFormat and all? Why not really.
// ### consider using QTextBoundaryFinder for something
// ### drag and drop
// ### documentation
// ### consider having extra textLayouts on each side of viewport for optimized scrolling. Could detect that condition
// ### Undo section removal/adding. This is a mess


class Highlighter : public SyntaxHighlighter
{
public:
    Highlighter(QWidget *parent)
        : SyntaxHighlighter(parent)
    {

    }

    void helper(int from, int size, bool blackForeground)
    {
        QTextCharFormat format;
        format.setBackground(blackForeground ? Qt::yellow : Qt::black);
        format.setForeground(blackForeground ? Qt::black : Qt::yellow);
        setFormat(from, size, format);
//        qDebug() << "setting format" << from << size << currentBlock().mid(from, size) << blackForeground;
    }

    virtual void highlightBlock(const QString &text)
    {
        enum { Space, LetterOrNumber, Other } state = Space;
        int last = 0;
        for (int i=0; i<text.size(); ++i) {
            if (text.at(i).isSpace()) {
                if (state != Space)
                    helper(last, i - last, state == LetterOrNumber);
                state = Space;
                last = i;
            } else if (text.at(i).isLetterOrNumber()
                       != (state == LetterOrNumber) || state == Space) {
                if (state != Space) {
                    helper(last, i - last, state == LetterOrNumber);
                }
                state = (text.at(i).isLetterOrNumber()
                         ? LetterOrNumber
                         : Other);
                last = i;
            }
        }
        if (state != Space) {
            helper(last, text.size() - last, state == LetterOrNumber);
        }
    }
};

class Yellow : public SyntaxHighlighter
{
public:
    Yellow(TextEdit *edit)
        : SyntaxHighlighter(edit)
    {}
    void highlightBlock(const QString &text)
    {
        QTextCharFormat format;
        format.setBackground(Qt::yellow);
        setFormat(0, text.size(), format);
    }

};

bool add = false;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0)
        : QMainWindow(parent), doLineNumbers(false)
    {
        QString fileName = "main.cpp";
        bool replay = false;
        bool readOnly = false;
        int chunkSize = -1;
        QTextCodec *codec = 0;
        const QStringList list = QApplication::arguments().mid(1);
        for (int i=0; i<list.size(); ++i) {
            const QString &arg = list.at(i);
            if (arg == "--replay") {
                replay = true;
                fileName.clear();
            } else if (arg == "--add") {
                replay = true;
                add = true;
                fileName.clear();
            } else if (arg == "--log") {
                fileName.clear();
                appendTimer.start(10, this);
                readOnly = true;
                chunkSize = 1000;
            } else if (arg == "--readonly") {
                readOnly = true;
            } else if (arg == "--linenumbers") {
                doLineNumbers = true;
            } else if (arg.startsWith("--codec=")) {
                codec = QTextCodec::codecForName(arg.mid(8).toLatin1());
                if (!codec) {
                    qWarning("Can't load codec called '%s'", qPrintable(arg.mid(8)));
                    exit(1);
                }
            } else if (arg.startsWith("--chunksize=")) {
                bool ok;
                chunkSize = arg.mid(12).toInt(&ok);
                if (!ok) {
                    qWarning("Can't parse %s", qPrintable(arg));
                    exit(1);
                }
            } else {
                fileName = arg;
            }
        }
        if (fileName.isEmpty() && !appendTimer.isActive()) {
            Q_ASSERT(replay);
            QStringList list = QDir(".").entryList(QStringList() << "*.log");
            if (list.isEmpty()) {
                qWarning("Nothing to replay");
                exit(1);
            }
            qSort(list);
            fileName = list.last();
        }
        if (replay) {
            QFile file(fileName);
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning("Can't open '%s' for reading", qPrintable(fileName));
                exit(1);
            }
#ifndef QT_NO_DEBUG_STREAM
            qDebug() << "replaying" << fileName;
#endif
#ifndef QT_NO_DEBUG
            if (add) {
                extern QString logFileName;
                logFileName = fileName;
            }
            extern bool doLog; // from textedit.cpp
            doLog = false;
#endif
            QDataStream ds(&file);
            ds >> fileName; // reuse this variable for loading document. First QString in log file
            while (!file.atEnd()) {
                int type;
                ds >> type;
                if (type == QEvent::KeyPress || type == QEvent::KeyRelease) {
                    int key;
                    int modifiers;
                    QString text;
                    bool isAutoRepeat;
                    int count;
                    ds >> key;
                    ds >> modifiers;
                    ds >> text;
                    ds >> isAutoRepeat;
                    ds >> count;
                    events.append(new QKeyEvent(static_cast<QEvent::Type>(type), key,
                                                static_cast<Qt::KeyboardModifiers>(modifiers),
                                                text, isAutoRepeat, count));
                } else if (type == QEvent::Resize) {
                    QSize size;
                    ds >> size;
                    events.append(new QResizeEvent(size, QSize()));
                } else {
                    Q_ASSERT(type == QEvent::MouseMove || type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease);
                    QPoint pos;
                    int button;
                    int buttons;
                    int modifiers;
                    ds >> pos;
                    ds >> button;
                    ds >> buttons;
                    ds >> modifiers;
                    events.append(new QMouseEvent(static_cast<QEvent::Type>(type), pos,
                                                  static_cast<Qt::MouseButton>(button),
                                                  static_cast<Qt::MouseButtons>(buttons),
                                                  static_cast<Qt::KeyboardModifiers>(modifiers)));
                }
            }
            if (!events.isEmpty())
                QTimer::singleShot(0, this, SLOT(sendNextEvent()));
        }

        QWidget *w = new QWidget(this);
        QVBoxLayout *l = new QVBoxLayout(w);
        setCentralWidget(w);
        l->addWidget(textEdit = new TextEdit(w));
        textEdit->setObjectName("primary");
        if (chunkSize != -1) {
            textEdit->document()->setChunkSize(chunkSize);
        }
        l->addWidget(otherEdit = new TextEdit);
        otherEdit->setReadOnly(true);
//        otherEdit->hide();
        otherEdit->setLineBreaking(false);
        otherEdit->setObjectName("otherEdit");
        textEdit->setReadOnly(readOnly);
        QFontDatabase fdb;
        foreach(QString family, fdb.families()) {
            if (fdb.isFixedPitch(family)) {
                QFont f(family);
                textEdit->setFont(f);
                break;
            }
        }

//        textEdit->setSyntaxHighlighter(new Highlighter(textEdit));
#ifndef QT_NO_DEBUG_STREAM
        if (codec) {
            qDebug() << "using codec" << codec->name();
        }
#endif
        if (appendTimer.isActive())
            textEdit->document()->setOption(TextDocument::SwapChunks, true);
        if (!fileName.isEmpty() && !textEdit->load(fileName, TextDocument::Sparse, codec)) {
#ifndef QT_NO_DEBUG_STREAM
            qDebug() << "Can't load" << fileName;
#endif
        }
#if 0
        textEdit->document()->setText("This is a test");
//        textEdit->setSyntaxHighlighter(new Yellow(textEdit));
        QTextCharFormat format;
        format.setBackground(Qt::red);
        textEdit->document()->insertTextSection(0, 4, format)->setPriority(10000);
        format = QTextCharFormat();
        format.setBackground(Qt::blue);
        textEdit->insertTextSection(0, 7, format);
#endif
        otherEdit->setDocument(textEdit->document());

        lbl = new QLabel(w);
        connect(textEdit, SIGNAL(cursorPositionChanged(int)),
                this, SLOT(onCursorPositionChanged(int)));
        l->addWidget(lbl);

        new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_E), textEdit, SLOT(ensureCursorVisible()));
        new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_L), this, SLOT(createTextSection()));
        new QShortcut(QKeySequence(QKeySequence::Close), this, SLOT(close()));
        new QShortcut(QKeySequence(Qt::Key_F2), this, SLOT(changeTextSectionFormat()));

        new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S), this, SLOT(save()));
        new QShortcut(QKeySequence(Qt::ALT + Qt::Key_S), this, SLOT(saveAs()));

        QMenu *menu = menuBar()->addMenu("&File");
        menu->addAction("About textedit", this, SLOT(about()));

        QHBoxLayout *h = new QHBoxLayout;
        h->addWidget(box = new QSpinBox(centralWidget()));
        connect(textEdit->verticalScrollBar(), SIGNAL(valueChanged(int)),
                this, SLOT(onScrollBarValueChanged()));

        new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_G), this, SLOT(gotoPos()));

        box->setReadOnly(true);
        box->setRange(0, INT_MAX);
        l->addLayout(h);

        connect(textEdit, SIGNAL(sectionClicked(TextSection *, QPoint)), this, SLOT(onTextSectionClicked(TextSection *, QPoint)));

        textEdit->viewport()->setAutoFillBackground(true);
        connect(textEdit->document(), SIGNAL(modificationChanged(bool)), this, SLOT(onModificationChanged(bool)));
    }

    void timerEvent(QTimerEvent *e)
    {
        if (e->timerId() == appendTimer.timerId()) {
            static int line = 0;
            textEdit->append(QString("This is line number %1\n").arg(++line));
            TextCursor curs = textEdit->textCursor();
            curs.movePosition(TextCursor::End);
            curs.movePosition(TextCursor::PreviousBlock);
            textEdit->setTextCursor(curs);
            textEdit->ensureCursorVisible();
            if (line % 100 == 0) {
                qDebug() << "memory used" << textEdit->document()->currentMemoryUsage()
                         << "documentSize" << textEdit->document()->documentSize();
            }
        } else {
            QMainWindow::timerEvent(e);
        }
    }

    void closeEvent(QCloseEvent *e)
    {
        QSettings("LazyTextEditor", "LazyTextEditor").setValue("geometry", saveGeometry());
        QMainWindow::closeEvent(e);
    }
    void showEvent(QShowEvent *e)
    {
        activateWindow();
        raise();
#ifndef QT_NO_DEBUG
        extern bool doLog; // from textedit.cpp
        if (doLog)
#endif
            restoreGeometry(QSettings("LazyTextEditor", "LazyTextEditor").value("geometry").toByteArray());
        QMainWindow::showEvent(e);
    }
public slots:
    void onModificationChanged(bool on)
    {
        QPalette pal = textEdit->viewport()->palette();
        pal.setColor(textEdit->viewport()->backgroundRole(), on ? Qt::yellow : Qt::white);
        textEdit->viewport()->setPalette(pal);
    }

    void save()
    {
        textEdit->document()->save();
    }

    void saveAs()
    {
        const QString str = QFileDialog::getSaveFileName(this, "Save as", ".");
        if (!str.isEmpty())
            textEdit->document()->save(str);
    }
    void about()
    {
        textEdit->textCursor().setPosition(0);
        QMessageBox::information(this, "Textedit", QString("Current memory usage %1KB").
                                 arg(textEdit->document()->currentMemoryUsage() / 1024),
                                 QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton);
    }

    void onCursorPositionChanged(int pos)
    {
        QString text = QString("Position: %1\n"
                               "Word: %2\n").
                       arg(pos).
                       arg(textEdit->textCursor().wordUnderCursor());
        if (doLineNumbers)
            text += QString("Line number: %0").arg(textEdit->document()->lineNumber(pos));
        lbl->setText(text);
    }

    void sendNextEvent()
    {
        QEvent *ev = events.takeFirst();
        QWidget *target = textEdit->viewport();
        switch (ev->type()) {
        case QEvent::Resize:
            resize(static_cast<QResizeEvent*>(ev)->size());
            break;
        case QEvent::KeyPress:
            if (static_cast<QKeyEvent*>(ev)->matches(QKeySequence::Close))
                break;
            // fall through
            target = textEdit;
        default:
            QApplication::postEvent(target, ev);
            break;
        }

        if (!events.isEmpty()) {
            QTimer::singleShot(0, this, SLOT(sendNextEvent()));
#ifndef QT_NO_DEBUG
        } else if (add) {
            extern bool doLog; // from textedit.cpp
            doLog = true;
#endif
        }
    }

    void createTextSection()
    {
        TextCursor cursor = textEdit->textCursor();
        if (cursor.hasSelection()) {
            static bool first = true;
            QTextCharFormat format;
            if (first) {
                format.setForeground(Qt::blue);
                format.setFontUnderline(true);
            } else {
                format.setBackground(Qt::black);
                format.setForeground(Qt::white);
            }
            const int pos = cursor.selectionStart();
            const int size = cursor.selectionEnd() - pos;
            TextSection *s = 0;
            if (first) {
                s = textEdit->insertTextSection(pos, size, format, cursor.selectedText());
                if (s) {
                    s->setCursor(Qt::PointingHandCursor);
                    Q_UNUSED(s);
                    Q_ASSERT(s);
                }

                Q_ASSERT(!otherEdit->sections().contains(s));
                Q_ASSERT(!s || textEdit->sections().contains(s));
            } else {
                s = textEdit->document()->insertTextSection(pos, size, format, cursor.selectedText());
            }
            first = !first;
        }
    }

    void changeTextSectionFormat()
    {
        static bool first = true;
        QTextCharFormat format;
        format.setFontUnderline(true);
        if (first) {
            format.setForeground(Qt::white);
            format.setBackground(Qt::blue);
        } else {
            format.setForeground(Qt::blue);
        }
        first = !first;
        foreach(TextSection *s, textEdit->sections()) {
            s->setFormat(format);
        }

    }
    void onTextSectionClicked(TextSection *section, const QPoint &pos)
    {
#ifndef QT_NO_DEBUG_STREAM
        qDebug() << section->text() << section->data() << pos;
#endif
    }
    void onScrollBarValueChanged()
    {
        box->setValue(textEdit->viewportPosition());
    }
    void gotoPos()
    {
        TextCursor &cursor = textEdit->textCursor();
        bool ok;
        int pos = QInputDialog::getInteger(this, "Goto pos", "Pos", cursor.position(), 0,
                                           textEdit->document()->documentSize(), 1, &ok);
        if (!ok)
            return;
        cursor.setPosition(pos);
    }

private:
    QSpinBox *box;
    TextEdit *textEdit, *otherEdit;
    QLabel *lbl;
    QLinkedList<QEvent*> events;
    bool doLineNumbers;
    QBasicTimer appendTimer;
};

#include "main.moc"


int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    MainWindow w;
    w.resize(500, 100);
    w.show();
    return a.exec();
}
