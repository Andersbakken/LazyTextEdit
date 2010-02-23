// Copyright 2010 Anders Bakken
//
// Licensed under the Apache License, Version 2.0 (the "License");
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
    void operatorEquals_data();
    void operatorEquals();
    void setPosition();
    void setPosition_data();
    void movePosition_data();
    void movePosition();
private:
    TextDocument document;
};

tst_TextCursor::tst_TextCursor()
{
    document.append("This is some text");
}

tst_TextCursor::~tst_TextCursor()
{
}

void tst_TextCursor::setPosition_data()
{
    QTest::addColumn<int>("one");
    QTest::addColumn<int>("two");
    QTest::addColumn<TextCursor::MoveMode>("moveMode");
    QTest::addColumn<int>("expectedPos");
    QTest::addColumn<int>("expectedAnchor");
    QTest::addColumn<QString>("selectedText");

    QTest::newRow("1") << 0 << 1 << TextCursor::MoveAnchor << 1 << 1 << QString();
    QTest::newRow("2") << 0 << 2 << TextCursor::KeepAnchor << 2 << 0 << QString("Fi");
    QTest::newRow("3") << 5 << 0 << TextCursor::KeepAnchor << 0 << 5 << QString("First");
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
    QFETCH(TextCursor::MoveMode, moveMode);
    QFETCH(int, expectedPos);
    QFETCH(int, expectedAnchor);
    QFETCH(QString, selectedText);

    cursor.setPosition(one);
    cursor.setPosition(two, moveMode);
    QCOMPARE(expectedPos, cursor.position());
    QCOMPARE(expectedAnchor, cursor.anchor());
    QCOMPARE(selectedText, cursor.selectedText());
}

void tst_TextCursor::movePosition_data()
{
    QTest::addColumn<int>("initialPosition");
    QTest::addColumn<int>("initialAnchor");
    QTest::addColumn<TextCursor::MoveOperation>("moveOperation");
    QTest::addColumn<TextCursor::MoveMode>("moveMode");
    QTest::addColumn<int>("expectedPosition");
    QTest::addColumn<int>("expectedAnchor");
    QTest::addColumn<QString>("selectedText");

    QTest::newRow("Start") << 20 << 20 << TextCursor::Start << TextCursor::MoveAnchor
                           << 0 << 0 << QString();
    QTest::newRow("Up") << 25 << 25 << TextCursor::Up
                        << TextCursor::MoveAnchor << 1 << 1 << QString();
    QTest::newRow("StartOfLine") << 24 << 25 << TextCursor::StartOfLine
                                 << TextCursor::MoveAnchor << 24 << 24 << QString();
    QTest::newRow("StartOfLine2") << 35 << 35 << TextCursor::StartOfLine
                                  << TextCursor::MoveAnchor << 24 << 24 << QString();
    QTest::newRow("StartOfBlock") << 85 << 85 << TextCursor::StartOfBlock
                                  << TextCursor::MoveAnchor << 24 << 24 << QString();
    QTest::newRow("StartOfWord") << 28 << 28 << TextCursor::StartOfWord
                                 << TextCursor::MoveAnchor << 24 << 24 << QString();
    QTest::newRow("PreviousBlock") << 89 << 89 << TextCursor::PreviousBlock
                                   << TextCursor::MoveAnchor << 24 << 24 << QString();
    QTest::newRow("PreviousCharacter") << 0 << 0 << TextCursor::PreviousCharacter
                                       << TextCursor::MoveAnchor << 0 << 0 << QString();
    QTest::newRow("PreviousWord") << 0 << 0 << TextCursor::PreviousWord
                                  << TextCursor::MoveAnchor << 0 << 0 << QString();
    QTest::newRow("Left") << 0 << 0 << TextCursor::Left
                          << TextCursor::MoveAnchor << 0 << 0 << QString();
    QTest::newRow("WordLeft") << 0 << 0 << TextCursor::WordLeft
                              << TextCursor::MoveAnchor << 0 << 0 << QString();
    QTest::newRow("FirstCharacter") << 0 << 0 << TextCursor::NextCharacter
                                    << TextCursor::KeepAnchor << 1 << 0 << "I";

    QTest::newRow("FirstCharacterReverse") << 1 << 1 << TextCursor::PreviousCharacter
                                           << TextCursor::KeepAnchor << 0 << 1 << "I";
    QTest::newRow("LastCharacter") << 179 << 179 << TextCursor::NextCharacter
                                   << TextCursor::KeepAnchor << 180 << 179 << "\n";
    QTest::newRow("LastCharacterReverse") << 180 << 180 << TextCursor::PreviousCharacter
                                          << TextCursor::KeepAnchor << 179 << 180 << "\n";

    QTest::newRow("EmptyBlock") << 180 << 180 << TextCursor::NextBlock
                                << TextCursor::KeepAnchor << 181 << 180 << "\n";

    QTest::newRow("EmptyBlockReverse") << 181 << 181 << TextCursor::PreviousBlock
                                       << TextCursor::KeepAnchor << 180 << 181 << "\n";

//     QTest::newRow("End") << 0 << 0 << TextCursor::End
//                          << TextCursor::MoveAnchor << 0 << 0 << QString();
//     QTest::newRow("Down") << 0 << 0 << TextCursor::Down
//                           << TextCursor::MoveAnchor << 0 << 0 << QString();
//     QTest::newRow("EndOfLine") << 0 << 0 << TextCursor::EndOfLine
//                                << TextCursor::MoveAnchor << 0 << 0 << QString();
//     QTest::newRow("EndOfWord") << 0 << 0 << TextCursor::EndOfWord << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("EndOfBlock") << 0 << 0 << TextCursor::EndOfBlock << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("NextBlock") << 0 << 0 << TextCursor::NextBlock << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("NextCharacter") << 0 << 0 << TextCursor::NextCharacter << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("NextWord") << 0 << 0 << TextCursor::NextWord << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("Right") << 0 << 0 << TextCursor::Right << TextCursor::MoveAnchor << 0 << 0;
//     QTest::newRow("WordRight") << 0 << 0 << TextCursor::WordRight << TextCursor::MoveAnchor << 0 << 0;
}

