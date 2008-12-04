/****************************************************************************
**
** Copyright (C) 1992-$THISYEAR$ Trolltech AS. All rights reserved.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <QtTest/QtTest>

#include <textdocument.h>
QT_FORWARD_DECLARE_CLASS(TextDocument)

//TESTED_CLASS=
//TESTED_FILES=

class tst_TextDocument : public QObject
{
    Q_OBJECT

public:
    tst_TextDocument();
    virtual ~tst_TextDocument();

private slots:
    void cursor_data();
    void cursor();
    void undoRedo();
    void save();
    void documentSize();
    void setText();
    void setText_data();
    void readCheck();
    void readCheck_data();
    void find();
    void find2();
    void find3_data();
    void find3();
    void findQChar_data();
    void findQChar();
    void sections();
};

tst_TextDocument::tst_TextDocument()
{
}

tst_TextDocument::~tst_TextDocument()
{
}

static const int chunkSizes[] = { -1, /* default */ 1000, 100, 10, 0 };
void tst_TextDocument::readCheck_data()
{
    QTest::addColumn<int>("deviceMode");
    QTest::addColumn<int>("chunkSize");
    QTest::addColumn<bool>("useIODevice");

    struct {
        const char *name;
        int deviceMode;
    } modes[] = { { "LoadAll", TextDocument::LoadAll },
                  { "Sparse", TextDocument::Sparse },
                  { 0, -1 } };

    for (int i=0; chunkSizes[i] != 0; ++i) {
        for (int j=0; modes[j].name; ++j) {
            for (int k=0; k<2; ++k) {
                QTest::newRow(QString("%0 %1 %2").arg(modes[j].name).arg(chunkSizes[i]).arg(bool(k)).toLatin1().constData())
                    << modes[j].deviceMode << chunkSizes[i] << bool(k);
            }
        }
    }
}

void tst_TextDocument::readCheck()
{
    QFETCH(int, deviceMode);
    QFETCH(int, chunkSize);
    QFETCH(bool, useIODevice);
    TextDocument doc;
    QFile file("tst_textdocument.cpp");
    QFile file2(useIODevice ? file.fileName() : QString());
    QVERIFY(file.open(QIODevice::ReadOnly));
    if (useIODevice)
        QVERIFY(file2.open(QIODevice::ReadOnly));
    if (chunkSize != -1)
        doc.setChunkSize(chunkSize);
    if (useIODevice) {
        QVERIFY(doc.load(&file2, TextDocument::DeviceMode(deviceMode)));
    } else {
        QVERIFY(doc.load(file.fileName(), TextDocument::DeviceMode(deviceMode)));
    }
    static const int sizes[] = { 1, 100, -1 };
    for (int i=0; sizes[i] != -1; ++i) {
        for (int j=0; !file.atEnd(); j+=sizes[i]) {
            const QString dr = doc.read(j, sizes[i]);

            QCOMPARE(QString::fromLatin1(file.read(sizes[i])), dr);
            QCOMPARE(dr.at(0), doc.readCharacter(j));
        }
        file.seek(0);
    }
}

void tst_TextDocument::find()
{
    TextDocument doc;
    doc.setText("This is one line\nThis is another\n");
    QVERIFY(!doc.find('\n', 0).isNull());
    QString searchTerm = "another";
    doc.append(searchTerm);
    int index = doc.find(searchTerm, 0).position();
    QVERIFY(index != -1);
    const int first = index;
    index = doc.find(searchTerm, index).position();
    QCOMPARE(index, first);

    index = doc.find(searchTerm, index + 1).position();
    QCOMPARE(index, doc.documentSize() - searchTerm.size());
    QCOMPARE(doc.find(searchTerm, index - 1, TextDocument::FindBackward).position(), first);
    QCOMPARE(doc.find(searchTerm, index + 1).position(), -1);
    QCOMPARE(doc.find(searchTerm, index - 2, TextDocument::FindWholeWords).position(), index);
    QVERIFY(!doc.find(searchTerm, 0).isNull());
    searchTerm.chop(1);
    searchTerm.remove(0, 1);
    QVERIFY(doc.find(searchTerm, 0, TextDocument::FindWholeWords).isNull());
    QCOMPARE(doc.find("This", 0, TextDocument::FindWholeWords).position(), 0);
    doc.remove(1, 1);
    QCOMPARE(doc.find("This", 0, TextDocument::FindWholeWords).position(), 16);
}

void tst_TextDocument::find2()
{
    TextDocument doc;
    doc.setChunkSize(2);
    doc.insert(0, "foobar");
    QCOMPARE(doc.find("oo", 0).position(), 1);
    QCOMPARE(doc.find("foobar", 0).position(), 0);
    QCOMPARE(doc.find("foobar", doc.documentSize(), TextDocument::FindBackward).position(), 0);
    QCOMPARE(doc.find("b", 0).position(), 3);
    QCOMPARE(doc.find("b", doc.documentSize(), TextDocument::FindBackward).position(), 3);

}

