// Copyright 2010 Anders Bakken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "textcursor.h"
#include "textcursor_p.h"
#include "textdocument.h"
#include "textdocument_p.h"
#include "textedit_p.h"
#include "textlayout_p.h"
#include "textedit.h"

class SelectionChangedEmitter
{
public:
    SelectionChangedEmitter(TextEdit *t)
        : selectionStart(-1), selectionEnd(-1), textEdit(t)
    {
        if (textEdit) {
            selectionStart = textEdit->textCursor().selectionStart();
            selectionEnd = textEdit->textCursor().selectionEnd();
        }
    }

    ~SelectionChangedEmitter()
    {
        if (textEdit) {
            const TextCursor &cursor = textEdit->textCursor();
            if (((selectionStart != selectionEnd) != cursor.hasSelection())
                || (selectionStart != selectionEnd
                    && (selectionStart != cursor.selectionStart()
                        || selectionEnd != cursor.selectionEnd()))) {
                QMetaObject::invokeMethod(textEdit, "selectionChanged");
            }
        }
    }
private:
    int selectionStart, selectionEnd;
    TextEdit *textEdit;
};

TextCursor::TextCursor()
    : d(0), textEdit(0)
{
}

TextCursor::TextCursor(const TextDocument *document, int pos, int anc)
    : d(new TextCursorSharedPrivate), textEdit(0)
{
    d->document = const_cast<TextDocument*>(document);
    if (d->document) {
        d->document->d->textCursors.insert(d);
        d->position = pos;
        d->anchor = anc == -1 ? pos : anc;
    }
}

TextCursor::TextCursor(const TextEdit *edit, int pos, int anc)
    : d(new TextCursorSharedPrivate), textEdit(0) // ### is this nasty? Will people expect that this cursor is the edit's textCursor?
{
    if (edit) {
        d->document = edit->document();
        d->document->d->textCursors.insert(d);
        d->position = pos;
        d->anchor = anc == -1 ? pos : anc;
    }
}

TextCursor::TextCursor(const TextCursor &cursor)
    : d(cursor.d), textEdit(0)
{
    ref();
}

TextCursor &TextCursor::operator=(const TextCursor &other)
{
    deref();
    d = other.d;
    textEdit = 0;
    ref();
    return *this;
}

TextCursor::~TextCursor()
{
    deref();
}

bool TextCursor::isNull() const
{
    return !d || !d->document;
}

TextDocument *TextCursor::document() const
{
    return d ? d->document : 0;
}

void TextCursor::setSelection(int pos, int length) // can be negative
{
    setPosition(pos + length);
    setPosition(pos, KeepAnchor);
}

void TextCursor::setPosition(int pos, MoveMode mode)
{
    Q_ASSERT(!isNull());
    d->overrideColumn = -1;
    if (pos < 0 || pos > d->document->documentSize()) {
        clearSelection();
        return;
    } else if (pos == d->position && (mode == KeepAnchor || d->anchor == d->position)) {
        return;
    }

    SelectionChangedEmitter emitter(textEdit);
    detach();
    cursorChanged(false);

#if 0
    Link *link = d->document->links(pos, 1).value(0, 0);
    if (link && link->position < pos && link->position + link->size > pos) { // inside a link
        pos = (pos > d->position ? link->position + link->size : link->position);
    }
#endif

    if (mode == TextCursor::MoveAnchor) {
        clearSelection();
    }

    d->position = pos;
    if (mode == MoveAnchor) {
        d->anchor = pos;
    }
    cursorChanged(true);
}

int TextCursor::position() const
{
    return isNull() ? -1 : d->position;
}

int TextCursor::anchor() const
{
    return isNull() ? -1 : d->anchor;
}

void TextCursor::insertText(const QString &text)
{
    Q_ASSERT(!isNull());
    const bool doJoin = hasSelection();
    if (doJoin) {
        removeSelectedText();
    }
    const bool old = d->document->d->cursorCommand;
    d->document->d->cursorCommand = true;
    if (d->document->insert(d->position, text) && textEdit) {
        emit textEdit->cursorPositionChanged(d->position);
    }
    if (doJoin)
        d->document->d->joinLastTwoCommands();
    d->document->d->cursorCommand = old;
}

