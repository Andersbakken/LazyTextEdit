#include "textdocument.h"
#include "textcursor.h"
#include "textcursor_p.h"
#include "textdocument_p.h"
#include <QBuffer>
#include <QObject>
#include <QString>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QTextCharFormat>
#include <QVariant>
#include <qalgorithms.h>

// #define DEBUG_CACHE_HITS

TextDocument::TextDocument(QObject *parent)
    : QObject(parent), d(new TextDocumentPrivate(this))
{
}

TextDocument::~TextDocument()
{
    foreach(TextCursorSharedPrivate *cursor, d->textCursors) {
        cursor->document = 0;
    }
    Chunk *c = d->first;
    while (c) {
        if (!(d->options & KeepTemporaryFiles) && !c->swap.isEmpty())
            QFile::remove(c->swap);
        Chunk *tmp = c;
        c = c->next;
        delete tmp;
    }
    foreach(TextSection *section, d->sections) {
        section->d.document = 0;
        section->d.textEdit = 0;
        delete section;
    }
    if (d->ownDevice)
        delete d->device.data();

    delete d;
}

bool TextDocument::load(QIODevice *device, DeviceMode mode, QTextCodec *codec)
{
    Q_ASSERT(device);
    if (!device->isReadable())
        return false;

    Options options = d->options;
    if (options & ConvertCarriageReturns && mode == Sparse) {
        qWarning("TextDocument::load() ConvertCarriageReturns is incompatible with Sparse");
        options &= ~ConvertCarriageReturns;
    }
    if ((options & (AutoDetectCarriageReturns|ConvertCarriageReturns)) == (AutoDetectCarriageReturns|ConvertCarriageReturns))
        options &= ~AutoDetectCarriageReturns;

    foreach(TextSection *section, d->sections) {
        emit sectionRemoved(section);
        section->d.document = 0;
        delete section;
    }
    d->sections.clear();

    if (d->documentSize > 0) {
        emit charactersRemoved(0, d->documentSize);
    }

    Chunk *c = d->first;
    while (c) {
        Chunk *tmp = c;
        c = c->next;
        delete tmp;
    }

    d->textCodec = codec;
    d->documentSize = device->size();
    if (d->documentSize <= d->chunkSize && mode == Sparse && !(options & NoImplicitLoadAll))
        mode = LoadAll;
#if 0
    if (codec && mode == Sparse) {

        qWarning("Sparse mode doesn't really work with unicode data yet. I am working on it.\n--\nAnders");
    }
#endif
    d->first = d->last = 0;

    if (d->device) {
        if (d->ownDevice && d->device.data() != device) // this is done when saving to the same file
            delete d->device.data();
    }

    d->ownDevice = false;
    d->device = device;
    d->deviceMode = mode;
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
    d->cachedChunk = 0;
    d->cachedChunkPos = -1;
    d->cachedChunkData.clear();
#endif
#ifndef NO_TEXTDOCUMENT_READ_CACHE
    d->cachePos = -1;
    d->cache.clear();
#endif

    switch (d->deviceMode) {
    case LoadAll: {
        device->seek(0);
        QTextStream ts(device);
        if (d->textCodec)
            ts.setCodec(d->textCodec);
        Chunk *current = 0;
        d->documentSize = 0; // in case of unicode
        do {
            Chunk *c = new Chunk;
            c->data = ts.read(d->chunkSize);
            if (options & AutoDetectCarriageReturns) {
                if (c->data.contains(QLatin1Char('\n'))) {
                    options |= ConvertCarriageReturns;
                }
                options &= ~AutoDetectCarriageReturns;
            }
            if (options & ConvertCarriageReturns)
                c->data.remove(QLatin1Char('\r'));
            d->documentSize += c->data.size();
            if (current) {
                current->next = c;
                c->previous = current;
            } else {
                d->first = c;
            }
            current = c;
        } while (!ts.atEnd());

        d->last = current;
        break; }

    case Sparse: {
        int index = 0;
        Chunk *current = 0;
        do {
            Chunk *chunk = new Chunk;
            chunk->from = index;
            chunk->length = qMin<int>(d->documentSize - index, d->chunkSize);
            if (!current) {
                d->first = chunk;
            } else {
                chunk->previous = current;
                current->next = chunk;
            }
            current = chunk;
            index += chunk->length;
        } while (index < d->documentSize);

        d->last = current;
        break; }
    }
//     if (d->first)
//         d->first->firstLineIndex = 0;
    emit charactersAdded(0, d->documentSize);
    emit documentSizeChanged(d->documentSize);
    emit textChanged();
    setModified(false);
    return true;
}

bool TextDocument::load(const QString &fileName, DeviceMode mode, QTextCodec *codec)
{
    if (mode == LoadAll) {
        QFile from(fileName);
        return from.open(QIODevice::ReadOnly) && load(&from, mode, codec);
    } else {
        QFile *file = new QFile(fileName);
        if (file->open(QIODevice::ReadOnly) && load(file, mode, codec)) {
            d->ownDevice = true;
            return true;
        } else {
            delete file;
            d->ownDevice = false;
            return false;
        }
    }
}

void TextDocument::clear()
{
    setText(QString());
}

QString TextDocument::read(int pos, int size) const
{
    Q_ASSERT(size >= 0);
    if (size == 0 || pos == d->documentSize) {
        return QString();
    }
    Q_ASSERT(pos < d->documentSize);

#ifndef NO_TEXTDOCUMENT_READ_CACHE
#ifdef DEBUG_CACHE_HITS
    static int hits = 0;
    static int misses = 0;
#endif
    if (d->cachePos != -1 && pos >= d->cachePos && d->cache.size() - (pos - d->cachePos) >= size) {
#ifdef DEBUG_CACHE_HITS
        qWarning() << "read hits" << ++hits << "misses" << misses;
#endif
        return d->cache.mid(pos - d->cachePos, size);
    }
#ifdef DEBUG_CACHE_HITS
    qWarning() << "read hits" << hits << "misses" << ++misses;
#endif
#endif

    QString ret(size, '\0');
    int written = 0;
    int offset;
    Chunk *c = d->chunkAt(pos, &offset);
    Q_ASSERT(c);
    int chunkPos = pos - offset;

    while (written < size && c) {
        const int max = qMin(size - written, c->size() - offset);
        const QString data = d->chunkData(c, chunkPos);
        chunkPos += data.size();
        ret.replace(written, max, data.constData() + offset, max);
        written += max;
        offset = 0;
        c = c->next;
    }

    if (written < size) {
        ret.truncate(written);
    }
    Q_ASSERT(!c || written == size);
#ifndef NO_TEXTDOCUMENT_READ_CACHE
    d->cachePos = pos;
    d->cache = ret;
#endif
    return ret;
}

