// Copyright 2010 Anders Bakken
//
// Licensed under the Apache License, Version 2.0(the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <QtTest/QtTest>

#define private public
// to be able to access private data in textdocument
#include <textdocument.h>
#include <textdocument_p.h>
QT_FORWARD_DECLARE_CLASS(TextDocument)

//TESTED_CLASS=
//TESTED_FILES=

    Q_DECLARE_METATYPE(TextDocument::Options);
    Q_DECLARE_METATYPE(TextDocument::DeviceMode);

class tst_TextDocument : public QObject
{
    Q_OBJECT

public:
    tst_TextDocument();
    virtual ~tst_TextDocument();

private:
    int convertRowColumnToIndex(const TextDocument *doc, int row, int column);

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
    void findWholeWordsRecursionCrash();
    void sections();
    void iterator();
    void modified();
    void unicode();
    void lineNumbers();
    void lineNumbersGenerated();
    void lineNumbersGenerated_data();
    void carriageReturns_data();
    void carriageReturns();
    void isWordOverride();
    void chunkBacktrack();
    void insertText();
    void memUsage();
    void textDocmentIteratorOnDocumentSize();
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
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("deviceMode");
    QTest::addColumn<int>("chunkSize");
    QTest::addColumn<bool>("useIODevice");

    static const char *fileNames[] = { /*"tst_textdocument.cpp", */"unicode.txt", 0 };
    struct {
        const char *name;
        int deviceMode;
    } modes[] = { { "LoadAll", TextDocument::LoadAll },
                  { "Sparse", TextDocument::Sparse },
                  { 0, -1 } };

    for (int f=0; fileNames[f]; ++f) {
        for (int i=0; chunkSizes[i] != 0; ++i) {
            for (int j=0; modes[j].name; ++j) {
                for (int k=0; k<2; ++k) {
                    QTest::newRow(QString("%0 %1 %2 %3").
                                  arg(QString::fromLatin1(fileNames[f])).
                                  arg(modes[j].name).arg(chunkSizes[i]).
                                  arg(bool(k)).toLatin1().constData())
                        << QString::fromLatin1(fileNames[f])
                        << modes[j].deviceMode
                        << chunkSizes[i]
                        << bool(k);
                }
            }
        }
    }
}