bool TextCursor::movePosition(TextCursor::MoveOperation op, TextCursor::MoveMode mode, int n)
{
    if (!d || !d->document || n <= 0)
        return false;

    switch (op) {
    case Start:
    case StartOfLine:
    case End:
    case EndOfLine:
        n = 1;
        break;
    default:
        break;
    }

    if (n > 1) {
        while (n > 0 && movePosition(op, mode, 1))
            --n;
        return n == 0;
    }
    detach();

    switch (op) {
    case NoMove:
        return true;

    case PreviousCharacter:
    case NextCharacter:
        return movePosition(op == TextCursor::PreviousCharacter
                            ? TextCursor::Left : TextCursor::Right, mode);

    case End:
    case Start:
        setPosition(op == TextCursor::Start ? 0 : d->document->documentSize(), mode);
        break;

    case Up:
    case Down: {
        TextLayout *textLayout = TextLayoutCacheManager::requestLayout(*this, 1); // should I use 1?
        Q_ASSERT(textLayout);
        int index;
        bool currentIsLast;
        const QTextLine currentLine = textLayout->lineForPosition(d->position, 0,
                                                                  &index,
                                                                  &currentIsLast);
        Q_ASSERT(textLayout->lines.size() <= 1 || (index != -1 && currentLine.isValid()));
        if (!currentLine.isValid())
            return false;
        const int col = columnNumber();
        int targetLinePos;
        if (op == Up) {
            targetLinePos = d->position - col - 1;
//             qDebug() << "I was at column" << col << "and position"
//                      << d->position << "so naturally I can find the previous line around"
//                      << (d->position - col - 1);
        } else {
            targetLinePos = d->position + currentLine.textLength() - col
                            + (currentIsLast ? 1 : 0);
//             qDebug() << "currentLine.textLength" << currentLine.textLength() << "col" << col
//                      << "currentIsLast" << currentIsLast;
            // ### probably need to add only if last line in layout
        }
        if (targetLinePos < 0) {
            if (d->position == 0) {
                return false;
            } else {
                setPosition(0, mode);
                return true;
            }
        } else if (targetLinePos >= d->document->documentSize()) {
            if (d->position == d->document->documentSize()) {
                return false;
            } else {
                setPosition(d->document->documentSize(), mode);
                return true;
            }
        }
        int offsetInLine;

        const QTextLine targetLine = textLayout->lineForPosition(targetLinePos, &offsetInLine);
        if (!targetLine.isValid())
            return false;
        targetLinePos -= offsetInLine; // targetLinePos should now be at col 0

//         qDebug() << "finding targetLine at" << targetLinePos
//                  << d->document->read(targetLinePos, 7)
//                  << "d->position" << d->position
//                  << "col" << col
//                  << "offsetInLine" << offsetInLine;

        int gotoCol = qMax(d->overrideColumn, col);
        if (gotoCol > targetLine.textLength()) {
            d->overrideColumn = qMax(d->overrideColumn, gotoCol);
            gotoCol = targetLine.textLength();
        }
        const int overrideColumn = d->overrideColumn;
        setPosition(targetLinePos + gotoCol, mode);
        d->overrideColumn = overrideColumn;
        break; }

    case EndOfLine:
    case StartOfLine: {
        int offset;
        bool lastLine;
        TextLayout *textLayout = TextLayoutCacheManager::requestLayout(*this, 1);
        QTextLine line = textLayout->lineForPosition(position(), &offset,
                                                     0, &lastLine);
        if (!line.isValid())
            return false;
        if (op == TextCursor::StartOfLine) {
            setPosition(position() - offset, mode);
        } else {
            int pos = position() - offset + line.textLength();
            if (!lastLine)
                --pos;
            setPosition(pos, mode);
        }
        break; }

    case StartOfBlock: {
        TextDocumentIterator it(d->document->d, d->position);
        const QLatin1Char newline('\n');
        while (it.hasPrevious() && it.previous() != newline) ;
        if (it.hasPrevious())
            it.next();
        setPosition(it.position(), mode);
        break; }
    case EndOfBlock: {
        TextDocumentIterator it(d->document->d, d->position);
        const QLatin1Char newline('\n');
        while (it.current() != newline && it.hasNext() && it.next() != newline) ;
        setPosition(it.position(), mode);
        break; }

    case StartOfWord:
    case PreviousWord:
    case WordLeft: {
        TextDocumentIterator it(d->document->d, d->position);

        while (it.hasPrevious()) {
            const QChar ch = it.previous();
            if (d->document->isWordCharacter(ch, it.position()))
                break;
        }
        while (it.hasPrevious()) {
            const QChar ch = it.previous();
            if (!d->document->isWordCharacter(ch, it.position()))
                break;
        }
        if (it.hasPrevious())
            it.next();
        setPosition(it.position(), mode);
        d->overrideColumn = -1;
        break; }

    case NextWord:
    case WordRight:
    case EndOfWord: {
        TextDocumentIterator it(d->document->d, d->position);
        while (it.hasNext()) {
            const QChar ch = it.next();
            if (d->document->isWordCharacter(ch, it.position()))
                break;
        }
        while (it.hasNext()) {
            const QChar ch = it.next();
            if (!d->document->isWordCharacter(ch, it.position()))
                break;
        }
        setPosition(it.position(), mode);
        d->overrideColumn = -1;
        break; }

    case PreviousBlock:
        movePosition(TextCursor::StartOfBlock, mode);
        movePosition(TextCursor::Left, mode);
        movePosition(TextCursor::StartOfBlock, mode);
        break;

    case NextBlock:
        movePosition(TextCursor::EndOfBlock, mode);
        movePosition(TextCursor::Right, mode);
        return true;

    case Left:
    case Right:
        d->overrideColumn = -1;
        setPosition(qBound<int>(0, position() + (op == TextCursor::Left ? -1 : 1),
                                d->document->documentSize()), mode);
        break;
    };

    return true;
}

