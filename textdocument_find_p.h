#ifndef TEXTDOCUMENT_FIND_P_H
#define TEXTDOCUMENT_FIND_P_H

#include <QtCore>
#include "textdocument_p.h"
class Needle
{
public:
    Needle(uint flags)
    {
        d.flags = flags;
    }

    virtual ~Needle() {}
    enum ExtraFlag {
        MatchByLine = 0x10000
    };

    uint flags() const { return d.flags; }
    virtual bool addChar(const QChar &ch) = 0;
    virtual int addString(const QString &string)
    {
        const int size = string.size();
        for (int i=0; i<size; ++i) {
            if (addChar(string.at(i))) {
                return i;
            }
        }
        return -1;
    }
    virtual int matchedLength() const = 0;

private:
    struct Data {
        uint flags;
    } d;
};

class CharNeedle : public Needle
{
public:
    CharNeedle(uint flags, const QChar &ch);
    virtual bool addChar(const QChar &ch);
    virtual int matchedLength() const;
private:
    struct Data {
        QChar ch;
    } d;
};

class StringNeedle : public Needle
{
public:
    StringNeedle(uint flags, const QString &string);
    virtual bool addChar(const QChar &ch);
    virtual int matchedLength() const;
private:
    struct Data {
        QString string;
        int index;
    } d;
};

class RegExpNeedle : public Needle
{
public:
    RegExpNeedle(uint flags, const QRegExp &rx);
    virtual bool addChar(const QChar &ch);
    virtual int addString(const QString &string);
    virtual int matchedLength() const;
private:
    struct Data {
        QRegExp regExp;
    } d;
};



#endif
