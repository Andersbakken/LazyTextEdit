#ifndef TEXTSECTION_H
#define TEXTSECTION_H

#include <QString>
#include <QTextCharFormat>
#include <QVariant>
#include <QCursor>

class TextDocument;
class TextEdit;
class TextSection
{
public:
    enum TextSectionOption {
        IncludePartial = 0x01
    };
    Q_DECLARE_FLAGS(TextSectionOptions, TextSectionOption);

    ~TextSection();
    QString text() const;
    int position() const { return d.position; }
    int size() const { return d.size; }
    QTextCharFormat format() const { return d.format; }
    void setFormat(const QTextCharFormat &format);
    QVariant data() const { return d.data; }
    void setData(const QVariant &data) { d.data = data; }
    TextDocument *document() const { return d.document; }
    QCursor cursor() const;
    void setCursor(const QCursor &cursor);
    void resetCursor();
    bool hasCursor() const;
private:
    struct Data {
        Data(int p, int s, TextDocument *doc, TextEdit *e, const QTextCharFormat &f, const QVariant &d)
            : position(p), size(s), document(doc), edit(e), format(f), data(d), hasCursor(false)
        {}
        int position, size;
        TextDocument *document;
        TextEdit *edit;
        QTextCharFormat format;
        QVariant data;
        QCursor cursor;
        bool hasCursor;
    } d;

    TextSection(int pos, int size, TextDocument *doc, const QTextCharFormat &format = QTextCharFormat(),
            const QVariant &data = QVariant())
        : d(pos, size, doc, 0, format, data)
    {}
    TextSection(int pos, int size, TextEdit *edit, const QTextCharFormat &format = QTextCharFormat(),
            const QVariant &data = QVariant())
        : d(pos, size, 0, edit, format, data)
    {}

    friend class TextDocument;
    friend class TextEdit;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TextSection::TextSectionOptions);


#endif