void TextCursor::deleteChar()
{
    Q_ASSERT(!isNull());
    if (hasSelection()) {
        removeSelectedText();
    } else if (d->position < d->document->documentSize()) {
        const bool old = d->document->d->cursorCommand;
        d->document->d->cursorCommand = true;
        d->document->remove(d->position, 1);
        d->document->d->cursorCommand = old;
    }
}

void TextCursor::deletePreviousChar()
{
    Q_ASSERT(!isNull());
    if (hasSelection()) {
        removeSelectedText();
    } else if (d->position > 0) {
        const bool old = d->document->d->cursorCommand;
        d->document->d->cursorCommand = true;
        d->document->remove(d->position - 1, 1);
        d->document->d->cursorCommand = old;
        d->anchor = --d->position;
    }
}

void TextCursor::select(SelectionType selection)
{
    if (!d || !d->document)
        return;

    clearSelection();

    switch (selection) {
    case LineUnderCursor:
        movePosition(StartOfLine);
        movePosition(EndOfLine, KeepAnchor);
        break;
    case WordUnderCursor:
        movePosition(StartOfWord);
        movePosition(EndOfWord, KeepAnchor);
        break;
    case BlockUnderCursor:
        movePosition(StartOfBlock);
        // also select the paragraph separator
        if (movePosition(PreviousBlock)) {
            movePosition(EndOfBlock);
            movePosition(NextBlock, KeepAnchor);
        }
        movePosition(EndOfBlock, KeepAnchor);
        break;
    }
}

bool TextCursor::hasSelection() const
{
    return !isNull() && d->anchor != d->position;
}

void TextCursor::removeSelectedText()
{
    Q_ASSERT(!isNull());
    if (d->anchor == d->position)
        return;

    SelectionChangedEmitter emitter(textEdit);
    detach();
    cursorChanged(false);
    const int min = qMin(d->anchor, d->position);
    const int max = qMax(d->anchor, d->position);
    d->anchor = d->position = min;
    const bool old = d->document->d->cursorCommand;
    d->document->d->cursorCommand = true;
    d->document->remove(min, max - min);
    d->document->d->cursorCommand = old;
    cursorChanged(true);
}

void TextCursor::clearSelection()
{
    Q_ASSERT(!isNull());
    if (hasSelection()) {
        detach();
        SelectionChangedEmitter emitter(textEdit);
        d->anchor = d->position;
    }
}

int TextCursor::selectionStart() const
{
    return qMin(d->anchor, d->position);
}

int TextCursor::selectionEnd() const
{
    return qMax(d->anchor, d->position);
}

int TextCursor::selectionSize() const
{
    return selectionEnd() - selectionStart();
}


QString TextCursor::selectedText() const
{
    if (isNull() || d->anchor == d->position)
        return QString();

    const int min = qMin(d->anchor, d->position);
    const int max = qMax(d->anchor, d->position);
    return d->document->read(min, max - min);
}

bool TextCursor::atBlockStart() const
{
    Q_ASSERT(!isNull());
    return atStart() || d->document->read(d->position - 1, 1).at(0) == '\n';
}

bool TextCursor::atBlockEnd() const
{
    Q_ASSERT(!isNull());
    return atEnd() || d->document->read(d->position, 1).at(0) == '\n'; // ### is this right?
}

