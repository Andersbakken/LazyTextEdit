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
#include <syntaxhighlighter.h>
QT_FORWARD_DECLARE_CLASS(SyntaxHighlighter)

//TESTED_CLASS=
//TESTED_FILES=

class tst_SyntaxHighlighter : public QObject
{
    Q_OBJECT

public:
    tst_SyntaxHighlighter();
    virtual ~tst_SyntaxHighlighter();

private slots:
    void sanityCheck();
private:
};

tst_SyntaxHighlighter::tst_SyntaxHighlighter()
{
}

tst_SyntaxHighlighter::~tst_SyntaxHighlighter()
{
}

class Highlighter : public SyntaxHighlighter
{
public:
    Highlighter(TextEdit *parent = 0)
        : SyntaxHighlighter(parent)
    {}
    void highlightBlock(const QString &string)
    {
        highlighted = string;
    }
    QString highlighted;
};

void tst_SyntaxHighlighter::sanityCheck()
{
    delete new Highlighter; // make sure null parent doesn't crash
    {
        TextEdit edit;
        SyntaxHighlighter *s = new Highlighter(&edit);
        QVERIFY(s->textEdit() == &edit);
        QCOMPARE(edit.syntaxHighlighters().size(), 1);
        QVERIFY(edit.syntaxHighlighters().first() == s);
        QVERIFY(edit.syntaxHighlighter() == s);
        delete s;
        QCOMPARE(edit.syntaxHighlighters().size(), 0);
    }
    {
        QPointer<SyntaxHighlighter> ptr;
        TextEdit *textEdit = new TextEdit;
        ptr = new Highlighter(textEdit);
        delete textEdit;
        QVERIFY(!ptr);
    }
    {
        QPointer<SyntaxHighlighter> ptr;
        TextEdit *textEdit = new TextEdit;
        ptr = new Highlighter;
        textEdit->addSyntaxHighlighter(ptr);
        delete textEdit;
        QVERIFY(ptr);
        delete ptr;
    }
    {
        TextEdit edit;
        QPointer<SyntaxHighlighter> s = new Highlighter;
        edit.setSyntaxHighlighter(s);
        QCOMPARE(edit.syntaxHighlighters().size(), 1);
        QVERIFY(edit.syntaxHighlighters().first() == s);
        QVERIFY(edit.syntaxHighlighter() == s);
        edit.setSyntaxHighlighter(s);
        QCOMPARE(edit.syntaxHighlighters().size(), 1);
        QVERIFY(edit.syntaxHighlighters().first() == s);
        QVERIFY(edit.syntaxHighlighter() == s);
        SyntaxHighlighter *s2 = new Highlighter;
        edit.setSyntaxHighlighter(s2);
        QCOMPARE(edit.syntaxHighlighters().size(), 1);
        QVERIFY(edit.syntaxHighlighters().first() == s2);
        QVERIFY(edit.syntaxHighlighter() == s2);
        QVERIFY(!s->textEdit());
    }

    {
        TextEdit edit;
        QList<SyntaxHighlighter*> list;
        for (int i=0; i<1024; ++i) {
            list.append(new Highlighter(&edit));
        }
        QCOMPARE(list, edit.syntaxHighlighters());
    }

}

QTEST_MAIN(tst_SyntaxHighlighter)
#include "tst_syntaxhighlighter.moc"