void tst_TextDocument::find3_data()
{
    QTest::addColumn<QRegExp>("regExp");
    QTest::addColumn<int>("position");
    QTest::addColumn<int>("flags");
    QTest::addColumn<int>("expected");

    QTest::newRow("1") << QRegExp("This") << 0 << 0 << 0;
    QTest::newRow("2") << QRegExp("This") << 1 << 0 << 17;
    QTest::newRow("3") << QRegExp("This") << -1 << int(TextDocument::FindBackward) << 17;
    QTest::newRow("4") << QRegExp("This") << 16 << int(TextDocument::FindBackward) << 0;
    QTest::newRow("5") << QRegExp("\\bis") << 16 << int(TextDocument::FindBackward) << 5;
    QTest::newRow("6") << QRegExp("is") << 0 << 0 << 2;
    QTest::newRow("7") << QRegExp("\\bis") << 0 << 0 << 5;
}


void tst_TextDocument::find3()
{
    TextDocument doc;
    doc.setText("This is one line\nThis is another");
    QFETCH(QRegExp, regExp);
    QFETCH(int, position);
    QFETCH(int, flags);
    QFETCH(int, expected);
    if (position == -1)
        position = doc.documentSize();

    QCOMPARE(doc.find(regExp, position, (TextDocument::FindMode)flags).position(), expected);
}


void tst_TextDocument::setText_data()
{
    QTest::addColumn<int>("chunkSize");
    for (int i=0; chunkSizes[i] != 0; ++i) {
        QTest::newRow(QString::number(chunkSizes[i]).toLatin1().constData()) << chunkSizes[i];
    }
}

void tst_TextDocument::setText()
{
    QString ba;
    const QString line("abcdefghijklmnopqrstuvwxyz\n");
    for (int i=0; i<100; ++i) {
        ba.append(line);
    }

    QFETCH(int, chunkSize);
    TextDocument doc;
    if (chunkSize != -1)
        doc.setChunkSize(chunkSize);
    doc.setText(ba);

    QCOMPARE(ba, doc.read(0, doc.documentSize()));
}

void tst_TextDocument::save()
{
    TextDocument doc;
//    doc.load("tst_textdocument.cpp");
    doc.setText("testing testing testing testing testing");
    doc.insert(0, "This is inserted at 0\n");
    doc.insert(10, "This is inserted at 10\n");
    QVERIFY(doc.read(0, doc.documentSize()).contains("This is inserted at 10"));
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QVERIFY(doc.save(&buffer));
    QCOMPARE(doc.documentSize(), buffer.data().size());
    const QString docSaved = doc.read(0, doc.documentSize());
    QVERIFY(docSaved.contains("This is inserted"));
    buffer.close();
    buffer.open(QIODevice::ReadOnly);
    const QString buf = QTextStream(&buffer).readAll();
    QVERIFY(docSaved == buf);
}

void tst_TextDocument::documentSize()
{
    QBuffer buffer;
    buffer.setData("foobar\n");
    buffer.open(QIODevice::ReadOnly);
    int bufferSize = buffer.size();
    TextDocument doc;
    doc.load(&buffer);
    QCOMPARE(int(buffer.size()), doc.documentSize());
    doc.remove(0, 2);
    bufferSize -= 2;
    QCOMPARE(bufferSize, doc.documentSize());
    doc.insert(0, "foobar");
    bufferSize += 6;
    QCOMPARE(bufferSize, doc.documentSize());

    doc.append("foobar");
    bufferSize += 6;
    QCOMPARE(bufferSize, doc.documentSize());
}

void tst_TextDocument::undoRedo()
{
    TextDocument doc;
    const QString initial = "Initial text";
    doc.setText(initial);
    QVERIFY(doc.isUndoRedoEnabled());
    QVERIFY(!doc.isUndoAvailable());
    QVERIFY(!doc.isRedoAvailable());
    TextCursor cursor(&doc);
    cursor.insertText("f");
    QVERIFY(doc.isUndoAvailable());
    QVERIFY(!doc.isRedoAvailable());
    doc.undo();
    QVERIFY(!doc.isUndoAvailable());
    QVERIFY(doc.isRedoAvailable());
    QCOMPARE(initial, doc.read(0, doc.documentSize()));
    doc.redo();
    QVERIFY(doc.isUndoAvailable());
    QVERIFY(!doc.isRedoAvailable());
    QCOMPARE("f" + initial, doc.read(0, doc.documentSize()));

    doc.undo();
    QVERIFY(!doc.isUndoAvailable());
    cursor.setPosition(0);
    cursor.movePosition(TextCursor::WordRight, TextCursor::KeepAnchor);
    QCOMPARE(cursor.selectedText(), QString("Initial"));
    cursor.insertText("B");
    QCOMPARE(doc.read(0, doc.documentSize()), QString("B text"));

    QVERIFY(doc.isUndoAvailable());
    doc.undo();
    QCOMPARE(initial, doc.read(0, doc.documentSize()));
    QVERIFY(doc.isRedoAvailable());

}

