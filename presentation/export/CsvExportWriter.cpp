#include "CsvExportWriter.h"

#include <QFile>
#include <QStringConverter>
#include <QTextStream>

namespace Monitor::Presentation::Export {

bool CsvExportWriter::write(
    const QString &path,
    const QStringList &headers,
    const QVector<QStringList> &rows,
    QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QChar(0xFEFF);
    stream << headers.join(QLatin1Char(',')) << '\n';

    for (const auto &row : rows) {
        QStringList fields;
        fields.reserve(row.size());
        for (const auto &field : row) {
            fields.append(escape(field));
        }
        stream << fields.join(QLatin1Char(',')) << '\n';
    }

    if (stream.status() != QTextStream::Ok) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write CSV stream.");
        }
        return false;
    }

    return true;
}

QString CsvExportWriter::escape(const QString &value)
{
    auto escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

} // namespace Monitor::Presentation::Export