QStringRef TextDocument::readRef(int pos, int size) const
{
    int offset;
    Chunk *c = d->chunkAt(pos, &offset);
    if (c && pos + offset + size <= c->size()) {
        const QString string = d->chunkData(c, pos - offset);
        return string.midRef(offset, size);
    }
    return QStringRef();
}


bool TextDocument::save(const QString &file)
{
    QFile from(file);
    return from.open(QIODevice::WriteOnly) && save(&from);
}

bool TextDocument::save()
{
    return d->device && save(d->device.data());
}


static bool isSameFile(const QIODevice *left, const QIODevice *right)
{
    if (left == right)
        return true;
    if (const QFile *lf = qobject_cast<const QFile *>(left)) {
        if (const QFile *rf = qobject_cast<const QFile *>(right)) {
            return QFileInfo(*lf) == QFileInfo(*rf);
        }
    }
    return false;
}

bool TextDocument::save(QIODevice *device)
{
    Q_ASSERT(device);
    if (::isSameFile(d->device.data(), device)) {
        QTemporaryFile tmp(0);
        if (!tmp.open())
            return false;
        if (save(&tmp)) {
            Q_ASSERT(qobject_cast<QFile*>(device));
            Q_ASSERT(qobject_cast<QFile*>(d->device));
            d->device.data()->close();
            d->device.data()->open(QIODevice::WriteOnly);
            tmp.seek(0);
            const int chunkSize = 128; //1024 * 16;
            char chunk[chunkSize];
            forever {
                const qint64 read = tmp.read(chunk, chunkSize);
                switch (read) {
                case -1: return false;
                case 0: return true;
                default:
                    if (d->device.data()->write(chunk, read) != read) {
                        return false;
                    }
                    break;
                }
            }
            d->device.data()->close();
            d->device.data()->open(QIODevice::ReadOnly);
            if (d->deviceMode == Sparse) {
                qDeleteAll(d->undoRedoStack);
                d->undoRedoStack.clear();
                d->undoRedoStackCurrent = 0;
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
                d->cachedChunkPos = -1;
                d->cachedChunk = 0;
                d->cachedChunkData.clear();
#endif
                Chunk *c = d->first;
                int pos = 0;
                while (c) {
                    Q_ASSERT((c->from == -1) == (c->length == -1));
                    if (c->from == -1) { // unload chunks from memory
                        c->from = pos;
                        c->length = c->data.size();
                        c->data.clear();
                    }
                    pos += c->length;
                    c = c->next;
                }
            }

            return true;
//            return load(d->device, d->deviceMode);
        }
        return false;

    }
    Q_ASSERT(device);
    if (!device->isWritable() || !d->first) {
        return false;
    }
    d->saveState = TextDocumentPrivate::Saving;
    const Chunk *c = d->first;
    emit saveProgress(0);
    int written = 0;
    QTextStream ts(device);
    if (d->textCodec)
        ts.setCodec(d->textCodec);
    while (c) {
        ts << d->chunkData(c, written);
        written += c->size();
        if (c != d->last) {
            const double part = qreal(written) / double(d->documentSize);
            emit saveProgress(int(part * 100.0));
        }
        if (d->saveState == TextDocumentPrivate::AbortSave) {
            d->saveState = TextDocumentPrivate::NotSaving;
            return false;
        }
        c = c->next;
    }
    d->saveState = TextDocumentPrivate::NotSaving;
    emit saveProgress(100);

    return true;
}

int TextDocument::documentSize() const
{
    return d->documentSize;
}

int TextDocument::chunkCount() const
{
    Chunk *c = d->first;
    int count = 0;
    while (c) {
        ++count;
        c = c->next;
    }
    return count;
}

int TextDocument::instantiatedChunkCount() const
{
    Chunk *c = d->first;
    int count = 0;
    while (c) {
        if (!c->data.isEmpty())
            ++count;
        c = c->next;
    }
    return count;
}

int TextDocument::swappedChunkCount() const
{
    Chunk *c = d->first;
    int count = 0;
    while (c) {
        if (!c->swap.isEmpty())
            ++count;
        c = c->next;
    }
    return count;
}

TextDocument::DeviceMode TextDocument::deviceMode() const
{
    return d->deviceMode;
}

QTextCodec * TextDocument::textCodec() const
{
    return d->textCodec;
}

class FindScope
{
public:
    FindScope(TextDocumentPrivate::FindState *s) : state(s) { if (state) *state = TextDocumentPrivate::Finding; }
    ~FindScope() { if (state) *state = TextDocumentPrivate::NotFinding; }
    TextDocumentPrivate::FindState *state;
};

static void initFind(const TextCursor &cursor, bool reverse, int *start, int *limit)
{
    if (cursor.hasSelection()) {
        *start = cursor.selectionStart();
        *limit = cursor.selectionEnd();
        if (reverse) {
            qSwap(*start, *limit);
        }
    } else {
        *start = cursor.position();
        *limit = (reverse ? 0 : cursor.document()->documentSize());
    }
}

