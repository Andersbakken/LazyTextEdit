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

#ifndef SYNTAXHIGHLIGHTER_H
#define SYNTAXHIGHLIGHTER_H

#include <QObject>
#include <QString>
#include <QTextCharFormat>
#include <QColor>
#include <QFont>
#include <QList>
#include <QTextLayout>

class TextEdit;
class TextLayout;
class TextDocument;
class SyntaxHighlighter : public QObject
{
    Q_OBJECT
public:
    SyntaxHighlighter(QObject *parent = 0);
    SyntaxHighlighter(TextEdit *parent);
    ~SyntaxHighlighter();
    void setTextEdit(TextEdit *doc);
    TextEdit *textEdit() const;
    TextDocument *document() const;
public Q_SLOTS:
    void rehighlight();
protected:
    virtual void highlightBlock(const QString &text) = 0;
    QString currentBlock() const { return d->currentBlock; }
    void setFormat(int start, int count, const QTextCharFormat &format);
    void setFormat(int start, int count, const QColor &color);
    inline void setColor(int start, int count, const QColor &color)
    { setFormat(start, count, color); }
    void setFormat(int start, int count, const QFont &font);
    inline void setFont(int start, int count, const QFont &font)
    { setFormat(start, count, font); }

    QTextBlockFormat blockFormat() const;
    void setBlockFormat(const QTextBlockFormat &format);
    QTextCharFormat format(int pos) const;
    int previousBlockState() const;
    int currentBlockState() const;
    void setCurrentBlockState(int s);
    int currentBlockPosition() const;
private:
    struct Private {
        Private() : textEdit(0), textLayout(0), previousBlockState(0), currentBlockState(0),
                    currentBlockPosition(-1) {}
        TextEdit *textEdit;
        TextLayout *textLayout;
        int previousBlockState, currentBlockState, currentBlockPosition;
        QList<QTextLayout::FormatRange> formatRanges;
        QTextBlockFormat blockFormat;
        QString currentBlock;
    } *d;

    friend class TextEdit;
    friend class TextLayout;
};

#endif
