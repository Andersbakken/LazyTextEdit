#ifndef TEXTDOCUMENT_P_H
#define TEXTDOCUMENT_P_H

#include <QString>
#include <QApplication>
#include <QThread>
#include <QIODevice>
#include <QTime>
#include <QMutexLocker>
#include <QMutex>
#include <QSet>
#include "textdocument.h"
#ifdef NO_TEXTDOCUMENT_CACHE
#define NO_TEXTDOCUMENT_CHUNK_CACHE
#define NO_TEXTDOCUMENT_READ_CACHE
#endif

#if defined TEXTDOCUMENT_LINENUMBER_CACHE && !defined TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL
#define TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL 100
#endif

struct Chunk {
    Chunk() : previous(0), next(0), from(-1), length(0), firstLineIndex(-1) {}

    mutable QString data;
    Chunk *previous, *next;
    int size() const { return data.isEmpty() ? length : data.size(); }
    mutable int from, length, firstLineIndex; // Not used when all is loaded
#ifdef TEXTDOCUMENT_LINENUMBER_CACHE
    mutable QVector<int> lineNumbers;
    // format is how many endlines in the area from (n *
    // TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL) to
    // ((n + 1) * TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL)
    static int lineNumberCacheInterval() { return TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL; }
#endif
};


// should really use this stuff for all of this stuff

static inline QPair<int, int> intersection(int index1, int size1, int index2, int size2)
{
    QPair<int, int> ret;
    ret.first = qMax(index1, index2);
    const int right = qMin(index1 + size1, index2 + size2);
    ret.second = right - ret.first;
    if (ret.second <= 0)
        return qMakePair(-1, 0);
    return ret;
}

class SectionManager : public QObject
{
    Q_OBJECT
public:
    static SectionManager *instance() { static SectionManager *inst = new SectionManager; return inst; }
signals:
    void sectionFormatChanged(Section *section);
private:
    SectionManager() : QObject(qApp) {}
    friend class Section;
};


class TextDocumentIterator;
struct DocumentCommand {
    enum Type {
        None,
        Inserted,
        Removed
    };

    DocumentCommand(Type t, int pos = -1, const QString &string = QString())
        : type(t), position(pos), text(string), joinStatus(NoJoin)
    {}

    const Type type;
    int position;
    QString text;

    enum JoinStatus {
        NoJoin,
        Forward,
        Backward
    } joinStatus;
};

struct Section;
struct TextCursorSharedPrivate;
struct TextDocumentPrivate : public QObject
{
    Q_OBJECT
public:
    TextDocumentPrivate(TextDocument *doc)
        : q(doc), first(0), last(0),
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
        cachedChunk(0), cachedChunkPos(-1),
#endif
#ifndef NO_TEXTDOCUMENT_READ_CACHE
        cachePos(-1),
#endif
        documentSize(0),
        saveState(NotSaving), device(0), ownDevice(false), modified(false),
        deviceMode(TextDocument::Sparse), chunkSize(16384),
        undoRedoStackCurrent(0), modifiedIndex(-1), undoRedoEnabled(true), ignoreUndoRedo(false),
        hasChunksWithLineNumbers(false), textCodec(0)
    {
        first = last = new Chunk;
    }

    TextDocument *q;
    QSet<TextCursorSharedPrivate*> textCursors;
    mutable Chunk *first, *last;

#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
    mutable Chunk *cachedChunk;
    mutable int cachedChunkPos;
    mutable QString cachedChunkData; // last uninstantiated chunk's read from file
#endif
#ifndef NO_TEXTDOCUMENT_READ_CACHE
    mutable int cachePos;
    mutable QString cache; // results of last read(). Could span chunks
#endif

    int documentSize;
    enum SaveState { NotSaving, Saving, AbortSave } saveState;
    QList<Section*> sections;
    QIODevice *device;
    bool ownDevice, modified;
    TextDocument::DeviceMode deviceMode;
    int chunkSize;

    QList<DocumentCommand*> undoRedoStack;
    int undoRedoStackCurrent, modifiedIndex;
    bool undoRedoEnabled, ignoreUndoRedo;

    bool hasChunksWithLineNumbers;
    QTextCodec *textCodec;

    void joinLastTwoCommands();

    void removeChunk(Chunk *c);
    QString chunkData(const Chunk *chunk, int pos) const;
    int chunkIndex(const Chunk *c) const;

    // evil API. pos < 0 means don't cache

    void updateChunkLineNumbers(Chunk *c, int pos) const;
    int countNewLines(Chunk *c, int chunkPos, int index) const;

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
public slots:
    void onDeviceDestroyed(QObject *o);
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
        const int chunkPos = p - offset;
        chunkData = doc->chunkData(chunk, chunkPos);
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