TextCursor TextDocument::find(const QRegExp &regexp, const TextCursor &cursor, FindMode flags) const
{
    if (flags & FindWholeWords) {
        qWarning("FindWholeWords doesn't work with regexps. Instead use an actual RegExp for this");
    }
    if (flags & FindCaseSensitively) {
        qWarning("FindCaseSensitively doesn't work with regexps. Instead use an QRegExp::caseSensitivity for this");
    }


    const bool reverse = flags & FindBackward;
    int pos;
    int limit;
    ::initFind(cursor, reverse, &pos, &limit);

    if (pos == d->documentSize) {
        if (!reverse)
            return TextCursor();
        --pos;
    }


    const TextDocumentIterator::Direction direction = (reverse
                                                       ? TextDocumentIterator::Left
                                                       : TextDocumentIterator::Right);
    TextDocumentIterator it(d, pos);
    if (reverse) {
        it.setMinBoundary(limit);
    } else {
        it.setMaxBoundary(limit);
    }
    const QLatin1Char newline('\n');
    int last = pos;
    bool ok = true;
    int progressInterval = 0;
    int lastProgress = pos;
    const int initialPos = pos;
    int maxFindLength = 0;
    const FindScope scope(flags & FindAllowInterrupt ? &d->findState : 0);
    if (flags & FindAllowInterrupt) {
        progressInterval = qMax<int>(1, (reverse
                                         ? (static_cast<qreal>(pos) / 100.0)
                                         : (static_cast<qreal>(d->documentSize) - static_cast<qreal>(pos)) / 100.0));
        maxFindLength = (reverse ? pos : d->documentSize - pos);
    }
    do {
        while ((it.nextPrev(direction, ok) != newline) && ok) ;
        int from = qMin(it.position(), last);
        int to = qMax(it.position(), last);
        if (!ok) {
            if (direction == TextDocumentIterator::Right)
                ++to;
        } else if (direction == TextDocumentIterator::Left) {
            ++from;
            ++to;
        }
        const QString line = read(from, to - from);
        last = it.position() + 1;
        const int index = (reverse ? regexp.lastIndexIn(line) : regexp.indexIn(line));
        if (index != -1) {
            if (!reverse && from + index + regexp.matchedLength() > limit)
                break;

            TextCursor ret(this, from + index, from + index + regexp.matchedLength());
            Q_ASSERT(ret.selectedText() == regexp.capturedTexts().first());
            return ret;
        }
        if (progressInterval != 0 && qAbs(it.position() - lastProgress) >= progressInterval) {
            const qreal progress = qAbs<int>(static_cast<qreal>(it.position() - initialPos)) / static_cast<qreal>(maxFindLength);
            emit findProgress(static_cast<int>(progress * 100.0), it.position());
            if (d->findState == TextDocumentPrivate::AbortFind) {
                d->findState = TextDocumentPrivate::NotFinding;
                break;
            }
            lastProgress = it.position();
        }
    } while (ok);
    return TextCursor();
}

TextCursor TextDocument::find(const QString &in, const TextCursor &cursor, FindMode flags) const
{
    if (in.isEmpty())
        return TextCursor();

    const bool reverse = flags & FindBackward;
    const bool caseSensitive = flags & FindCaseSensitively;
    const bool wholeWords = flags & FindWholeWords;

    int pos;
    int limit;
    ::initFind(cursor, reverse, &pos, &limit);

    if (pos == d->documentSize) {
        if (!reverse)
            return TextCursor();
        --pos;
    }


    // ### what if one searches for a string with non-word characters in it and FindWholeWords?
    const TextDocumentIterator::Direction direction = (reverse ? TextDocumentIterator::Left : TextDocumentIterator::Right);
    QString word = caseSensitive ? in : in.toLower();
    if (reverse) {
        QChar *data = word.data();
        const int size = word.size();
        for (int i=0; i<size / 2; ++i) {
            qSwap(data[i], data[size - 1 - i]);
        }
    }
    TextDocumentIterator it(d, pos);
    if (reverse) {
        it.setMinBoundary(limit);
    } else {
        it.setMaxBoundary(limit);
    }
    if (!caseSensitive)
        it.setConvertToLowerCase(true);

    bool ok = true;
    QChar ch = it.current();
    int wordIndex = 0;
    int progressInterval = 0;
    int lastProgress = pos;
    const int initialPos = pos;
    int maxFindLength = 0;
    const FindScope scope(flags & FindAllowInterrupt ? &d->findState : 0);
    if (flags & FindAllowInterrupt) {
        progressInterval = qMax<int>(1, (reverse
                                         ? (static_cast<qreal>(pos) / 100.0)
                                         : (static_cast<qreal>(d->documentSize) - static_cast<qreal>(pos)) / 100.0));
        maxFindLength = (reverse ? pos : d->documentSize - pos);
    }
    do {
        if (progressInterval != 0 && qAbs(it.position() - lastProgress) >= progressInterval) {
            const qreal progress = qAbs<int>(static_cast<qreal>(it.position() - initialPos)) / static_cast<qreal>(maxFindLength);
            emit findProgress(static_cast<int>(progress * 100.0), it.position());
            if (d->findState == TextDocumentPrivate::AbortFind) {
                break;
            }
            lastProgress = it.position();
        }

        bool found = ch == word.at(wordIndex);
        if (found && wholeWords && (wordIndex == 0 || wordIndex == word.size() - 1)) {
            const uint requiredBounds = ((wordIndex == 0) != reverse) ? TextDocumentIterator::Left : TextDocumentIterator::Right;
            const uint bounds = d->wordBoundariesAt(it.position());
            if (requiredBounds & ~bounds) {
                found = false;
            }
        }
        if (found) {
            if (++wordIndex == word.size()) {
                break;
            }
        } else if (wordIndex != 0) {
            wordIndex = 0;
            continue;
        }
        ch = it.nextPrev(direction, ok);
    } while (ok);

    if (ok && wordIndex == word.size()) {
        int pos = it.position() - (reverse ? 0 : word.size() - 1);
        // the iterator reads one past the last matched character so we have to account for that here
        TextCursor ret(this, pos, pos + wordIndex);
        return ret;
    }

    return TextCursor();
}

TextCursor TextDocument::find(const QChar &chIn, const TextCursor &cursor, FindMode flags) const
{
    if (flags & FindWholeWords) {
        qWarning("FindWholeWords makes not sense searching for characters");
    }

    const bool reverse = flags & FindBackward;
    int pos;
    int limit;
    ::initFind(cursor, reverse, &pos, &limit);
    if (pos == d->documentSize) {
        if (!reverse)
            return TextCursor();
        --pos;
    }
    Q_ASSERT(pos >= 0 && pos <= d->documentSize);

    const bool caseSensitive = flags & FindCaseSensitively;
    const QChar ch = (caseSensitive ? chIn : chIn.toLower());
    TextDocumentIterator it(d, pos);
    if (reverse) {
        it.setMinBoundary(limit);
    } else {
        it.setMaxBoundary(limit);
    }
    const TextDocumentIterator::Direction dir = (reverse
                                                 ? TextDocumentIterator::Left
                                                 : TextDocumentIterator::Right);
    int lastProgress = pos;
    const int initialPos = pos;
    int maxFindLength = 0;
    int progressInterval = 0;
    const FindScope scope(flags & FindAllowInterrupt ? &d->findState : 0);
    if (flags & FindAllowInterrupt) {
        progressInterval = qMax<int>(1, (reverse
                                         ? (static_cast<qreal>(pos) / 100.0)
                                         : (static_cast<qreal>(d->documentSize) - static_cast<qreal>(pos)) / 100.0));
        maxFindLength = (reverse ? pos : d->documentSize - pos);
    }

    QChar c = it.current();
    bool ok = true;
    do {
        if ((caseSensitive ? c : c.toLower()) == ch) {
            TextCursor ret(this, it.position(), it.position() + 1);
            return ret;
        }
        c = it.nextPrev(dir, ok);
        if (progressInterval != 0 && qAbs(it.position() - lastProgress) >= progressInterval) {
            const qreal progress = qAbs<int>(static_cast<qreal>(it.position() - initialPos)) / static_cast<qreal>(maxFindLength);
            emit findProgress(static_cast<int>(progress * 100.0), it.position());
            if (d->findState == TextDocumentPrivate::AbortFind) {
                d->findState = TextDocumentPrivate::NotFinding;
                break;
            }
            lastProgress = it.position();
        }

    } while (ok);

    return TextCursor();
}

