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
#include <QTextCodec>
#include <QChar>
#include <QRegExp>
#include "textcursor.h"
#include "textsection.h"

class Chunk;
class TextDocumentPrivate;
class TextDocument : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int documentSize READ documentSize)
    Q_PROPERTY(int chunkCount READ chunkCount)
    Q_PROPERTY(int chunkSize READ chunkSize WRITE setChunkSize)
    Q_PROPERTY(bool undoRedoEnabled READ isUndoRedoEnabled WRITE setUndoRedoEnabled)
    Q_PROPERTY(bool modified READ isModified WRITE setModified DESIGNABLE false)
    Q_PROPERTY(bool undoAvailable READ isUndoAvailable NOTIFY undoAvailableChanged)
    Q_PROPERTY(bool redoAvailable READ isRedoAvailable NOTIFY redoAvailableChanged)
    Q_ENUMS(DeviceMode)
    Q_FLAGS(Options)
    Q_FLAGS(FindMode)

public:
    TextDocument(QObject *parent = 0);
    ~TextDocument();

    enum DeviceMode {
        Sparse,
        LoadAll
    };

    enum Option {
        NoOptions = 0x00,
        SwapChunks = 0x01,
        KeepTemporaryFiles = 0x02
    };
    Q_DECLARE_FLAGS(Options, Option);

    Options options() const;
    void setOptions(Options opt);
    inline void setOption(Option opt, bool on = true) { setOptions(on ? (options() | opt) : (options() &= ~opt)); }

    inline bool load(QIODevice *device, DeviceMode mode, const QByteArray &codecName)
    { return load(device, mode, QTextCodec::codecForName(codecName)); }
    inline bool load(const QString &fileName, DeviceMode mode, const QByteArray &codecName)
    { return load(fileName, mode, QTextCodec::codecForName(codecName)); }
    bool load(QIODevice *device, DeviceMode mode = Sparse, QTextCodec *codec = 0);
    bool load(const QString &fileName, DeviceMode mode = Sparse, QTextCodec *codec = 0);

    void clear();
    DeviceMode deviceMode() const;

    QTextCodec *textCodec() const;

    void setText(const QString &text);
    QString read(int pos, int size) const;
    QStringRef readRef(int pos, int size) const;
    QChar readCharacter(int index) const;
    bool save(const QString &file);
    bool save(QIODevice *device);
    bool save();
    int documentSize() const;
    int chunkCount() const;

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
    void remove(int pos, int size);

    QList<TextSection*> sections(int from = 0, int size = -1, TextSection::TextSectionOptions opt = 0) const;
    inline TextSection *sectionAt(int pos) const { return sections(pos, 1, TextSection::IncludePartial).value(0); }
    TextSection *insertTextSection(int pos, int size, const QTextCharFormat &format = QTextCharFormat(),
                                   const QVariant &data = QVariant());
    void insertTextSection(TextSection *section);
    void takeTextSection(TextSection *section);
    int currentMemoryUsage() const;

    bool isUndoAvailable() const;
    bool isRedoAvailable() const;

    bool isModified() const;

    int lineNumber(int position) const;
    int columnNumber(int position) const;
    int lineNumber(const TextCursor &cursor) const;
    int columnNumber(const TextCursor &cursor) const;
    virtual bool isWordCharacter(const QChar &ch, int index) const;
public slots:
    inline bool append(const QString &ba) { return insert(documentSize(), ba); }
    void setModified(bool modified);
    void undo();
    void redo();
    bool abortSave();
signals:
    void textChanged();
    void sectionAdded(TextSection *section);
    void sectionRemoved(TextSection *removed);
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
    friend class TextLayout;
    friend class TextSection;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextDocument::FindMode);
Q_DECLARE_OPERATORS_FOR_FLAGS(TextDocument::Options);

#endif
