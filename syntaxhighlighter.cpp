// Copyright 2009 Anders Bakken
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

#include "syntaxhighlighter.h"
#include "textedit.h"
#include "textdocument.h"
#include "textlayout_p.h"
#include "textdocument_p.h"

SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
    : QObject(parent), d(new Private)
{
}

SyntaxHighlighter::SyntaxHighlighter(TextEdit *parent)
    : QObject(parent), d(new Private)
{
    if (parent) {
        parent->setSyntaxHighlighter(this);
    }
}

SyntaxHighlighter::~SyntaxHighlighter()
{
    delete d;
}

void SyntaxHighlighter::setTextEdit(TextEdit *doc)
{
    if (d->textEdit)
        d->textEdit->setSyntaxHighlighter(0);
    d->textEdit = doc;
    if (d->textEdit)
        d->textEdit->setSyntaxHighlighter(this);
}
TextEdit *SyntaxHighlighter::textEdit() const
{
    return d->textEdit;
}


TextDocument * SyntaxHighlighter::document() const
{
    return d->textEdit ? d->textEdit->document() : 0;
}

void SyntaxHighlighter::rehighlight()
{
    if (d->textEdit) {
        Q_ASSERT(d->textLayout);
        d->textLayout->dirty(d->textEdit->viewport()->width());
    }
}

void SyntaxHighlighter::setFormat(int start, int count, const QTextCharFormat &format)
{
    ASSUME(d->textEdit);
    Q_ASSERT(start >= 0);
    Q_ASSERT(start + count <= d->currentBlock.size());
    d->formatRanges.append(QTextLayout::FormatRange());
    QTextLayout::FormatRange &range = d->formatRanges.last();
    range.start = start;
    range.length = count;
    range.format = format;
}

void SyntaxHighlighter::setFormat(int start, int count, const QColor &color)
{
    QTextCharFormat format;
    format.setForeground(color);
    setFormat(start, count, format);
}

void SyntaxHighlighter::setFormat(int start, int count, const QFont &font)
{
    QTextCharFormat format;
    format.setFont(font);
    setFormat(start, count, format);
}

QTextCharFormat SyntaxHighlighter::format(int pos) const
{
    QTextCharFormat ret;
    foreach(const QTextLayout::FormatRange &range, d->formatRanges) {
        if (range.start <= pos && range.start + range.length > pos) {
            ret.merge(range.format);
        } else if (range.start > pos) {
            break;
        }
    }
    return ret;
}


int SyntaxHighlighter::previousBlockState() const
{
    return d->previousBlockState;
}

int SyntaxHighlighter::currentBlockState() const
{
    return d->currentBlockState;
}

void SyntaxHighlighter::setCurrentBlockState(int s)
{
    d->previousBlockState = d->currentBlockState;
    d->currentBlockState = s; // ### These don't entirely follow QSyntaxHighlighter's behavior
}

int SyntaxHighlighter::currentBlockPosition() const
{
    return d->currentBlockPosition;
}

void SyntaxHighlighter::setBlockFormat(const QTextBlockFormat &format)
{
    d->blockFormat = format;
}