bool TextDocument::insert(int pos, const QString &string)
{
#ifdef QT_DEBUG
    Q_ASSERT(d->iterators.isEmpty());
#endif
    Q_ASSERT(pos >= 0 && pos <= d->documentSize);
    if (string.isEmpty())
        return false;

    const bool undoAvailable = isUndoAvailable();
    DocumentCommand *cmd = 0;
    if (!d->ignoreUndoRedo && d->undoRedoEnabled && d->cursorCommand) { // can only undo commands from
        d->clearRedo();
        if (!d->undoRedoStack.isEmpty()
            && d->undoRedoStack.last()->type == DocumentCommand::Inserted
            && d->undoRedoStack.last()->position + d->undoRedoStack.last()->text.size() == pos) {
            d->undoRedoStack.last()->text += string;
        } else {
            cmd = new DocumentCommand(DocumentCommand::Inserted, pos, string);
            if (!d->modified)
                d->modifiedIndex = d->undoRedoStackCurrent;
            emit d->undoRedoCommandInserted(cmd);
            d->undoRedoStack.append(cmd);
            ++d->undoRedoStackCurrent;
            Q_ASSERT(d->undoRedoStackCurrent == d->undoRedoStack.size());
        }
    }
    d->modified = true;

    Chunk *c;
    int offset;
    c = d->chunkAt(pos, &offset);
//    qDebug() << c << (c == d->last) << (c == d->first) <<  offset << c->size() << d->chunkSize;
    if (c == d->last && offset == c->size() && c->size() >= d->chunkSize) {
        Chunk *chunk = new Chunk;
        c->next = chunk;
        chunk->previous = c;
        d->last = chunk;
        offset = 0;
        chunk->data = string;
        d->documentSize += string.size();
        if (d->options & SwapChunks) {
            if (c->previous) {
                d->swapOutChunk(c->previous);
            }
        }
        c = chunk;
    } else {
        d->instantiateChunk(c);
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
        if (c == d->cachedChunk) {
            d->cachedChunkData.clear(); // avoid detach when inserting
        }
#endif
        c->data.insert(offset, string);
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
        if (c == d->cachedChunk) {
            d->cachedChunkData = c->data;
        } else if (pos <= d->cachedChunkPos) {
            Q_ASSERT(d->cachedChunk);
            d->cachedChunkPos += string.size();
        }
#endif
#ifndef NO_TEXTDOCUMENT_READ_CACHE
        if (pos <= d->cachePos) {
            d->cachePos += string.size();
        } else if (pos < d->cachePos + d->cache.size()) {
            d->cachePos = -1;
            d->cache.clear();
        }
#endif

        TextSection *s = d->sectionAt(pos, 0);
        if (s && s->position() != pos) {
            s->d.size += string.size();
        }

        d->documentSize += string.size();

        foreach(TextCursorSharedPrivate *cursor, d->textCursors) {
            if (cursor->position >= pos)
                cursor->position += string.size();
            if (cursor->anchor >= pos)
                cursor->anchor += string.size();
        }
        foreach(TextSection *section, d->getSections(pos, -1, 0, 0)) {
            section->d.position += string.size();
        }

        if (d->hasChunksWithLineNumbers && c->firstLineIndex != -1) {
            const int extraLines = string.count(QLatin1Char('\n'));
            if (extraLines != 0) {
#ifdef TEXTDOCUMENT_LINENUMBER_CACHE
                c->lineNumbers.clear();
                // ### could be optimized
#else
                Q_ASSERT(c->lines != -1);
                c->lines += extraLines;
#endif
                c = c->next;
                while (c) {
                    if (c->firstLineIndex != -1) {
//                     qDebug() << "changing chunk number" << d->chunkIndex(c)
//                              << "starting with" << d->chunkData(c, -1).left(5)
//                              << "from" << c->firstLineIndex << "to" << (c->firstLineIndex + extraLines);
                        c->firstLineIndex += extraLines;
                    }
                    c = c->next;
                }
            }
        }
    }

    emit charactersAdded(pos, string.size());
    emit documentSizeChanged(d->documentSize);
    if (isUndoAvailable() != undoAvailable) {
        emit undoAvailableChanged(!undoAvailable);
    }
    if (cmd)
        emit d->undoRedoCommandFinished(cmd);

    emit textChanged();
    return true;
}

static inline int count(const QString &string, int from, int size, const QChar &ch)
{
    Q_ASSERT(from + size <= string.size());
    const ushort needle = ch.unicode();
    const ushort *haystack = string.utf16() + from;
    int num = 0;
    for (int i=0; i<size; ++i) {
        if (*haystack++ == needle)
            ++num;
    }
//    Q_ASSERT(string.mid(from, size).count(ch) == num);
    return num;
}

