// Copyright 2009 Anders Bakken
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
#include <QTemporaryFile>
#include <QDebug>
#include "weakpointer.h"

#ifndef ASSUME
  #ifdef FATAL_ASSUMES
    #define ASSUME(cond) Q_ASSERT(cond)
  #else
    #if defined(__GNUC__) || defined(_AIX)
      #define ASSUME(cond) if (!(cond)) qWarning("Failed assumption %s:%d (%s) %s", __FILE__, __LINE__, __FUNCTION__, #cond);
    #else
      #define ASSUME(cond) if (!(cond)) qWarning("Failed assumption %s:%d %s", __FILE__, __LINE__, #cond);
    #endif
  #endif
#endif

#include "textdocument.h"
#ifdef NO_TEXTDOCUMENT_CACHE
#define NO_TEXTDOCUMENT_CHUNK_CACHE
#define NO_TEXTDOCUMENT_READ_CACHE
#endif

#if defined TEXTDOCUMENT_LINENUMBER_CACHE && !defined TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL
#define TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL 100
#endif

static inline bool matchSection(const TextSection *section, const TextEdit *textEdit)
{
    if (!textEdit) {
        return true;
    } else if (!section->textEdit()) {
        return true;
    } else {
        return textEdit == section->textEdit();
    }
}


struct Chunk {
    Chunk() : previous(0), next(0), from(-1), length(0),
              firstLineIndex(-1), lines(-1), swap(0) {}
    ~Chunk() { delete swap; }

    mutable QString data;
    Chunk *previous, *next;
    int size() const { return data.isEmpty() ? length : data.size(); }
#ifdef QT_DEBUG
    int pos() const { int p = 0; Chunk *c = previous; while (c) { p += c->size(); c = c->previous; }; return p; }
#endif
    mutable int from, length; // Not used when all is loaded
    mutable int firstLineIndex, lines;
#ifdef TEXTDOCUMENT_LINENUMBER_CACHE
    mutable QVector<int> lineNumbers;
    // format is how many endlines in the area from (n *
    // TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL) to
    // ((n + 1) * TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL)
#endif
    QTemporaryFile *swap;
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

static inline bool compareTextSection(const TextSection *left, const TextSection *right)
{
    // don't make this compare document. Look at ::sections()
    return left->position() < right->position();
}

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

struct TextSection;
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
        saveState(NotSaving), ownDevice(false), modified(false),
        deviceMode(TextDocument::Sparse), chunkSize(16384),
        undoRedoStackCurrent(0), modifiedIndex(-1), undoRedoEnabled(true), ignoreUndoRedo(false),
        hasChunksWithLineNumbers(false), textCodec(0), options(0), cursorCommand(false)
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
    QList<TextSection*> sections;
    WeakPointer<QIODevice> device;
    bool ownDevice, modified;
    TextDocument::DeviceMode deviceMode;
    int chunkSize;

    QList<DocumentCommand*> undoRedoStack;
    int undoRedoStackCurrent, modifiedIndex;
    bool undoRedoEnabled, ignoreUndoRedo;

    bool hasChunksWithLineNumbers;
    QTextCodec *textCodec;
    TextDocument::Options options;
    bool cursorCommand;

#ifdef QT_DEBUG
    mutable QSet<TextDocumentIterator*> iterators;
#endif
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
    void swapOutChunk(Chunk *c);
    QList<TextSection*> getSections(int from, int size, TextSection::TextSectionOptions opt, const TextEdit *filter) const;
    inline TextSection *sectionAt(int pos, const TextEdit *filter) const { return getSections(pos, 1, TextSection::IncludePartial, filter).value(0); }
    void textEditDestroyed(TextEdit *edit);
signals:
    void sectionFormatChanged(TextSection *section);
    void sectionCursorChanged(TextSection *section);
    void undoRedoCommandInserted(DocumentCommand *cmd);
    void undoRedoCommandRemoved(DocumentCommand *cmd);
    void undoRedoCommandTriggered(DocumentCommand *cmd, bool undo);
    void undoRedoCommandFinished(DocumentCommand *cmd);
private:
    friend class TextSection;
};

// should not be kept as a member. Invalidated on inserts/removes
class TextDocumentIterator
{
public:
    TextDocumentIterator(const TextDocumentPrivate *d, int p)
        : doc(d), pos(p), convert(false)
    {
        Q_ASSERT(doc);
        if (p == d->documentSize) {
            chunk = d->last;
            offset = chunk->size() - 1;
        } else {
            chunk = doc->chunkAt(p, &offset);
            Q_ASSERT(chunk);
            const int chunkPos = p - offset;
            chunkData = doc->chunkData(chunk, chunkPos);
#ifdef QT_DEBUG
            if (doc->q->readCharacter(p) != chunkData.at(offset)) {
                qDebug() << doc->q->readCharacter(p) << pos
                         << chunkData.at(offset) << offset << chunkData.size();
            }
#endif
            Q_ASSERT(chunkData.at(offset) == doc->q->readCharacter(p));
        }
#ifdef QT_DEBUG
        doc->iterators.insert(this);
#endif
    }
#ifdef QT_DEBUG
    ~TextDocumentIterator()
    {
        Q_ASSERT(doc->iterators.remove(this));
    }
#endif

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
#ifdef QT_DEBUG
        if (doc->q->readCharacter(pos) != chunkData.at(offset)) {
            qDebug() << doc->q->readCharacter(pos) << pos
                     << chunkData.at(offset) << offset << chunkData.size();
        }
#endif
        ASSUME(doc->q->readCharacter(pos) == chunkData.at(offset));
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
