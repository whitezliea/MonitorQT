#ifndef CSVEXPORTWRITER_H
#define CSVEXPORTWRITER_H

#include <QString>
#include <QStringList>
#include <QVector>

namespace Monitor::Presentation::Export {

class CsvExportWriter
{
public:
    static bool write(
        const QString &path,
        const QStringList &headers,
        const QVector<QStringList> &rows,
        QString *errorMessage = nullptr);

private:
    static QString escape(const QString &value);
};

} // namespace Monitor::Presentation::Export

#endif // CSVEXPORTWRITER_H