void TextDocument::remove(int pos, int size)
{
#ifdef QT_DEBUG
    Q_ASSERT(d->iterators.isEmpty());
#endif
    Q_ASSERT(pos >= 0 && pos + size <= d->documentSize);
    Q_ASSERT(size >= 0);

    if (size == 0)
        return;

    DocumentCommand *cmd = 0;
    const bool undoAvailable = isUndoAvailable();
    if (!d->ignoreUndoRedo && d->undoRedoEnabled && d->cursorCommand) {
        d->clearRedo();
        if (!d->undoRedoStack.isEmpty()
            && d->undoRedoStack.last()->type == DocumentCommand::Removed
            && d->undoRedoStack.last()->position == pos + size) {
            d->undoRedoStack.last()->text.prepend(read(pos, size));
            d->undoRedoStack.last()->position -= size;
        } else {
            cmd = new DocumentCommand(DocumentCommand::Removed, pos, read(pos, size));
            if (!d->modified)
                d->modifiedIndex = d->undoRedoStackCurrent;
            emit d->undoRedoCommandInserted(cmd);
            d->undoRedoStack.append(cmd);
            ++d->undoRedoStackCurrent;
            Q_ASSERT(d->undoRedoStackCurrent == d->undoRedoStack.size());
        }
    }
    d->modified = true;

    int toRemove = size;
    int newLinesRemoved = 0;
    while (toRemove > 0) {
        int offset;
        Chunk *c = d->chunkAt(pos, &offset);
        if (offset == 0 && toRemove >= c->size()) {
            toRemove -= c->size();
            if (d->hasChunksWithLineNumbers) {
                newLinesRemoved += d->chunkData(c, pos).count('\n');
                // ### should use the QVector<int> stuff if possible
            }
            d->removeChunk(c);
        } else {
            d->instantiateChunk(c);
            const int removed = qMin(toRemove, c->size() - offset);
            if (d->hasChunksWithLineNumbers) {
                const int tmp = ::count(c->data, offset, removed, QLatin1Char('\n'));
                newLinesRemoved += tmp;
#ifdef TEXTDOCUMENT_LINENUMBER_CACHE
                if (tmp > 0)
                    c->lineNumbers.clear();
                    // ### Could clear only the parts that need to be cleared really
#endif
            }
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
//            qDebug() << "removing from" << c << d->cachedChunk;
            if (d->cachedChunk == c) {
                d->cachedChunkData.clear();
            }
#endif
            c->data.remove(offset, removed);
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
            if (d->cachedChunk == c)
                d->cachedChunkData = c->data;
#endif


            toRemove -= removed;
        }
    }

    foreach(TextCursorSharedPrivate *cursor, d->textCursors) {
        if (cursor->position >= pos)
            cursor->position -= qMin(size, cursor->position - pos);
        if (cursor->anchor >= pos)
            cursor->anchor -= qMin(size, cursor->anchor - pos);
    }

    QList<TextSection*> s = d->getSections(pos, -1, 0, 0);
    foreach(TextSection *section, s) {
        const QPair<int, int> intersection = ::intersection(pos, size, section->position(), section->size());
        if (intersection.second == section->size()) {
            delete section;
        } else {
            if (intersection.first == section->position() || intersection.first == -1)
                section->d.position -= size;
            section->d.size -= intersection.second;
        }
    }

    d->documentSize -= size;
#ifndef NO_TEXTDOCUMENT_READ_CACHE
    if (pos + size < d->cachePos) {
        d->cachePos -= size;
    } else if (pos <= d->cachePos + d->cache.size()) {
        d->cachePos = -1;
        d->cache.clear();
    }
#endif
    if (newLinesRemoved > 0) {
        Q_ASSERT(d->hasChunksWithLineNumbers);
        int offset;
        Chunk *c = d->chunkAt(pos, &offset);
        if (offset != 0)
            c = c->next;
        while (c) {
            if (c->firstLineIndex != -1) {
                c->firstLineIndex -= newLinesRemoved;
            }
            c = c->next;
        }
    }


    emit charactersRemoved(pos, size);
    emit documentSizeChanged(d->documentSize);
    if (isUndoAvailable() != undoAvailable) {
        emit undoAvailableChanged(!undoAvailable);
    }
    if (cmd)
        emit d->undoRedoCommandFinished(cmd);
    emit textChanged();
}

void TextDocument::takeTextSection(TextSection *section)
{
    Q_ASSERT(section);
    Q_ASSERT(section->document() == this);

    QList<TextSection*>::iterator first = qLowerBound(d->sections.begin(), d->sections.end(), section, compareTextSection);
    Q_ASSERT(first != d->sections.end());
    const QList<TextSection*>::iterator last = qUpperBound(d->sections.begin(), d->sections.end(), section, compareTextSection);

    while (first != last) {
        if (*first == section) {
            emit sectionRemoved(section);
            d->sections.erase(first);
            break;
        }
        ++first;
    }

    // Moved this to the end as the slots called by sectionRemoved (presently) rely
    // on section->d.document to be valid.
    section->d.textEdit = 0;
    section->d.document = 0;
}

QList<TextSection*> TextDocument::sections(int pos, int size, TextSection::TextSectionOptions flags) const
{
    return d->getSections(pos, size, flags, 0);
}

void TextDocument::insertTextSection(TextSection *section)
{
    Q_ASSERT(!d->sections.contains(section));
    QList<TextSection*>::iterator it = qLowerBound<QList<TextSection*>::iterator>(d->sections.begin(), d->sections.end(),
                                                                                  section, compareTextSection);
    d->sections.insert(it, section);
    emit sectionAdded(section);
}

TextSection *TextDocument::insertTextSection(int pos, int size,
                                             const QTextCharFormat &format, const QVariant &data)
{
    Q_ASSERT(pos >= 0);
    Q_ASSERT(size >= 0);
    Q_ASSERT(pos < d->documentSize);

    TextSection *l = new TextSection(pos, size, this, format, data);
    QList<TextSection*>::iterator it = qLowerBound<QList<TextSection*>::iterator>(d->sections.begin(), d->sections.end(), l, compareTextSection);
    d->sections.insert(it, l);
    emit sectionAdded(l);
    return l;
}

bool TextDocument::abortSave()
{
    if (d->saveState == TextDocumentPrivate::Saving) {
        d->saveState = TextDocumentPrivate::AbortSave;
        return true;
    }
    return false;
}

bool TextDocument::abortFind() const
{
    if (d->findState == TextDocumentPrivate::Finding) {
        d->findState = TextDocumentPrivate::AbortFind;
        return true;
    }
    return false;
}


QChar TextDocument::readCharacter(int pos) const
{
    if (pos == d->documentSize)
        return QChar();
    Q_ASSERT(pos >= 0 && pos < d->documentSize);
#ifndef NO_TEXTDOCUMENT_READ_CACHE
#ifdef DEBUG_CACHE_HITS
    static int hits = 0;
    static int misses = 0;
#endif
    if (pos >= d->cachePos && pos < d->cachePos + d->cache.size()) {
#ifdef DEBUG_CACHE_HITS
        qWarning() << "readCharacter hits" << ++hits << "misses" << misses;
#endif
        return d->cache.at(pos - d->cachePos);
    }
#ifdef DEBUG_CACHE_HITS
    qWarning() << "readCharacter hits" << hits << "misses" << ++misses;
#endif
#endif

    int offset;
    Chunk *c = d->chunkAt(pos, &offset);
    return d->chunkData(c, pos - offset).at(offset);
}

void TextDocument::setText(const QString &text)
{
    // ### could optimize this to avoid detach and copy if text.size() <= chunkSize
    // ### but is it worth it?
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QTextStream ts(&buffer);
    if (d->textCodec)
        ts.setCodec(d->textCodec);
    ts << text;
    buffer.close();
    buffer.open(QIODevice::ReadOnly);
    const bool ret = load(&buffer, LoadAll);
    Q_ASSERT(ret);
    Q_UNUSED(ret);
}

