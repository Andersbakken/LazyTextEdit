#include "syntaxhighlighter.h"
#include "textedit.h"
#include "textlayout_p.h"

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

void SyntaxHighlighter::rehighlight()
{
    if (d->textEdit) {
        Q_ASSERT(d->textLayout);
        d->textLayout->dirty(d->textEdit->viewport()->width());
    }
}

void SyntaxHighlighter::setFormat(int start, int count, const QTextCharFormat &format)
{
    Q_ASSERT(d->textEdit);
    d->formatRanges.append(QTextLayout::FormatRange());
    QTextLayout::FormatRange &range = d->formatRanges.last();
    range.start = d->currentBlockOffset + start;
    range.length = count;
    range.format = format;
    d->hasBackground |= format.hasProperty(QTextCharFormat::BackgroundBrush);
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


QString SyntaxHighlighter::currentBlockText() const
{
    return QString();
//    return d->textEdit && d->currentBlockIndex != -1 ? d->textEdit->blockText(d->currentBlockIndex)
}

int SyntaxHighlighter::currentBlockPosition() const
{
    return d->currentBlockPosition;
}

