#ifndef TEXTSECTION_H
#define TEXTSECTION_H

#include <QString>
#include <QTextCharFormat>
#include <QVariant>

class TextDocument;
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

#endif