int TextDocument::chunkSize() const
{
    return d->chunkSize;
}

void TextDocument::setChunkSize(int size)
{
    Q_ASSERT(d->chunkSize > 0);
    d->chunkSize = size;
}

int TextDocument::currentMemoryUsage() const
{
    Chunk *c = d->first;
    int used = 0;
    while (c) {
        used += c->data.size() * sizeof(QChar);
        c = c->next;
    }
    return used;
}

void TextDocument::undo()
{
    if (!isUndoAvailable())
        return;
    d->undoRedo(true);
}

void TextDocument::redo()
{
    if (!isRedoAvailable())
        return;
    d->undoRedo(false);
}

bool TextDocument::isUndoRedoEnabled() const
{
    return d->undoRedoEnabled;
}

void TextDocument::setUndoRedoEnabled(bool enable)
{
    if (enable == d->undoRedoEnabled)
        return;
    d->undoRedoEnabled = enable;
    if (!enable) {
        qDeleteAll(d->undoRedoStack);
        d->undoRedoStack.clear();
        d->undoRedoStackCurrent = 0;
    }
}

bool TextDocument::isUndoAvailable() const
{
    return d->undoRedoStackCurrent > 0;
}

bool TextDocument::isRedoAvailable() const
{
    return d->undoRedoStackCurrent < d->undoRedoStack.size();
}

bool TextDocument::isModified() const
{
    return d->modified;
}

void TextDocument::setModified(bool modified)
{
    if (d->modified == modified)
        return;

    d->modified = modified;
    if (!modified) {
        d->modifiedIndex = d->undoRedoStackCurrent;
    } else {
        d->modifiedIndex = -1;
    }

    emit modificationChanged(modified);
}

int TextDocument::lineNumber(int position) const
{
    d->hasChunksWithLineNumbers = true;
    int offset;
    Chunk *c = d->chunkAt(position, &offset);
    d->updateChunkLineNumbers(c, position - offset);
    Q_ASSERT(c->firstLineIndex != -1);
    Q_ASSERT(d->first->firstLineIndex != -1);
    const int extra = (offset == 0 ? 0 : d->countNewLines(c, position - offset, offset));
#ifdef QT_DEBUG
    if (position <= 16000) {
        const QString data = read(0, position);
        // if we're on a newline it shouldn't count so we do read(0, position)
        // not read(0, position + 1);
        const int count = data.count(QLatin1Char('\n'));
        if (count != c->firstLineIndex + extra) {
            qDebug() << "TextDocument::lineNumber returns" << (c->firstLineIndex + extra)
                     << "should have returned" << (count + 1)
                     << "for index" << position;
        }
    }
#endif
    return c->firstLineIndex + extra;
}

int TextDocument::columnNumber(int position) const
{
    TextCursor cursor(this, position);
    return cursor.isNull() ? -1 : cursor.columnNumber();
}

int TextDocument::lineNumber(const TextCursor &cursor) const
{
    return cursor.document() == this ? lineNumber(cursor.position()) : -1;
}

int TextDocument::columnNumber(const TextCursor &cursor) const
{
    return cursor.document() == this ? cursor.columnNumber() : -1;
}

void TextDocument::setOptions(Options opt)
{
    d->options = opt;
}

TextDocument::Options TextDocument::options() const
{
    return d->options;
}

bool TextDocument::isWordCharacter(const QChar &ch, int /*index*/) const
{
    // from qregexp.
    return ch.isLetterOrNumber() || ch.isMark() || ch == QLatin1Char('_');
}

QString TextDocument::swapFileName(Chunk *chunk)
{
    QString file = QDesktopServices::storageLocation(QDesktopServices::TempLocation);
    file.reserve(file.size() + 24);
    QTextStream ts(&file);
    ts << QLatin1Char('/') << QLatin1String("lte_") << chunk << QLatin1Char('_')
       << this << QLatin1Char('_') << QCoreApplication::applicationPid();

    return file;
}

// --- TextDocumentPrivate ---

Chunk *TextDocumentPrivate::chunkAt(int p, int *offset) const
{
    Q_ASSERT(p <= documentSize);
    Q_ASSERT(p >= 0);
    Q_ASSERT(first);
    Q_ASSERT(last);
    if (p == documentSize) {
        if (offset)
            *offset = last->size();
        return last;
    }
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
    Q_ASSERT(!cachedChunk || cachedChunkPos != -1);
    if (cachedChunk && p >= cachedChunkPos && p < cachedChunkPos + cachedChunkData.size()) {
        if (offset)
            *offset = p - cachedChunkPos;
        return cachedChunk;
    }
#endif
    int pos = p;
    Chunk *c = first;

    forever {
        const int size = c->size();
        if (pos < size) {
            break;
        }
        pos -= size;
        c = c->next;
        Q_ASSERT(c);
    }

    if (offset)
        *offset = pos;

    Q_ASSERT(c);
    return c;
}

void TextDocumentPrivate::clearRedo()
{
    const bool doEmit = undoRedoStackCurrent < undoRedoStack.size();
    while (undoRedoStackCurrent < undoRedoStack.size()) {
        DocumentCommand *cmd = undoRedoStack.takeLast();
        emit undoRedoCommandRemoved(cmd);
        delete cmd;
    }
    if (doEmit) {
        emit q->redoAvailableChanged(false);
    }
}

/* Evil double meaning of pos here. If it's -1 we don't cache it. */
QString TextDocumentPrivate::chunkData(const Chunk *chunk, int chunkPos) const
{
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
#ifdef DEBUG_CACHE_HITS
    static int hits = 0;
    static int misses = 0;
#endif
    if (chunk == cachedChunk) {
#ifdef DEBUG_CACHE_HITS
        qWarning() << "chunkData hits" << ++hits << "misses" << misses;
#endif
        Q_ASSERT(cachedChunkData.size() == chunk->size());
        return cachedChunkData;
    } else
#endif
    if (chunk->from == -1) {
        return chunk->data;
    } else if (!device && chunk->swap.isEmpty()) {
        // Can only happen if the device gets deleted behind our back when in Sparse mode
        return QString().fill(QLatin1Char(' '), chunk->size());
    } else {
        QFile file;
        QIODevice *dev = device.data();
        if (!chunk->swap.isEmpty()) {
            file.setFileName(chunk->swap);
            if (!file.open(QIODevice::ReadOnly)) {
                qWarning("TextDocumentPrivate::chunkData() Can't open file for reading '%s'", qPrintable(chunk->swap));
                return QString().fill(QLatin1Char(' '), chunk->size());
            }
            dev = &file;
        }
        QTextStream ts(dev);
        if (textCodec)
            ts.setCodec(textCodec);
//         if (!chunk->swap.isEmpty()) {
//             qDebug() << "reading stuff from swap" << chunk << chunk->from << chunk->size() << chunk->swap;
//         }
        ts.seek(chunk->from);
        const QString data = ts.read(chunk->length);
        Q_ASSERT(data.size() == chunk->size());
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
#ifdef DEBUG_CACHE_HITS
        qWarning() << "chunkData hits" << hits << "misses" << ++misses;
#endif
        if (chunkPos != -1) {
            cachedChunk = const_cast<Chunk*>(chunk);
            cachedChunkData = data;
            cachedChunkPos = chunkPos;
#ifdef QT_DEBUG
            if (chunkPos != chunk->pos()) {
                qWarning() << chunkPos << chunk->pos();
            }
            Q_ASSERT(chunkPos == chunk->pos());
#endif
        }
#endif
        return data;
    }
}