bool TextCursor::atStart() const
{
    Q_ASSERT(!isNull());
    return d->position == 0;
}

bool TextCursor::atEnd() const
{
    Q_ASSERT(!isNull());
    return d->position == d->document->documentSize();
}


bool TextCursor::operator!=(const TextCursor &rhs) const
{
    return !(*this == rhs);
}

bool TextCursor::operator<(const TextCursor &rhs) const
{
    if (!d)
        return true;

    if (!rhs.d)
        return false;

    Q_ASSERT_X(d->document == rhs.d->document, "TextCursor::operator<",
               "cannot compare cursors attached to different documents");

    return d->position < rhs.d->position;
}

bool TextCursor::operator<=(const TextCursor &rhs) const
{
    return *this < rhs || *this == rhs;
}

bool TextCursor::operator==(const TextCursor &rhs) const
{
    if (isCopyOf(rhs))
        return true;

    if (!d || !rhs.d)
        return false;

    return (d->position == rhs.d->position
            && d->anchor == rhs.d->anchor
            && d->document == rhs.d->document);
}

bool TextCursor::operator>=(const TextCursor &rhs) const
{
    return *this > rhs || *this == rhs;
}

bool TextCursor::operator>(const TextCursor &rhs) const
{
    if (!d)
        return false;

    if (!rhs.d)
        return true;

    Q_ASSERT_X(d->document == rhs.d->document, "TextCursor::operator>=",
               "cannot compare cursors attached to different documents");

    return d->position > rhs.d->position;
}

bool TextCursor::isCopyOf(const TextCursor &other) const
{
    return d == other.d;
}

int TextCursor::columnNumber() const
{
    Q_ASSERT(d && d->document);
    TextLayout *textLayout = TextLayoutCacheManager::requestLayout(*this, 0);
    int col;
    textLayout->lineForPosition(d->position, &col);
    return col;
}

int TextCursor::lineNumber() const
{
    Q_ASSERT(d && d->document);
    return d->document->lineNumber(position());
}

void TextCursor::detach()
{
    Q_ASSERT(d);
    if (d->ref > 1) {
        d->ref.deref();
        TextCursorSharedPrivate *p = new TextCursorSharedPrivate;
        p->position = d->position;
        p->overrideColumn = d->overrideColumn;
        p->anchor = d->anchor;
        p->document = d->document;
        d->document->d->textCursors.insert(p);
        d = p;
    }
}

bool TextCursor::ref()
{
    return d && d->ref.ref();
}

bool TextCursor::deref()
{
    TextCursorSharedPrivate *dd = d;
    d = 0;
    if (dd && !dd->ref.deref()) {
        if (dd->document) {
            const bool removed = dd->document->d->textCursors.remove(dd);
            Q_ASSERT(removed);
            Q_UNUSED(removed)
        }
        delete dd;
        return false;
    }
    return true;
}

void TextCursor::cursorChanged(bool ensureVisible)
{
    if (textEdit) {
        if (textEdit->d->cursorBlinkTimer.isActive())
            textEdit->d->cursorVisible = true;
        if (ensureVisible) {
            emit textEdit->cursorPositionChanged(d->position);
            textEdit->ensureCursorVisible();
        }
        const QRect r = textEdit->cursorBlockRect(*this) & textEdit->viewport()->rect();
        if (!r.isNull()) {
            textEdit->viewport()->update(r);
        }
    }
}

