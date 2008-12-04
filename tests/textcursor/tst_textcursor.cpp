/****************************************************************************
**
** Copyright (C) 1992-$THISYEAR$ Trolltech AS. All rights reserved.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <QtTest/QtTest>

#include <textcursor.h>
#include <textdocument.h>
QT_FORWARD_DECLARE_CLASS(TextCursor)

//TESTED_CLASS=
//TESTED_FILES=

class tst_TextCursor : public QObject
{
    Q_OBJECT

public:
    tst_TextCursor();
    virtual ~tst_TextCursor();

private slots:
    void setPosition();
    void setPosition_data();
//     void movePosition_data();
//     void movePosition();
};

tst_TextCursor::tst_TextCursor()
{
}

tst_TextCursor::~tst_TextCursor()
{
}

void tst_TextCursor::setPosition_data()
{
    QTest::addColumn<int>("one");
    QTest::addColumn<int>("two");
    QTest::addColumn<int>("moveMode");
    QTest::addColumn<int>("expectedPos");
    QTest::addColumn<int>("expectedAnchor");
    QTest::addColumn<QString>("selectedText");

    QTest::newRow("1") << 0 << 1 << int(TextCursor::MoveAnchor) << 1 << 1 << QString();
    QTest::newRow("2") << 0 << 2 << int(TextCursor::KeepAnchor) << 2 << 0 << QString("Fi");
    QTest::newRow("3") << 5 << 0 << int(TextCursor::KeepAnchor) << 0 << 5 << QString("First");
}

void tst_TextCursor::setPosition()
{
    static QString text("First1\n"
                        "Second2\n"
                        "Third333\n"
                        "Fourth4444\n"
                        "Fifth55555\n");
    TextDocument document;
    document.setText(text);

    TextCursor cursor(&document);
    QFETCH(int, one);
    QFETCH(int, two);
    QFETCH(int, moveMode);
    QFETCH(int, expectedPos);
    QFETCH(int, expectedAnchor);
    QFETCH(QString, selectedText);

    cursor.setPosition(one);
    cursor.setPosition(two, static_cast<TextCursor::MoveMode>(moveMode));
    QCOMPARE(expectedPos, cursor.position());
    QCOMPARE(expectedAnchor, cursor.anchor());
    QCOMPARE(selectedText, cursor.selectedText());
}

// void tst_TextCursor::movePosition_data()
// {
//     QTest::addColumn<int>("position");
//     QTest::addColumn<int>("anchor");
//     QTest::addColumn<int>("moveOperation");
//     QTest::addColumn<int>("expectedPosition");

//     QTest::newRow("Start") << 20 << 20 << TextCursor::Start << TextCursor::MoveAnchor << 0;
//     QTest::newRow("Up") << 25 << 25 << TextCursor::Up << TextCursor::MoveAnchor << 1;
//     QTest::newRow("StartOfLine") << 25 << 25 << TextCursor::StartOfLine << TextCursor::MoveAnchor << 25;
//     QTest::newRow("StartOfLine2") << 35 << 35 << TextCursor::StartOfLine << TextCursor::MoveAnchor << 25;
//     QTest::newRow("StartOfBlock") << 85 << 85 << TextCursor::StartOfBlock << TextCursor::MoveAnchor << 24;
//     QTest::newRow("StartOfWord") << 28 << 28 << TextCursor::StartOfWord << TextCursor::MoveAnchor << 24;
//     QTest::newRow("PreviousBlock") << 89 << 89 << TextCursor::PreviousBlock << TextCursor::MoveAnchor << 24;
//     QTest::newRow("PreviousCharacter") << 0 << 0 << TextCursor::PreviousCharacter << TextCursor::MoveAnchor << 0;
//     QTest::newRow("PreviousWord") << 0 << 0 << TextCursor::PreviousWord << TextCursor::MoveAnchor << 0;
//     QTest::newRow("Left") << 0 << 0 << TextCursor::Left << TextCursor::MoveAnchor << 0;
//     QTest::newRow("WordLeft") << 0 << 0 << TextCursor::WordLeft << TextCursor::MoveAnchor << 0;
//     QTest::newRow("End") << 0 << 0 << TextCursor::End << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("Down") << 0 << 0 << TextCursor::Down << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("EndOfLine") << 0 << 0 << TextCursor::EndOfLine << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("EndOfWord") << 0 << 0 << TextCursor::EndOfWord << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("EndOfBlock") << 0 << 0 << TextCursor::EndOfBlock << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("NextBlock") << 0 << 0 << TextCursor::NextBlock << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("NextCharacter") << 0 << 0 << TextCursor::NextCharacter << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("NextWord") << 0 << 0 << TextCursor::NextWord << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("Right") << 0 << 0 << TextCursor::Right << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("WordRight") << 0 << 0 << TextCursor::WordRight << TextCursor::MoveAnchor << 0 << 0;
// }

// void tst_TextCursor::movePosition()
// {
//     static const char *trueToLife = "I can only sing it loud\n" // 0 - 23
//                                     "always try to sing it clear what the hell are we all doing here\n" // 24 - 87
//                                     "making too much of nothing\n" // 88 - 114
//                                     "or creating one unholy mess an unfair study in survival, I guess\n"; // 115 - 179
//     TextDocument document;
//     document.setText(QString::fromLatin1(trueToLife));

//     TextCursor cursor(&document);
//     QFETCH(int, one);
//     QFETCH(int, two);
//     QFETCH(TextCursor::MoveMode, moveMode);
//     QFETCH(int, expectedPos);
//     QFETCH(int, expectedAnchor);
//     QFETCH(QString, selectedText);

//     cursor.setPosition(one);
//     cursor.setPosition(two, moveMode);
//     QCOMPARE(expectedPos, cursor.position());
//     QCOMPARE(expectedAnchor, cursor.anchor());
//     QCOMPARE(selectedText, cursor.selectedText());
// }


Q_DECLARE_METATYPE(TextCursor::MoveOperation);
Q_DECLARE_METATYPE(TextCursor::MoveMode);

QTEST_MAIN(tst_TextCursor)
#include "tst_textcursor.moc"