int TextDocumentPrivate::chunkIndex(const Chunk *c) const
{
    int index = 0;
    while (c->previous) {
        ++index;
        c = c->previous;
    }
    return index;
}

void TextDocumentPrivate::instantiateChunk(Chunk *chunk)
{
    if (chunk->from == -1 && chunk->swap.isEmpty())
        return;
    chunk->data = chunkData(chunk, -1);
//    qDebug() << "instantiateChunk" << chunk << chunk->swap;
    chunk->swap.clear();
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
    // Don't want to cache this chunk since it's going away. If it
    // already was cached then sure, but otherwise don't
    if (chunk == cachedChunk) {
        cachedChunk = 0;
        cachedChunkPos = -1;
        cachedChunkData.clear();
    }
#endif
    chunk->from = chunk->length = -1;
}

void TextDocumentPrivate::removeChunk(Chunk *c)
{
    Q_ASSERT(c);
    if (c == first) {
        first = c->next;
    } else {
        c->previous->next = c->next;
    }
    if (c == last) {
        last = c->previous;
    } else {
        c->next->previous = c->previous;
    }
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
    if (c == cachedChunk) {
        cachedChunk = 0;
        cachedChunkPos = -1;
        cachedChunkData.clear();
    }
#endif
    if (!first) {
        Q_ASSERT(!last);
        first = last = new Chunk;
    }

    delete c;
}

void TextDocumentPrivate::undoRedo(bool undo)
{
    const bool undoWasAvailable = q->isUndoAvailable();
    const bool redoWasAvailable = q->isRedoAvailable();
    DocumentCommand *cmd = undoRedoStack.at(undo
                                            ? --undoRedoStackCurrent
                                            : undoRedoStackCurrent++);
    const bool was = ignoreUndoRedo;
    ignoreUndoRedo = true;
    Q_ASSERT(cmd->type != DocumentCommand::None);
    if ((cmd->type == DocumentCommand::Inserted) == undo) {
        q->remove(cmd->position, cmd->text.size());
    } else {
        q->insert(cmd->position, cmd->text);
    }
    emit undoRedoCommandTriggered(cmd, undo);
    ignoreUndoRedo = was;

    if ((cmd->joinStatus == DocumentCommand::Backward && undo)
        || (cmd->joinStatus == DocumentCommand::Forward && !undo)) {
        undoRedo(undo);
    }
    if (undoWasAvailable != q->isUndoAvailable())
        emit q->undoAvailableChanged(!undoWasAvailable);
    if (redoWasAvailable != q->isRedoAvailable())
        emit q->redoAvailableChanged(!redoWasAvailable);
    if (modified && modifiedIndex == undoRedoStackCurrent)
        q->setModified(false);
}

QString TextDocumentPrivate::wordAt(int position, int *start) const
{
    TextDocumentIterator from(this, position);
    if (!q->isWordCharacter(from.current(), position)) {
        if (start)
            *start = -1;
        return QString();
    }

    while (from.hasPrevious()) {
        const QChar ch = from.previous();
        if (!q->isWordCharacter(ch, from.position())) {
            // ### could just peek rather than going one too far
            from.next();
            break;
        }
    }
    TextDocumentIterator to(this, position);
    while (to.hasNext()) {
        const QChar ch = to.next();
        if (!q->isWordCharacter(ch, to.position()))
            break;
    }

    if (start)
        *start = from.position();
    return q->read(from.position(), to.position() - from.position());
}

QString TextDocumentPrivate::paragraphAt(int position, int *start) const
{
    const QLatin1Char newline('\n');
    TextDocumentIterator from(this, position);
    while (from.hasPrevious() && from.previous() != newline)
        ;
    TextDocumentIterator to(this, position);
    while (to.hasNext() && to.next() != newline)
        ;
    if (start)
        *start = from.position();
    return q->read(from.position(), to.position() - from.position());
}

uint TextDocumentPrivate::wordBoundariesAt(int pos) const
{
    Q_ASSERT(pos >= 0 && pos < documentSize);
    uint ret = 0;
    if (pos == 0 || !q->isWordCharacter(q->readCharacter(pos - 1), pos - 1)) {
        ret |= TextDocumentIterator::Left;
    }
    if (pos + 1 == documentSize || !q->isWordCharacter(q->readCharacter(pos + 1), pos + 1)) {
        ret |= TextDocumentIterator::Right;
    }
    return ret;
}


void TextDocumentPrivate::joinLastTwoCommands()
{
    Q_ASSERT(!q->isRedoAvailable());
    Q_ASSERT(undoRedoStack.size() >= 2);
    undoRedoStack.last()->joinStatus = DocumentCommand::Backward;
    undoRedoStack.at(undoRedoStack.size() - 2)->joinStatus = DocumentCommand::Forward;
}

void TextDocumentPrivate::updateChunkLineNumbers(Chunk *c, int chunkPos) const
{
    Q_ASSERT(c);
    if (c->firstLineIndex == -1) {
        Chunk *cc = c;
        int pos = chunkPos;
        while (cc->previous && cc->previous->firstLineIndex == -1) {
            pos -= cc->size();
            cc = cc->previous;
        }
        // cc is at the first chunk that has firstLineIndex != -1
        Q_ASSERT(!cc->previous || cc->previous->firstLineIndex != -1);
        Q_ASSERT(cc->firstLineIndex == 1 || cc->firstLineIndex == -1);
        // special case for first chunk
        do {
            const int size = cc->size();
            if (!cc->previous) {
                cc->firstLineIndex = 0;
            } else {
                const int prevSize = cc->previous->size();
                const int lineCount = countNewLines(cc->previous, pos - prevSize, prevSize);
                Q_ASSERT(cc->previous->firstLineIndex != -1);
                cc->firstLineIndex = cc->previous->firstLineIndex + lineCount;
            }
            pos += size;
            cc = cc->next;
        } while (cc && cc != c->next);
        countNewLines(c, chunkPos, c->size());
    }
    Q_ASSERT(c->firstLineIndex != -1);
}