struct Command {
    Command(int p = -1, const QString &t = QString()) : pos(p), removeCount(-1), text(t) {}
    Command(int p, int r) : pos(p), removeCount(r) {}

    const int pos;
    const int removeCount;
    const QString text;
};

Q_DECLARE_METATYPE(Command);

void tst_TextDocument::cursor_data()
{
    QTest::addColumn<int>("position");
    QTest::addColumn<int>("anchor");
    QTest::addColumn<Command>("command");
    QTest::addColumn<int>("expectedPosition");
    QTest::addColumn<int>("expectedAnchor");

    QTest::newRow("1") << 0 << 0 << Command(1, "foo") << 0 << 0;
    QTest::newRow("2") << 0 << 0 << Command(1, 1) << 0 << 0;
    QTest::newRow("3") << 0 << 0 << Command(0, "foo") << 3 << 3;
    QTest::newRow("4") << 0 << 0 << Command(0, 1) << 0 << 0;
    QTest::newRow("5") << 1 << 1 << Command(0, 1) << 0 << 0;
    QTest::newRow("6") << 0 << 3 << Command(1, 1) << 0 << 2;
    QTest::newRow("7") << 0 << 3 << Command(1, "b") << 0 << 4;

}

void tst_TextDocument::cursor()
{
    static const char *text = "abcdefghijklmnopqrstuvwxyz\n";
    TextDocument doc;
    doc.setText(QString::fromLatin1(text));

    QFETCH(int, position);
    QFETCH(int, anchor);
    QFETCH(Command, command);
    QFETCH(int, expectedPosition);
    QFETCH(int, expectedAnchor);

    TextCursor cursor(&doc, position, anchor);
    if (command.removeCount > 0) {
        doc.remove(command.pos, command.removeCount);
    } else {
        doc.insert(command.pos, command.text);
    }
    QCOMPARE(cursor.position(), expectedPosition);
    QCOMPARE(cursor.anchor(), expectedAnchor);
}

void tst_TextDocument::sections()
{
    static const char *text = "abcdefghijklmnopqrstuvwxyz\n";
    TextDocument doc;
    doc.setText(QString::fromLatin1(text));
    Section *section = doc.insertSection(0, 4);
    QVERIFY(section);
    QVERIFY(!doc.insertSection(2, 5));
    QCOMPARE(doc.sections(0, 4).value(0), section);
    QCOMPARE(doc.sections(0, 7).value(0), section);
    QCOMPARE(doc.sections(0, 3).size(), 0);
    QCOMPARE(doc.sections(0, 3, TextDocument::IncludePartial).value(0), section);
    QCOMPARE(doc.sections(2, 6, TextDocument::IncludePartial).value(0), section);
}


void tst_TextDocument::findQChar_data()
{
    QTest::addColumn<QChar>("ch");
    QTest::addColumn<int>("position");
    QTest::addColumn<int>("flags");
    QTest::addColumn<int>("expected");

    QTest::newRow("1") << QChar('t') << 0 << 0 << 0;
    QTest::newRow("2") << QChar('t') << 0 << int(TextDocument::FindCaseSensitively) << 28;
    QTest::newRow("3") << QChar('T') << 0 << 0 << 0;
    QTest::newRow("4") << QChar('T') << 0 << int(TextDocument::FindCaseSensitively) << 0;

    QTest::newRow("5") << QChar('t') << -1 << int(TextDocument::FindBackward) << 28;
    QTest::newRow("6") << QChar('t') << -1 << int(TextDocument::FindBackward|TextDocument::FindCaseSensitively) << 28;
    QTest::newRow("7") << QChar('T') << -1 << int(TextDocument::FindBackward) << 28;
    QTest::newRow("8") << QChar('T') << -1 << int(TextDocument::FindBackward|TextDocument::FindCaseSensitively) << 17;

    QTest::newRow("9") << QChar('t') << 28 << int(TextDocument::FindBackward) << 28;
}


void tst_TextDocument::findQChar()
{
    TextDocument doc;
    doc.setText("This is one line\nThis is another");
    QFETCH(QChar, ch);
    QFETCH(int, position);
    QFETCH(int, flags);
    QFETCH(int, expected);
    if (position == -1)
        position = doc.documentSize();

//    qDebug() << ch << doc.readCharacter(position) << position << doc.read(qMax(0, position - 3), 7) << position;
//    qDebug() << ch << position << doc.documentSize() << flags << expected;
    QCOMPARE(doc.find(ch, position, (TextDocument::FindMode)flags).position(), expected);
    QCOMPARE(doc.find(QString(ch), position, (TextDocument::FindMode)flags).position(), expected);
}



QTEST_MAIN(tst_TextDocument)
#include "tst_textdocument.moc"
