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

#ifndef TEXTCURSOR_H
#define TEXTCURSOR_H

#include <QString>
#include <QKeyEvent>
#include <QSize>
#include <QTextLine>

class TextEdit;
class TextLayout;
class TextDocument;
class TextCursorSharedPrivate;
class TextCursor
{
public:
    TextCursor();
    explicit TextCursor(const TextDocument *document, int pos = 0, int anchor = -1);
    explicit TextCursor(const TextEdit *document, int pos = 0, int anchor = -1);
    TextCursor(const TextCursor &cursor);
    TextCursor &operator=(const TextCursor &other);
    ~TextCursor();

    TextDocument *document() const;
    bool isNull() const;
    inline bool isValid() const { return !isNull(); }

    enum MoveMode {
        MoveAnchor,
        KeepAnchor
    };

    void setPosition(int pos, MoveMode mode = MoveAnchor);
    int position() const;

    void setSelection(int pos, int length); // can be negative

    int viewportWidth() const;
    void setViewportWidth(int width);

    int anchor() const;

    void insertText(const QString &text);

    QChar cursorCharacter() const;
    QString cursorLine() const;

    QString wordUnderCursor() const;
    QString paragraphUnderCursor() const;

    enum MoveOperation {
        NoMove,
        Start,
        Up,
        StartOfLine,
        StartOfBlock,
        StartOfWord,
        PreviousBlock,
        PreviousCharacter,
        PreviousWord,
        Left,
        WordLeft,
        End,
        Down,
        EndOfLine,
        EndOfWord,
        EndOfBlock,
        NextBlock,
        NextCharacter,
        NextWord,
        Right,
        WordRight
    };

    bool movePosition(MoveOperation op, MoveMode = MoveAnchor, int n = 1);

    void deleteChar();
    void deletePreviousChar();

    enum SelectionType {
        WordUnderCursor,
        LineUnderCursor,
        BlockUnderCursor
    };

    void select(SelectionType selection);

    bool hasSelection() const;
    void removeSelectedText();
    void clearSelection();
    int selectionStart() const;
    int selectionEnd() const;
    int selectionSize() const;
    inline int selectionLength() const { return selectionSize(); }

    QString selectedText() const;

    bool atBlockStart() const;
    bool atBlockEnd() const;
    bool atStart() const;
    bool atEnd() const;

    bool operator!=(const TextCursor &rhs) const;
    bool operator<(const TextCursor &rhs) const;
    bool operator<=(const TextCursor &rhs) const;
    bool operator==(const TextCursor &rhs) const;
    bool operator>=(const TextCursor &rhs) const;
    bool operator>(const TextCursor &rhs) const;

    bool isCopyOf(const TextCursor &other) const;

    int columnNumber() const;
    int lineNumber() const;
private:
    TextLayout *textLayout(int margin) const;
    bool cursorMoveKeyEvent(QKeyEvent *e);
    void cursorChanged(bool ensureCursorVisible);
    void detach();
    bool ref();
    bool deref();

    TextCursorSharedPrivate *d;
    TextEdit *textEdit;
    friend class TextEdit;
    friend class TextLayoutCacheManager;
    friend class TextDocument;
    friend class TextDocumentPrivate;
};

QDebug operator<<(QDebug dbg, const TextCursor &cursor);

#endif
