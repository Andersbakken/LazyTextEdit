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

#include <textedit.h>
#include <textdocument.h>
QT_FORWARD_DECLARE_CLASS(TextEdit)

//TESTED_CLASS=
//TESTED_FILES=

// Need Qt 4.6.0> for qWaitForWindowShown
#if (QT_VERSION < QT_VERSION_CHECK(4, 6, 0))

#ifdef Q_WS_X11
extern void qt_x11_wait_for_window_manager(QWidget *w);
#endif

namespace QTest
{
    inline static bool qWaitForWindowShown(QWidget *window)
    {
#if defined(Q_WS_X11)
        qt_x11_wait_for_window_manager(window);
        QCoreApplication::processEvents();
#elif defined(Q_WS_QWS)
        Q_UNUSED(window);
        qWait(100);
#else
        Q_UNUSED(window);
        qWait(50);
#endif
        return true;
    }

}

QT_END_NAMESPACE

#endif // Qt < 4.6.0

class tst_TextEdit : public QObject
{
    Q_OBJECT

public:
    tst_TextEdit();
    virtual ~tst_TextEdit();

private slots:
    void clickInReadOnlyEdit();
    void scrollReadOnly();
    void scrollLines();
    void cursorForPosition();
    void columnNumberIssue();
    void selectionTest();
    void sectionTest();
    void deleteTest();
    void emptyDocumentTest();
    void changeDocumentTest();
    void clickInBlankAreaTest();
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

void tst_TextEdit::scrollLines()
{
    TextEdit edit;
    QString text = "Line 1\nLine2\nLine 3\n\nLine 5\n\nLine 7\nLine 8\n\nLine 10\n";
    int numLines = text.count('\n');
    edit.document()->setText(text);

    QList<int> lineSteps;
    lineSteps << 1 << 3; // Single-click, mouse wheel

    QList<int> directions;
    directions << 1 << -1; // Forwards, backwards

    QScrollBar *vsb = edit.verticalScrollBar();

    foreach(int lineStep, lineSteps) {
        int lineNo = 0;
        //printf("%d-line step\n", lineStep);
        foreach(int direction, directions) {
            //printf("  %s\n", direction>0?"Forwards":"Backwards");
            if (direction>0) {
                // Forwards
                vsb->setSliderPosition( 0 );
            }

            // Forwards
            while (lineNo <= numLines && lineNo >= 0) {
                int actualLineNo = edit.lineNumber(edit.viewportPosition());
                //printf("    Line %d\n", actualLineNo);
                if (lineNo != actualLineNo) {
                    printf("  %s: expected to be at line %d, actually at line %d\n",
                           (direction > 0 ? "Forwards" : "Backwards"), lineNo, actualLineNo);
                }
                QCOMPARE(lineNo, actualLineNo);
                for (int i=0; i<lineStep; lineNo+=direction, ++i) {
                    vsb->triggerAction(direction > 0
                                       ? QAbstractSlider::SliderSingleStepAdd
                                       : QAbstractSlider::SliderSingleStepSub);
                }
            }
            lineNo = numLines;
        }
    }
}

void tst_TextEdit::clickInReadOnlyEdit()
{
    TextEdit edit;
    QString line = "1234567890\n";
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
    QCOMPARE(1, 1);
    return;
    TextEdit edit;
    edit.resize(200, 200);
    edit.setText("This is a very very very long line and it should most certainly wrap around at least once.\n"
                 "This is a shorter line\n"
                 "1\n"
                 "2\n"
                 "3\n"
                 "4\n"
                 "5\n"
                 "6\n"
                 "7\n"
                 "8\n"
                 "9\n"
                 "10\n"
                 "11\n"
                 "12\n");
    edit.show();
    QTest::qWaitForWindowShown(&edit);
    QVERIFY(edit.verticalScrollBar()->maximum() != 0);
    while (edit.cursorPosition() < edit.document()->documentSize()) {
        QCOMPARE(0, edit.textCursor().columnNumber());
        QTest::keyClick(&edit, Qt::Key_Down);
    }
//    QVERIFY(edit.verticalScrollBar()->value() != 0);
    while (edit.cursorPosition() > 0) {
//        qDebug() << edit.cursorPosition();
        QTest::keyClick(&edit, Qt::Key_Up);
        QCOMPARE(0, edit.textCursor().columnNumber());
    }
//     QEventLoop loop;
//     loop.exec();
}

void tst_TextEdit::selectionTest()
{
    TextEdit edit;
    QString line = "1234567890\n";
    edit.document()->setText(line);
//    edit.setReadOnly(true);
    edit.resize(400, 400);
    edit.show();
    QTest::qWaitForWindowShown(&edit);

    QCOMPARE(edit.selectedText(), QString());
    QVERIFY(!edit.textCursor().hasSelection());

    QTest::keyClick(&edit, Qt::Key_Right, Qt::ShiftModifier);
    QVERIFY(edit.textCursor().hasSelection());
    QCOMPARE(edit.selectedText(), QString("1"));
    QCOMPARE(edit.textCursor().anchor(), 0);
    QCOMPARE(edit.textCursor().position(), 1);

    QTest::keyClick(&edit, Qt::Key_Right, Qt::ShiftModifier);
    QVERIFY(edit.textCursor().hasSelection());
    QCOMPARE(edit.selectedText(), QString("12"));
    QCOMPARE(edit.textCursor().anchor(), 0);
    QCOMPARE(edit.textCursor().position(), 2);

    QTest::keyClick(&edit, Qt::Key_Right, Qt::ShiftModifier);
    QVERIFY(edit.textCursor().hasSelection());
    QCOMPARE(edit.textCursor().selectedText(), QString("123"));
    QCOMPARE(edit.textCursor().anchor(), 0);
    QCOMPARE(edit.textCursor().position(), 3);

    QTest::keyClick(&edit, Qt::Key_Left, Qt::ShiftModifier);
    QVERIFY(edit.textCursor().hasSelection());
    QCOMPARE(edit.textCursor().selectedText(), QString("12"));
    QCOMPARE(edit.textCursor().anchor(), 0);
    QCOMPARE(edit.textCursor().position(), 2);

    QTest::keyClick(&edit, Qt::Key_Left, Qt::ShiftModifier);
    QVERIFY(edit.textCursor().hasSelection());
    QCOMPARE(edit.textCursor().selectedText(), QString("1"));
    QCOMPARE(edit.textCursor().anchor(), 0);
    QCOMPARE(edit.textCursor().position(), 1);

    QTest::keyClick(&edit, Qt::Key_Left, Qt::ShiftModifier);
    QVERIFY(!edit.textCursor().hasSelection());
    QCOMPARE(edit.textCursor().selectedText(), QString());
    QCOMPARE(edit.textCursor().anchor(), 0);
    QCOMPARE(edit.textCursor().position(), 0);
}

void tst_TextEdit::sectionTest()
{
    TextEdit edit;
    QString  text = "This is some\nrandom document string\nthing\n";
    edit.document()->setText(text);
    TextSection *ts = edit.insertTextSection(5, 10);
    edit.document()->takeTextSection(ts);
    QVERIFY( !edit.sections().contains(ts) );
}

void tst_TextEdit::deleteTest()
{
    TextDocument *doc = new TextDocument();
    doc->setText("Test to make sure that deleting a text edit with sections doesn't result in the sections being deleted again in the document destructor\n");
    TextEdit *edit = new TextEdit();
    edit->setDocument(doc);

    TextSection *s = edit->insertTextSection(4, 4);
    Q_UNUSED(s);
    delete edit;
    delete doc;
}

void tst_TextEdit::emptyDocumentTest()
{
    TextEdit edit;
    for(int i=0; i < 100 ; i++) {
	edit.append(QString("This is text on line %1\n").arg(i));
    }
    edit.show();
    QTest::qWaitForWindowShown(&edit);
    edit.setDocument(new TextDocument()); /// This causes the problem
    QTest::qWaitForWindowShown(&edit);
}

void tst_TextEdit::changeDocumentTest()
{
    TextDocument doc1;
    doc1.setText("abcdefg");

    TextDocument doc2;
    doc2.setText("hijklmn");

    TextEdit editor1;
    editor1.setDocument(&doc1);
    editor1.setDocument(&doc2);

    TextEdit editor2;
    editor2.setDocument(&doc1);

    TextSection *s = editor2.insertTextSection(0, 1);
    s->setCursor(Qt::PointingHandCursor);
}

void tst_TextEdit::clickInBlankAreaTest()
{
    TextEdit edit;
    edit.load("spectre.scs");
    // Make the window big enough to leave blank space below
    edit.setMinimumSize(QSize(1000, 1200));
    edit.show();
    QTest::qWaitForWindowShown(&edit);
    // Click below the visible text
    QTest::mouseClick(edit.viewport(), Qt::LeftButton, Qt::NoModifier,
		      QPoint(50, 1000));
}



QTEST_MAIN(tst_TextEdit)
#include "tst_textedit.moc"
