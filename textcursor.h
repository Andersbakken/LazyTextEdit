#ifndef TEXTCURSOR_H
#define TEXTCURSOR_H

#include <QString>
#include <QPointer>
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

#endif
