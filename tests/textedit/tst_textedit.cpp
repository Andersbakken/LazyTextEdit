/****************************************************************************
**
** Copyright (C) 1992-$THISYEAR$ Trolltech AS. All rights reserved.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <QtTest/QtTest>

#include <textedit.h>
#include <textdocument.h>
QT_FORWARD_DECLARE_CLASS(TextEdit)

//TESTED_CLASS=
//TESTED_FILES=

class tst_TextEdit : public QObject
{
    Q_OBJECT

public:
    tst_TextEdit();
    virtual ~tst_TextEdit();

private slots:
    void clickInReadOnlyEdit();
    void scrollReadOnly();
    void cursorForPosition();
    void columnNumberIssue();
};

tst_TextEdit::tst_TextEdit()
{
}

tst_TextEdit::~tst_TextEdit()
{
}

void tst_TextEdit::scrollReadOnly()
{
    TextEdit edit;
    QString line = "12434567890\n";
    QString text;
    for (int i=0; i<1000; ++i) {
        text.append(line);
    }
    edit.document()->setText(text);
    edit.setReadOnly(true);
    edit.resize(400, 400);
    edit.show();
    QTest::qWaitForWindowShown(&edit);
//     edit.setCursorPosition(0);
//     edit.verticalScrollBar()->setValue(0);
//    qDebug() << edit.cursorPosition() << edit.verticalScrollBar()->value();
//     QEventLoop loop;
//     loop.exec();
    QTest::keyClick(&edit, Qt::Key_Down);
    QVERIFY(edit.verticalScrollBar()->value() != 0);
}

void tst_TextEdit::clickInReadOnlyEdit()
{
    TextEdit edit;
    QString line = "12434567890\n";
    QString text;
    for (int i=0; i<1000; ++i) {
        text.append(line);
    }
    edit.document()->setText(text);
    edit.setReadOnly(true);
    edit.resize(400, 400);
    edit.show();
    QTest::qWaitForWindowShown(&edit);
//     edit.setCursorPosition(0);
//     edit.verticalScrollBar()->setValue(0);
//    qDebug() << edit.cursorPosition() << edit.verticalScrollBar()->value();
//     QEventLoop loop;
//     loop.exec();
    QTest::mouseClick(edit.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QVERIFY(edit.verticalScrollBar()->value() == 0);

}

void tst_TextEdit::cursorForPosition()
{
    TextEdit edit;
    const QString string = "123456";
    QSet<QChar> all;
    for (int i=0; i<all.size(); ++i) {
        all.insert(string.at(i));
    }
    edit.setText(string);
    QFont font;
    font.setPixelSize(10);
    edit.setFont(font);
    edit.resize(500, 500);
    const QRect r = edit.viewport()->rect();
    QPoint pos(0, 0);
    for (int x=r.left(); x<r.right(); ++x) {
        for (int y=r.top(); y<r.bottom(); ++y) {
            TextCursor cursor = edit.cursorForPosition(pos);
            if (!cursor.isValid())
                continue;
            QCOMPARE(cursor.position(), edit.textPositionAt(pos));
            const QChar ch = cursor.cursorCharacter();
            const QChar readChar = edit.readCharacter(cursor.position());
            QCOMPARE(readChar, ch);
            if (all.remove(ch) && all.isEmpty()) {
                goto end;
            }
        }
    }
end:
    QCOMPARE(all.size(), 0);
}

void tst_TextEdit::columnNumberIssue()
{
    TextEdit edit;
    edit.resize(400, 400);
    edit.show();
    const QString line = "This is a very very very long line and it should most certainly wrap around at least once.\n";

    QString text;
    enum { Lines = 500 };
    text.reserve(line.size() * Lines);
    for (int i=0; i<Lines; ++i) {
        text.append(line);
    }
    edit.setText(text);
    while (edit.cursorPosition() < edit.document()->documentSize() - line.size()) {
//         qDebug() << edit.cursorPosition()
//                  << edit.document()->documentSize();
        QTest::keyClick(&edit, Qt::Key_Down);
    }
    while (edit.cursorPosition() > text.size()) {
//        qDebug() << edit.cursorPosition();
        QTest::keyClick(&edit, Qt::Key_Down);
        QCOMPARE(0, edit.textCursor().columnNumber());
    }
//     QEventLoop loop;
//     loop.exec();
}

QTEST_MAIN(tst_TextEdit)
#include "tst_textedit.moc"
