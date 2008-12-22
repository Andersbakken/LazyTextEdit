#ifndef TEXTDOCUMENT_H
#define TEXTDOCUMENT_H

#include <QObject>
#include <QString>
#include <QString>
#include <QPair>
#include <QEventLoop>
#include <QList>
#include <QVariant>
#include <QTextCharFormat>
#include "textcursor.h"

class Section
{
public:
    ~Section();
    QString text() const;
    int position() const { return d.position; }
    int size() const { return d.size; }
    QTextCharFormat format() const { return d.format; }
    void setFormat(const QTextCharFormat &format);
    QVariant data() const { return d.data; }
    void setData(const QVariant &data) { d.data = data; }
    TextDocument *document() const { return d.document; }
private:
    struct Data {
        Data(int p, int s, TextDocument *doc, const QTextCharFormat &f, const QVariant &d)
            : position(p), size(s), document(doc), format(f), data(d)
        {}
        int position, size;
        TextDocument *document;
        QTextCharFormat format;
        QVariant data;
    } d;

    Section(int pos, int size, TextDocument *doc, const QTextCharFormat &format = QTextCharFormat(),
            const QVariant &data = QVariant())
        : d(pos, size, doc, format, data)
    {}
    friend class TextDocument;
};

class Chunk;
class TextDocumentPrivate;
class TextDocument : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int documentSize READ documentSize)
    Q_PROPERTY(int chunkSize READ chunkSize WRITE setChunkSize)
    Q_PROPERTY(bool undoRedoEnabled READ isUndoRedoEnabled WRITE setUndoRedoEnabled)
    Q_PROPERTY(bool modified READ isModified WRITE setModified DESIGNABLE false)
    Q_ENUMS(DeviceMode)
public:
    TextDocument(QObject *parent = 0);
    ~TextDocument();

    enum DeviceMode {
        Sparse,
        LoadAll
    };

    bool load(QIODevice *device, DeviceMode mode = Sparse);
    bool load(const QString &fileName, DeviceMode mode = Sparse);
    void clear();
    DeviceMode deviceMode() const;

    void setText(const QString &text);
    QString read(int pos, int size) const;
    QChar readCharacter(int index) const;
    bool save(const QString &file);
    bool save(QIODevice *device);
    bool save();
    int documentSize() const;

    enum FindModeFlag {
        FindBackward        = 0x00001,
        FindCaseSensitively = 0x00002,
        FindWholeWords      = 0x00004
    };
    Q_DECLARE_FLAGS(FindMode, FindModeFlag);

    int chunkSize() const;
    void setChunkSize(int pos);

    bool isUndoRedoEnabled() const;
    void setUndoRedoEnabled(bool enable);

    QIODevice *device() const;

    TextCursor find(const QRegExp &rx, int pos = 0, FindMode flags = 0) const;
    TextCursor find(const QString &ba, int pos = 0, FindMode flags = 0) const;
    TextCursor find(const QChar &ch, int pos = 0, FindMode flags = 0) const;
    bool insert(int pos, const QString &ba);
    inline bool append(const QString &ba) { return insert(documentSize(), ba); }
    void remove(int pos, int size);
    enum SectionOption {
        IncludePartial = 0x01
    };
    Q_DECLARE_FLAGS(SectionOptions, SectionOption);

    QList<Section*> sections(int from = 0, int size = -1, SectionOptions opt = 0) const;
    inline Section *sectionAt(int pos) const { return sections(pos, 1, IncludePartial).value(0); }
    Section *insertSection(int pos, int size, const QTextCharFormat &format = QTextCharFormat(),
                           const QVariant &data = QVariant());
    void removeSection(Section *section);
    void takeSection(Section *section);
    int currentMemoryUsage() const;

    bool isUndoAvailable() const;
    bool isRedoAvailable() const;

    bool isModified() const { return false; }
    void setModified(bool modified) {}
public slots:
    void undo();
    void redo();
    bool abortSave();
signals:
    void foo(int pos, int size);
    void sectionAdded(Section *section);
    void sectionRemoved(Section *removed);
    void charactersAdded(int from, int count);
    void charactersRemoved(int from, int count);
    void saveProgress(int progress);
    void documentSizeChanged(int size);
    void undoAvailableChanged(bool on);
    void redoAvailableChanged(bool on);
    void modificationChanged(bool modified);
private:
    TextDocumentPrivate *d;
    friend class TextEdit;
    friend class TextCursor;
    friend class TextDocumentPrivate;
};

#endif