bool TextCursor::cursorMoveKeyEvent(QKeyEvent *e)
{
    MoveMode mode = MoveAnchor;
    MoveOperation op = NoMove;

    if (e == QKeySequence::MoveToNextChar) {
        op = Right;
    } else if (e == QKeySequence::MoveToPreviousChar) {
        op = Left;
    } else if (e == QKeySequence::SelectNextChar) {
        op = Right;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectPreviousChar) {
        op = Left;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectNextWord) {
        op = WordRight;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectPreviousWord) {
        op = WordLeft;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectStartOfLine) {
        op = StartOfLine;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectEndOfLine) {
        op = EndOfLine;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectStartOfBlock) {
        op = StartOfBlock;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectEndOfBlock) {
        op = EndOfBlock;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectStartOfDocument) {
        op = Start;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectEndOfDocument) {
        op = End;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectPreviousLine) {
        op = Up;
        mode = KeepAnchor;
    } else if (e == QKeySequence::SelectNextLine) {
        op = Down;
        mode = KeepAnchor;
#if 0
        {
            QTextBlock block = cursor.block();
            QTextLine line = currentTextLine(cursor);
            if (!block.next().isValid()
                && line.isValid()
                && line.lineNumber() == block.layout()->lineCount() - 1)
                op = End;
        }
#endif
    } else if (e == QKeySequence::MoveToNextWord) {
        op = WordRight;
    } else if (e == QKeySequence::MoveToPreviousWord) {
        op = WordLeft;
    } else if (e == QKeySequence::MoveToEndOfBlock) {
        op = EndOfBlock;
    } else if (e == QKeySequence::MoveToStartOfBlock) {
        op = StartOfBlock;
    } else if (e == QKeySequence::MoveToNextLine) {
        op = Down;
    } else if (e == QKeySequence::MoveToPreviousLine) {
        op = Up;
    } else if (e == QKeySequence::MoveToStartOfLine) {
        op = StartOfLine;
    } else if (e == QKeySequence::MoveToEndOfLine) {
        op = EndOfLine;
    } else if (e == QKeySequence::MoveToStartOfDocument) {
        op = Start;
    } else if (e == QKeySequence::MoveToEndOfDocument) {
        op = End;
    } else if (e == QKeySequence::MoveToNextPage || e == QKeySequence::MoveToPreviousPage
               || e == QKeySequence::SelectNextPage || e == QKeySequence::SelectPreviousPage) {
        const MoveMode mode = (e == QKeySequence::MoveToNextPage
                               || e == QKeySequence::MoveToPreviousPage
                               ? MoveAnchor : KeepAnchor);
        const MoveOperation operation = (e == QKeySequence::MoveToNextPage
                                         || e == QKeySequence::SelectNextPage
                                         ? Down : Up);
        int visibleLines = 10;
        if (textEdit)
            visibleLines = textEdit->d->visibleLines;
        for (int i=0; i<visibleLines; ++i) {
            if (!movePosition(operation, mode))
                break;
        }
        return true;
    } else {
        return false;
    }
    return movePosition(op, mode);
}

int TextCursor::viewportWidth() const
{
    return textEdit ? textEdit->viewport()->width() : d->viewportWidth;
}

void TextCursor::setViewportWidth(int width)
{
    if (textEdit) {
        qWarning("It makes no sense to set the viewportWidth of a text cursor that belongs to a text edit. The actual viewportWidth will be used");
        return;
    }
    d->viewportWidth = width;
}

QChar TextCursor::cursorCharacter() const
{
    Q_ASSERT(d && d->document);
    return d->document->readCharacter(d->position);
}

QString TextCursor::cursorLine() const
{
    Q_ASSERT(d && d->document);
    int layoutIndex = -1;
    TextLayout *textLayout = TextLayoutCacheManager::requestLayout(*this, 2); // should I use 1?

    QTextLine line = textLayout->lineForPosition(position(), 0, &layoutIndex);
    Q_ASSERT(line.isValid()); // ### could this be legitimate?
    Q_ASSERT(textLayout);
    return textLayout->textLayouts.at(layoutIndex)->text()
        .mid(line.textStart(), line.textLength());
    // ### there is a bug here. It doesn't seem to use the right
    // ### viewportWidth even though it is the textEdit's cursor
}

QString TextCursor::wordUnderCursor() const
{
    Q_ASSERT(!isNull());
    return d->document->d->wordAt(d->position);
}

QString TextCursor::paragraphUnderCursor() const
{
    Q_ASSERT(!isNull());
    return d->document->d->paragraphAt(d->position);
}

QDebug operator<<(QDebug dbg, const TextCursor &cursor)
{
    QString ret = QString::fromLatin1("TextCursor(");
    if (cursor.isNull()) {
        ret.append(QLatin1String("null)"));
        dbg.maybeSpace() << ret;
    } else {
        if (!cursor.hasSelection()) {
            dbg << "anchor/position:" << cursor.anchor() << "character:" << cursor.cursorCharacter();
        } else {
            dbg << "anchor:" << cursor.anchor() << "position:" << cursor.position()
                << "selectionSize:" << cursor.selectionSize();
            QString selectedText;
            if (cursor.selectionSize() > 10) {
                selectedText = cursor.document()->read(cursor.selectionStart(), 7);
                selectedText.append("...");
            } else {
                selectedText = cursor.selectedText();
            }
            dbg << "selectedText:" << selectedText;
        }
    }
    return dbg.space();
}


