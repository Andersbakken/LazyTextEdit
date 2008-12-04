#ifndef TEXTDOCUMENT_P_H
#define TEXTDOCUMENT_P_H

#include <QString>
#include <QThread>
#include <QIODevice>
#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QMutex>
#include "textdocument.h"

struct Chunk {
    Chunk() : previous(0), next(0), from(-1), length(0) {}

    mutable QString data;
    Chunk *previous, *next;
    int size() const { return data.isEmpty() ? length : data.size(); }
    mutable int from, length; // Not used when all is loaded
};


class TextDocumentIterator;
struct DocumentCommand {
    enum Type {
        None,
        Inserted,
        Removed
    };

    DocumentCommand(Type t, int pos = -1, const QString &string = QString())
        : type(t), position(pos), text(string), joined(Not)
    {}

    const Type type;
    int position;
    QString text;

    enum Joined {
        Not,
        Forward,
        Backward
    } joined;
};

struct Section;
struct TextCursorSharedPrivate;
struct TextDocumentPrivate : public QObject
{
    Q_OBJECT
public:
    TextDocumentPrivate(TextDocument *doc)
        : q(doc), first(0), last(0), cachedChunk(0), cachedChunkPos(-1), documentSize(0),
        saveState(NotSaving), cachePos(-1), device(0), ownDevice(false),
        deviceMode(TextDocument::Sparse), chunkSize(16384),
        undoRedoStackCurrent(0), undoRedoEnabled(true), ignoreUndoRedo(false)
    {
        first = last = new Chunk;
        first->from = 0;
    }

    TextDocument *q;
    QSet<TextCursorSharedPrivate*> textCursors;
    mutable Chunk *first, *last, *cachedChunk;
    mutable int cachedChunkPos;
    mutable QString cachedChunkData; // last uninstantiated chunk's read from file
    int documentSize;
    enum SaveState { NotSaving, Saving, AbortSave } saveState;
    mutable int cachePos;
    mutable QString cache; // results of last read(). Could span chunks
    QList<Section*> sections;
    QIODevice *device;
    bool ownDevice;
    TextDocument::DeviceMode deviceMode;
    int chunkSize;

    QList<DocumentCommand*> undoRedoStack;
    int undoRedoStackCurrent;
    bool undoRedoEnabled, ignoreUndoRedo;

    void joinLastTwoCommands();

    void removeChunk(Chunk *c);
    QString chunkData(const Chunk *chunk, int pos) const;
    // evil API. pos < 0 means don't cache

    void instantiateChunk(Chunk *chunk);
    Chunk *chunkAt(int pos, int *offset) const;
    void clearRedo();
    void undoRedo(bool undo);

    QString wordAt(int position, int *start = 0) const;
    QString paragraphAt(int position, int *start = 0) const;

    friend class TextDocument;
    static inline bool isWord(const QChar &ch) // from qregexp.cpp
    {
        return ch.isLetterOrNumber() || ch.isMark() || ch == QLatin1Char('_');
    }

signals:
    void undoRedoCommandInserted(DocumentCommand *cmd);
    void undoRedoCommandRemoved(DocumentCommand *cmd);
    void undoRedoCommandTriggered(DocumentCommand *cmd, bool undo);
    void undoRedoCommandFinished(DocumentCommand *cmd);
};

// should not be kept as a member. Invalidated on inserts/removes
class TextDocumentIterator
{
public:
    TextDocumentIterator(const TextDocumentPrivate *d, int p)
        : doc(d), pos(p), convert(false)
    {
        Q_ASSERT(doc);
        chunk = doc->chunkAt(p, &offset);
        Q_ASSERT(chunk);
        chunkData = doc->chunkData(chunk, p + offset);
    }

    inline bool hasNext() const
    {
        return (chunk->next || chunk->size() > offset + 1);
    }

    inline bool hasPrevious() const
    {
        return (chunk->previous || offset > 0);
    }

    inline int position() const
    {
        return pos;
    }

    inline QChar current() const
    {
        Q_ASSERT(doc);
        Q_ASSERT(chunk);
        if (pos == doc->documentSize)
            return QChar();
        Q_ASSERT(doc->q->readCharacter(pos) == chunkData.at(offset));
        return convert ? chunkData.at(offset).toLower() : chunkData.at(offset);
    }

    inline QChar next()
    {
        Q_ASSERT(doc);
        Q_ASSERT(chunk);
        ++pos;
        if (++offset >= chunkData.size()) {
            offset = 0;
            chunk = chunk->next;
            Q_ASSERT(chunk);
            chunkData = doc->chunkData(chunk, pos);
        }
        return current();
    }

    inline QChar previous()
    {
        Q_ASSERT(doc);
        Q_ASSERT(chunk);
        Q_ASSERT(hasPrevious());
        --pos;
        if (--offset < 0) {
            chunk = chunk->previous;
            Q_ASSERT(chunk);
            chunkData = doc->chunkData(chunk, pos - chunk->size() - 1);
            offset = chunkData.size() - 1;
        }
        return current();
    }

    enum Direction { Left, Right };
    inline QChar nextPrev(Direction dir, bool &ok)
    {
        if (dir == Left) {
            if (!hasPrevious()) {
                ok = false;
                return QChar();
            }
            ok = true;
            return previous();
        } else {
            if (!hasNext()) {
                ok = false;
                return QChar();
            }
            ok = true;
            return next();
        }
    }

    bool convertToLowerCase() const
    {
        return convert;
    }

    void setConvertToLowerCase(bool on)
    {
        convert = on;
    }

private:
    const TextDocumentPrivate *doc;
    int pos;
    int offset;
    QString chunkData;
    Chunk *chunk;
    bool convert;
};


#endif
