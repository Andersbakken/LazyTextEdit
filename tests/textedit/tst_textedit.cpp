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
    qDebug() << edit.cursorPosition() << edit.verticalScrollBar()->value();
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
    qDebug() << edit.cursorPosition() << edit.verticalScrollBar()->value();
//     QEventLoop loop;
//     loop.exec();
    QTest::mouseClick(edit.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QVERIFY(edit.verticalScrollBar()->value() == 0);

}

QTEST_MAIN(tst_TextEdit)
#include "tst_textedit.moc"