void tst_TextDocument::readCheck()
{
    QFETCH(QString, fileName);
    QFETCH(int, deviceMode);
    QFETCH(int, chunkSize);
    QFETCH(bool, useIODevice);
    TextDocument doc;
    QFile file(fileName);
    QFile file2(useIODevice ? fileName : QString());
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

    const QString fileData = QTextStream(&file).readAll();
    QCOMPARE(fileData.size(), doc.documentSize());
    for (int i=0; sizes[i] != -1; ++i) {
        for (int j=0; j<fileData.size(); j+=sizes[i]) {
            const int max = qMin(sizes[i], doc.documentSize() - j);
            const QString dr = doc.read(j, max);
            QCOMPARE(dr.size(), max);

            const QString fromTs = fileData.mid(j, max);
            QCOMPARE(dr.size(), fromTs.size());
//             if (fromTs != dr) {
//                 qDebug() << fromTs.size() << dr.size()
//                          << fromTs.startsWith(dr)
//                          << dr.startsWith(fromTs)
//                          << sizes[i] << j << doc.documentSize();
//                 for (int k=0; k<dr.size(); ++k) {
//                     if (dr.at(k) != fromTs.at(k)) {
//                         qDebug() << k << "is different" <<
//                             dr.at(k).row() << dr.at(k).cell() << fromTs.at(k).row() << fromTs.at(k).cell();
//                     }
//                 }


//                 exit(0);
//             }
            QCOMPARE(fromTs, dr);
            QCOMPARE(dr.at(0), doc.readCharacter(j));
        }
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
    for (int i=0; i<2; ++i) {
        ba.append(line);
    }

    QFETCH(int, chunkSize);
    TextDocument doc;
    if (chunkSize != -1)
        doc.setChunkSize(chunkSize);
    doc.setText(ba);

    QCOMPARE(doc.read(0, doc.documentSize()), ba);
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
//    if (doc.read(0, doc.documentSize()) != buf) {
    if (docSaved != buf) {
        QVERIFY(docSaved != buf);
        QVERIFY(docSaved == doc.read(0, doc.documentSize()));
        {
            QFile file("buf");
            file.open(QIODevice::WriteOnly);
            QTextStream(&file) << buf;
        }
        {
            QFile file("doc");
            file.open(QIODevice::WriteOnly);
            QTextStream(&file) << docSaved;
        }
    }
//    QVERIFY(doc.read(0, doc.documentSize()) == buf);
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
    doc.setUndoRedoEnabled(false);
    QVERIFY(!doc.isRedoAvailable());
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
    TextSection *section = doc.insertTextSection(0, 4);
    QVERIFY(section);
    QVERIFY(doc.insertTextSection(2, 5));
    QCOMPARE(doc.sections(0, 4).value(0), section);
    QCOMPARE(doc.sections(0, 7).value(0), section);
    QCOMPARE(doc.sections(0, 3).size(), 0);
    QCOMPARE(doc.sections(0, 3, TextSection::IncludePartial).value(0), section);
    QCOMPARE(doc.sections(2, 6, TextSection::IncludePartial).value(0), section);
    TextSection *section2 = doc.insertTextSection(4, 2);
    QVERIFY(section2);
    QCOMPARE(doc.sections(3, 2, TextSection::IncludePartial).size(), 3);
    QCOMPARE(doc.sections(3, 2).size(), 0);

    // Insert identical sections, make sure we pull out the right one
    TextSection *section3 = doc.insertTextSection(5, 5);
    TextSection *section4 = doc.insertTextSection(5, 5);
    doc.takeTextSection(section4);
    QVERIFY(doc.sections().contains(section3));
    // "[abcd][e]fghijklmnopqrstuvwxyz\n";
}

void tst_TextDocument::findWholeWordsRecursionCrash()
{
    QTemporaryFile file;
    file.open();
    QTextStream ts(&file);
    for (int i=0; i<100000; ++i) {
        ts << "_sometext_" << endl;
    }

    TextDocument document;
    QVERIFY(document.load(file.fileName()));
    QVERIFY(document.find("omet", 0, TextDocument::FindWholeWords).isNull());
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


void tst_TextDocument::iterator()
{
    TextDocument doc;
    doc.setChunkSize(100);
    QVERIFY(doc.load("../../textedit.cpp", TextDocument::Sparse));
    TextDocumentIterator it(doc.d, 0);
    while (it.hasNext()) {
        const QChar ch = doc.readCharacter(it.position() + 1);
        QCOMPARE(it.next(), ch);
    }
}

void tst_TextDocument::modified()
{
    TextDocument doc;
    QCOMPARE(doc.isModified(), false);
    doc.setText("foo bar");
    TextCursor cursor(&doc);
    QCOMPARE(doc.isModified(), false);
    cursor.insertText("1");
    QCOMPARE(doc.isModified(), true);
    doc.setModified(false);
    QCOMPARE(doc.isModified(), false);
    cursor.deletePreviousChar();
    QCOMPARE(doc.isModified(), true);
    QCOMPARE(doc.isUndoAvailable(), true);
    doc.undo();
    QCOMPARE(doc.isModified(), false);
    doc.redo();
    QCOMPARE(doc.isModified(), true);
    doc.setModified(false);
    QCOMPARE(doc.isModified(), false);
    doc.undo();
    QCOMPARE(doc.isModified(), true);
}

void tst_TextDocument::unicode()
{


}

void tst_TextDocument::lineNumbers()
{
    TextDocument doc;
    QCOMPARE(doc.lineNumber(0), 0); // Shouldn't assert with empty doc
    doc.setChunkSize(16);
    QString line = "f\n";
    for (int i=0; i<1024; ++i) {
        doc.append(line);
    }
    QCOMPARE(8, doc.read(0, 16).count(QLatin1Char('\n')));
    for (int i=0; i<1024; ++i) {
        QCOMPARE(doc.lineNumber(i * 2), i); // 1-indexed
    }
}


void tst_TextDocument::carriageReturns_data()
{
    QTest::addColumn<TextDocument::Options>("options");
    QTest::addColumn<TextDocument::DeviceMode>("deviceMode");
    QTest::addColumn<QByteArray>("text");
    QTest::addColumn<bool>("containsCarriageReturns");

    QTest::newRow("1") << TextDocument::Options(TextDocument::NoOptions) << TextDocument::Sparse << QByteArray("foo\nbar") << false;
    QTest::newRow("2") << TextDocument::Options(TextDocument::NoOptions) << TextDocument::LoadAll << QByteArray("foo\nbar") << false;
    QTest::newRow("3") << TextDocument::Options(TextDocument::NoOptions) << TextDocument::Sparse << QByteArray("foo\r\nbar") << true;
    QTest::newRow("4") << TextDocument::Options(TextDocument::NoOptions) << TextDocument::LoadAll << QByteArray("foo\r\nbar") << true;
    QTest::newRow("5") << TextDocument::Options(TextDocument::NoOptions) << TextDocument::Sparse << QByteArray("foo\r\nbar") << true;
    QTest::newRow("6") << TextDocument::Options(TextDocument::NoOptions) << TextDocument::LoadAll << QByteArray("foo\r\nbar") << true;
    QTest::newRow("7") << TextDocument::Options(TextDocument::DefaultOptions) << TextDocument::Sparse << QByteArray("foo\r\nbar") << true;
    QTest::newRow("8") << TextDocument::Options(TextDocument::DefaultOptions) << TextDocument::LoadAll << QByteArray("foo\r\nbar") << false;
    QTest::newRow("9") << TextDocument::Options(TextDocument::ConvertCarriageReturns) << TextDocument::LoadAll << QByteArray("foo\r\nbar") << false;
}

void tst_TextDocument::carriageReturns()
{
    QFETCH(TextDocument::Options, options);
    QFETCH(TextDocument::DeviceMode, deviceMode);
    QFETCH(QByteArray, text);
    QFETCH(bool, containsCarriageReturns);

    TextDocument doc;
    doc.setOptions(options|TextDocument::NoImplicitLoadAll);
    QBuffer buffer;
    buffer.setData(text);
    buffer.open(QIODevice::ReadOnly);
    doc.load(&buffer, deviceMode);
    QCOMPARE(doc.find(QLatin1Char('\r')).isValid(), containsCarriageReturns);
}

static inline int count(const QString &string, int from, int size, const QChar &ch)
{
    Q_ASSERT(from + size <= string.size());
    const ushort needle = ch.unicode();
    const ushort *haystack = string.utf16() + from;
    int num = 0;
    for (int i=0; i<size; ++i) {
        if (*haystack++ == needle)
            ++num;
    }
//    Q_ASSERT(string.mid(from, size).count(ch) == num);
    return num;
}

void tst_TextDocument::lineNumbersGenerated_data()
{
    QTest::addColumn<int>("chunkSize");
    QTest::addColumn<int>("seed");
    QTest::addColumn<int>("size");
    QTest::addColumn<int>("tests");
    QTest::newRow("a1") << 1 << 100 << 1000 << 100;
    QTest::newRow("a10") << 10 << 100 << 1000 << 100;
    QTest::newRow("a100") << 100 << 100 << 1000 << 100;
    QTest::newRow("a1000") << 1000 << 100 << 1000 << 100;
}

void tst_TextDocument::lineNumbersGenerated()
{
    QFETCH(int, chunkSize);
    QFETCH(int, seed);
    QFETCH(int, size);
    QFETCH(int, tests);
    QVERIFY(true);
    return;

    TextDocument doc;
    doc.setChunkSize(chunkSize);
    srand(seed);
    QString string;
    string.fill('x', size);
    for (int i=0; i<size; ++i) {
        switch (rand() % 20) {
        case 0: string[i] = QLatin1Char('\n'); break;
        case 1:
        case 2:
        case 3: string[i] = QLatin1Char(' '); break;
        default:
            continue;
        }
    }
    doc.setText(string);
    for (int i=0; i<tests; ++i) {
        const int idx = rand() % size;
        const int real = ::count(string, 0, idx, QLatin1Char('\n'));
        const int count = doc.lineNumber(idx);
        if (count != real) {
            qDebug("expected: %d got: %d", real, count);
        }
        QCOMPARE(real, count);
    }
}

class DocumentSubClass : public TextDocument
{
public:
    bool isWordCharacter(const QChar &ch, int) const
    {
        return !ch.isSpace();
    }

};

void tst_TextDocument::isWordOverride()
{
    QString text = "|This| |is| |a| |test|";
    {
        TextDocument doc;
        doc.setText(text);
        TextCursor cursor(&doc);
        QCOMPARE(cursor.wordUnderCursor(), QString());
        cursor.movePosition(TextCursor::NextWord, TextCursor::KeepAnchor);
        QCOMPARE(cursor.selectedText(), QString("|This"));
        cursor.movePosition(TextCursor::NextWord, TextCursor::KeepAnchor);
        QCOMPARE(cursor.selectedText(), QString("|This| |is"));
    }
    {
        DocumentSubClass doc;
        doc.setText(text);
        TextCursor cursor(&doc);
        QCOMPARE(cursor.wordUnderCursor(), QString("|This|"));
        cursor.movePosition(TextCursor::NextWord, TextCursor::KeepAnchor);
        QCOMPARE(cursor.selectedText(), QString("|This|"));
        cursor.movePosition(TextCursor::NextWord, TextCursor::KeepAnchor);
        QCOMPARE(cursor.selectedText(), QString("|This| |is|"));
    }
}

void tst_TextDocument::chunkBacktrack()
{
    TextDocument doc;
/*    QString s(doc.chunkSize()-2, 'x');
    s += "\nabcdefg\n";
    s = s + s + s + s + s;
    doc.setText(s);
    TextCursor c(&doc);
    c.setPosition(doc.documentSize());
    while (c.position() > 0) {
        c = doc.find('\n', c.position()-1, TextDocument::FindBackward);
        if (c.isNull()) {
            break;
        }
    }
*/
    QCOMPARE(doc.load("Script"), true);
    QString s("Sarah");

    int index = 0;
    TextCursor tc;
    while (!(tc = doc.find(s, index, 0)).isNull()) {
        printf("  Found at %d\n", tc.position());
        index = tc.position() + 1;

        TextCursor c(&doc, index);
        int row = c.lineNumber()-1;
        int col = c.columnNumber();
        int otherindex = convertRowColumnToIndex(&doc, row, col);
        Q_ASSERT(index == otherindex);
    }
}

int tst_TextDocument::convertRowColumnToIndex(const TextDocument *doc, int row, int column)
{
    // NOTE: this function works, but is slow. Quantify points
    // the finger at the underlying lazy document counting lines.
    // The line-numbering cache in there helps, but it's still
    // much slower than motif.  Needs tuned.
    if (row < 0) {
        printf("Requested row index %d must be greater than 0\n", row);
        return -1;
    }
    if (column < 0) {
        printf("Requested column index %d must be greater than 0\n", row);
        return -1;
    }


    int max = doc->documentSize();

    // The Qt widget seems to start row numbering at 1, not 0...
    int docrow = row+1;

    // This is a bit lame.  Keep skipping through until we hit it.
    int index = -1;
    TextCursor tc(doc);
    tc.setPosition(0);

    if (getenv("QT_LOOKUP")) {
        // TODO: I started to look at this, since it seems like a cleaner way
        // to do things, but it ends up being noticably slower (with 10 operations
        // in the test I was creating taking about 10 seconds, versus 4 with the code below).
        // Needs further investigation, but this is really what we should be doing,
        // (or rather, what TextCursor should be doing).

        // Get to the right row
        for (int i=0; i < docrow; ++i) {
            if (tc.position() >= max) {
                printf("Requested line number %d is out of range - the document has %d lines\n", row, i);
                return -1;
            }
            tc.movePosition(TextCursor::NextBlock);
        }

        // At this stage, we're in the right row.  We should also be at the beginning,
        // since we advanced from the start.

        // Cache the number of columns in this row in case we need to print an error message
        int rowStart = tc.position();
        tc.movePosition(TextCursor::EndOfLine, TextCursor::KeepAnchor);
        int actualColumns = tc.selectionSize();
        if (column > actualColumns) {
            printf("Requested column number %d is out of range - line %d has %d columns\n",
                   column, row, actualColumns);
            return -1;
        }

        return rowStart + column;
    } else {

        int inc = 100;
        while (index < 0) {
            int pos     = tc.position();
            int thisrow = tc.lineNumber();

            if (thisrow >= docrow) {
                // If we passed the row we want - back up
                while (tc.position() && tc.lineNumber() > docrow) {
                    tc.movePosition(TextCursor::PreviousBlock);
                }
                // Now we're in the right row.
                // First, determine whether we need to go back or forwards
                int incr = (tc.columnNumber() > column ? -1 : 1);
                while (tc.columnNumber() != column) {
                    tc.setPosition(tc.position() + incr);
                    if (tc.position() <=0
                        || tc.position() >= max
                        || tc.lineNumber() != docrow) {
                        // Make sure we didn't 'wrap around' to another row.
                        printf("Requested column number %d is out of range for line %d\n",
                               column, row);
                        return -1;
                    }
                }
                index = tc.position() - (tc.columnNumber() - column);
                break;
            } else {
                // Advance, but make sure we don't shoot off the end of the document
                int incr = (pos + inc < max ? inc : max - pos);
                if (incr) {
                    tc.setPosition(pos + incr);
                } else {
                    // We already hit the end of the document
                    printf("Requested line number %d is out of range - the document has %d lines\n", row, tc.lineNumber()-1);
                    break;
                }
            }
        }
    }

    return index;
}


void tst_TextDocument::insertText()
{
    TextDocument d;
    d.setText("This is a short\nlittle\ndocument\nwith\nline\nnumbers\n");
    // Calling 'lineNumber' initializes TextDocumentPrivate::hasChunksWithLineNumbers
    // and Chunk::firstLineIndex, but not Chunk::lines, leading to an abort
    // in TextDocument::insert.  It asserts that 'lines' has been set just before
    // it attempts to increment Chunk::lines by the number of newlines in the text
    // being added.
    d.lineNumber(4);
    TextCursor c = TextCursor(&d);
    c.setPosition(20);
    c.insertText("text\nwith\nnewlines\n");

    d.clear();
    const int blockSize = 3;
    QString firstBlock  = QString(blockSize, '1');
    QString secondBlock = QString(blockSize, '_');
    QString thirdBlock  = QString(blockSize, '^');
    c = TextCursor(&d);
    c.insertText(firstBlock);
    c.movePosition(TextCursor::End);
    QCOMPARE(c.position(), blockSize);
    c.insertText(secondBlock);
    c.movePosition(TextCursor::End);
    QCOMPARE(c.position(), blockSize*2);
    c.insertText(thirdBlock);

    QString out = d.read(0, blockSize*3);
    QString expected = firstBlock + secondBlock + thirdBlock;
    QCOMPARE(out, expected);
    if (out != expected) {
        printf("Generated output string was: %s\n"      \
               "Expected result was        : %s\n",
               qPrintable(out), qPrintable(expected));
    }

    QCOMPARE(d.load("Script"), true);

    TextCursor c2(&d);
    c2.movePosition(TextCursor::End);
    c2.insertText("\n");

    c2.setPosition(d.documentSize());
    c2.movePosition(TextCursor::PreviousBlock);
}

void printStatistics(const TextDocument *doc)
{
    const int chunkCount = doc->chunkCount();
    const int instantiatedChunkCount = doc->instantiatedChunkCount();

    printf("Memory usage: %d bytes, spread across %d chunks(%d instantiated, %d swapped)\n",
           doc->currentMemoryUsage(),
           chunkCount,
           instantiatedChunkCount,
           chunkCount - instantiatedChunkCount);
}

void tst_TextDocument::memUsage()
{
    TextDocument d;
    d.setOptions(TextDocument::SwapChunks);
    QString s = "%1  +------------------------------------------------------+\n";
    const int iterations = 1000000;

    printStatistics(&d);

    for (int i=0; i<iterations; ++i) {
        if (i%10000==0) {
            printf("%d%% - %d/%d: ", (int)(i ? ((double)i/iterations)*100 : 0), i, iterations);
            printStatistics(&d);
        }
        d.append(s.arg(i));
    }
    printStatistics(&d);

}

void tst_TextDocument::textDocmentIteratorOnDocumentSize()
{
    QByteArray data;
    for (int i=0; i<26; ++i) {
        data.append('a' + i);
    }
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    TextDocument doc;
    doc.setChunkSize(10);
    QVERIFY(doc.load(&buffer));
    TextCursor cursor(&doc);
    cursor.movePosition(TextCursor::End);
    cursor.insertText("\n");
    cursor.setPosition(doc.documentSize());
    cursor.movePosition(TextCursor::PreviousBlock);
}

QTEST_MAIN(tst_TextDocument)
#include "tst_textdocument.moc"