static inline QList<int> dumpNewLines(const QString &string, int from, int size)
{
    QList<int> ret;
    for (int i=from; i<from + size; ++i) {
        if (string.at(i) == QLatin1Char('\n'))
            ret.append(i);
    }
    return ret;
}


int TextDocumentPrivate::countNewLines(Chunk *c, int chunkPos, int size) const
{
//     qDebug() << "CALLING countNewLines on" << chunkIndex(c) << chunkPos << size;
//     qDebug() << (c == first) << c->firstLineIndex << chunkPos << size
//              << c->size();
    int ret = 0;
#ifndef TEXTDOCUMENT_LINENUMBER_CACHE
    if (size == c->size()) {
        if (c->lines == -1) {
            c->lines = ::count(chunkData(c, chunkPos), 0, size, QLatin1Char('\n'));
//             qDebug() << "counting" << c->lines << "in" << chunkIndex(c)
//                      << "Size" << size << "chunkPos" << chunkPos;
        }
        ret = c->lines;
    } else {
        ret = ::count(chunkData(c, chunkPos), 0, size, QLatin1Char('\n'));
    }
#else
//     qDebug() << size << ret << c->lineNumbers << chunkPos
//              << dumpNewLines(chunkData(c, chunkPos), 0, c->size());
    static const int lineNumberCacheInterval = TEXTDOCUMENT_LINENUMBER_CACHE_INTERVAL;
    if (c->lineNumbers.isEmpty()) {
        const QString data = chunkData(c, chunkPos);
        const int s = data.size();
        c->lineNumbers.fill(0, (s + lineNumberCacheInterval - 1) / lineNumberCacheInterval);
//        qDebug() << data.size() << c->lineNumbers.size() << lineNumberCacheInterval;

        for (int i=0; i<s; ++i) {
            if (data.at(i) == QLatin1Char('\n')) {
                ++c->lineNumbers[i / lineNumberCacheInterval];
//                 qDebug() << "found one at" << i << "put it in" << (i / lineNumberCacheInterval)
//                          << "chunkPos" << chunkPos;
                if (i < size)
                    ++ret;
            }
        }
    } else {
        for (int i=0; i<c->lineNumbers.size(); ++i) {
            if (i * lineNumberCacheInterval > size) {
                break;
            } else if (c->lineNumbers.at(i) == 0) {
                // nothing in this area
                continue;
            } else if ((i + 1) * lineNumberCacheInterval > size) {
                ret += ::count(chunkData(c, chunkPos), i * lineNumberCacheInterval,
                               size - i * lineNumberCacheInterval, QChar('\n'));
                // partly
                break;
            } else {
                ret += c->lineNumbers.at(i);
            }
        }
    }
#endif
    return ret;
}

void TextDocumentPrivate::swapOutChunk(Chunk *c)
{
    if (!c->swap.isEmpty())
        return;
    Q_ASSERT(!c->data.isEmpty());
    c->from = 0;
    c->length = c->data.size();
    c->swap = q->swapFileName(c);
    QFile file(c->swap);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("TextDocumentPrivate::chunkData() Can't open file for writing '%s'", qPrintable(c->swap));
        c->swap.clear();
        return;
    }
    QTextStream ts(&file);
    if (textCodec)
        ts.setCodec(textCodec);
    ts << c->data;
    c->data.clear();
#ifndef NO_TEXTDOCUMENT_CHUNK_CACHE
    // ### do I want to do this?
    if (cachedChunk == c) {
        cachedChunk = 0;
        cachedChunkPos = -1;
        cachedChunkData.clear();
    }
#endif
}

static inline bool match(int pos, int left, int size)
{
    return pos >= left && pos < left + size;
}

static inline bool match(int pos, int size, const TextSection *section, TextSection::TextSectionOptions flags)
{
    const int sectionPos = section->position();
    const int sectionSize = section->size();

    if (::match(sectionPos, pos, size) && ::match(sectionPos + sectionSize - 1, pos, size)) {
        return true;
    } else if (flags & TextSection::IncludePartial) {
        const int boundaries[] = { pos, pos + size - 1 };
        for (int i=0; i<2; ++i) {
            if (::match(boundaries[i], sectionPos, sectionSize))
                return true;
        }
    }
    return false;
}

static inline void filter(QList<TextSection*> &sections, const TextEdit *filter)
{
    if (filter) {
        for (int i=sections.size() - 1; i>=0; --i) {
            if (!::matchSection(sections.at(i), filter))
                sections.removeAt(i);
        }
    }
}

QList<TextSection*> TextDocumentPrivate::getSections(int pos, int size, TextSection::TextSectionOptions flags, const TextEdit *filter) const
{
    if (size == -1)
        size = documentSize - pos;
    QList<TextSection*> ret;
    if (pos == 0 && size == documentSize) {
        ret = sections;
        ::filter(ret, filter);
        return ret;
    }
    // binary search. TextSections are sorted in order of position
    if (sections.isEmpty()) {
        return ret;
    }

    const TextSection tmp(pos, size, static_cast<TextDocument*>(0), QTextCharFormat(), QVariant());
    QList<TextSection*>::const_iterator it = qLowerBound(sections.begin(), sections.end(), &tmp, compareTextSection);
    if (flags & TextSection::IncludePartial && it != sections.begin()) {
        QList<TextSection*>::const_iterator prev = it;
        do {
            if (::match(pos, size, *--prev, flags))
                ret.append(*prev);
        } while (prev != sections.begin());
    }
    while (it != sections.end()) {
        if (::match(pos, size, *it, flags)) {
            ret.append(*it);
        } else {
            break;
        }
        ++it;
    }
    ::filter(ret, filter);
    return ret;
}

void TextDocumentPrivate::textEditDestroyed(TextEdit *edit)
{
    QMutableListIterator<TextSection*> i(sections);
    while (i.hasNext()) {
        TextSection *section = i.next();
        if (section->textEdit() == edit) {
            section->d.document = 0;
            // Make sure we also remove it from the list of sections so it
            // isn't deleted in the TextDocument destructor too.
            i.remove();
            delete section;
        }
    }
}