void tst_TextCursor::movePosition()
{
    static const char *trueToLife = "I can only sing it loud\n" // 0 - 23
                                    "always try to sing it clear what the hell are we all doing here\n" // 24 - 87
                                    "making too much of nothing\n" // 88 - 114
                                    "or creating one unholy mess an unfair study in survival, I guess\n" // 115 - 179
                                    "\nSomething\n";  // 180 - 190
    TextDocument document;
    document.setText(QString::fromLatin1(trueToLife));

    TextCursor cursor(&document);
    QFETCH(int, initialPosition);
    QFETCH(int, initialAnchor);
    QFETCH(TextCursor::MoveOperation, moveOperation);
    QFETCH(TextCursor::MoveMode, moveMode);
    QFETCH(int, expectedPosition);
    QFETCH(int, expectedAnchor);
    QFETCH(QString, selectedText);

    cursor.setPosition(initialAnchor);
    if (initialPosition != initialAnchor) {
        cursor.setPosition(initialPosition, TextCursor::KeepAnchor);
    }
    Q_ASSERT(cursor.position() == initialPosition);
    Q_ASSERT(cursor.anchor() == initialAnchor);
    cursor.movePosition(moveOperation, moveMode);
    QCOMPARE(cursor.position(), expectedPosition);
    QCOMPARE(cursor.anchor(), expectedAnchor);
    QCOMPARE(cursor.selectedText(), selectedText);
}

void tst_TextCursor::operatorEquals_data()
{
    QTest::addColumn<TextCursor*>("left");
    QTest::addColumn<TextCursor*>("right");
    QTest::addColumn<bool>("expected");

    QTest::newRow("null vs null") << new TextCursor << new TextCursor << true;
    QTest::newRow("non null vs null") << new TextCursor(&document) << new TextCursor << false;

    QTest::newRow("normal vs normal equal") << new TextCursor(&document) << new TextCursor(&document) << true;
    TextCursor *cursor = new TextCursor(&document);
    cursor->setPosition(1);
    QTest::newRow("normal vs normal different") << new TextCursor(&document) << cursor << false;

    cursor = new TextCursor(&document);
    cursor->setPosition(1, TextCursor::KeepAnchor);

    TextCursor *cursor2 = new TextCursor(&document);
    cursor2->setPosition(1, TextCursor::KeepAnchor);
    QTest::newRow("selection vs selection") << cursor << cursor2 << true;

    cursor = new TextCursor(&document);
    cursor->setPosition(1, TextCursor::KeepAnchor);

    cursor2 = new TextCursor(&document);
    cursor2->setPosition(1, TextCursor::MoveAnchor);
    QTest::newRow("selection vs selection different") << cursor << cursor2 << false;


}

void tst_TextCursor::operatorEquals()
{
    QFETCH(TextCursor*, left);
    QFETCH(TextCursor*, right);
    QFETCH(bool, expected);

    const bool equal = (*left == *right);
    const bool notEqual = (*left != *right);
    delete left;
    delete right;
    QVERIFY(equal != notEqual);
    QCOMPARE(equal, expected);
}


Q_DECLARE_METATYPE(TextCursor *);
Q_DECLARE_METATYPE(TextCursor::MoveOperation);
Q_DECLARE_METATYPE(TextCursor::MoveMode);

QTEST_MAIN(tst_TextCursor)
#include "tst_textcursor.moc"
