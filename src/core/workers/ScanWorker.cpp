#include "ScanWorker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace
{
QString trimQuotes(const QString &value)
{
    if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\'')))
    {
        return value.mid(1, value.size() - 2);
    }
    return value;
}
} // namespace

void ScanWorker::scan(const QString &toolsRoot)
{
    ScanResultDTO result;
    QDir root(toolsRoot);
    if (!root.exists())
    {
        result.error = QStringLiteral("Tools root does not exist: %1").arg(toolsRoot);
        emit scanFinished(result);
        return;
    }

    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries)
    {
        const QString toolDir = root.filePath(entry);
        const QString yamlPath = QDir(toolDir).filePath(QStringLiteral("tool.yaml"));
        if (!QFile::exists(yamlPath))
        {
            continue;
        }

        QString error;
        ToolDTO dto = parseTool(toolDir, error);
        if (!error.isEmpty())
        {
            if (!result.error.isEmpty())
            {
                result.error.append('\n');
            }
            result.error.append(error);
        }

        if (!dto.id.isEmpty())
        {
            result.tools.append(dto);
        }
    }

    emit scanFinished(result);
}

ToolDTO ScanWorker::parseTool(const QString &toolDirPath, QString &error) const
{
    ToolDTO dto;
    dto.id = QFileInfo(toolDirPath).fileName();

    const QString yamlPath = QDir(toolDirPath).filePath(QStringLiteral("tool.yaml"));
    QFile file(yamlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Failed to open %1").arg(yamlPath);
        return dto;
    }

    const QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split('\n');

    dto.name = trimQuotes(readScalar(lines, QStringLiteral("name")));
    if (dto.name.isEmpty())
    {
        dto.name = dto.id;
    }

    dto.version = trimQuotes(readScalar(lines, QStringLiteral("version")));
    dto.description = trimQuotes(readScalar(lines, QStringLiteral("description")));
    dto.tags = readList(lines, QStringLiteral("tags"));
    dto.command = trimQuotes(readScalar(lines, QStringLiteral("command")));
    dto.workdir = trimQuotes(readScalar(lines, QStringLiteral("workdir")));

    dto.env.type = trimQuotes(readBlockScalar(lines, QStringLiteral("env"), QStringLiteral("type")));
    dto.env.useUv = readBlockScalar(lines, QStringLiteral("env"), QStringLiteral("use_uv")).toLower() == QStringLiteral("true");
    dto.env.interpreter = trimQuotes(readBlockScalar(lines, QStringLiteral("env"), QStringLiteral("interpreter")));
    dto.env.dependencies = readBlockList(lines, QStringLiteral("env"), QStringLiteral("dependencies"));

    dto.params = readParams(lines);

    if (dto.command.isEmpty())
    {
        error = QStringLiteral("Missing command in %1").arg(yamlPath);
    }

    return dto;
}

QString ScanWorker::readScalar(const QStringList &lines, const QString &key)
{
    const QString pattern = QStringLiteral("^\\s*%1:\\s*(.+)$").arg(QRegularExpression::escape(key));
    QRegularExpression re(pattern);
    for (const QString &line : lines)
    {
        const auto match = re.match(line);
        if (match.hasMatch())
        {
            return match.captured(1).trimmed();
        }
    }
    return {};
}

QStringList ScanWorker::readList(const QStringList &lines, const QString &key)
{
    QStringList values;
    const QString headPattern = QStringLiteral("^\\s*%1:\\s*$").arg(QRegularExpression::escape(key));
    QRegularExpression head(headPattern);
    QRegularExpression item(QStringLiteral("^\\s*-\\s*(.+)$"));

    bool inBlock = false;
    for (const QString &line : lines)
    {
        if (!inBlock)
        {
            if (head.match(line).hasMatch())
            {
                inBlock = true;
            }
            continue;
        }

        auto m = item.match(line);
        if (m.hasMatch())
        {
            values << trimQuotes(m.captured(1).trimmed());
            continue;
        }

        // End of block when encountering a non-item line.
        if (!line.trimmed().isEmpty() && !line.startsWith(' '))
        {
            break;
        }
    }
    return values;
}

QString ScanWorker::readBlockScalar(const QStringList &lines, const QString &block, const QString &key)
{
    const QString blockHead = QStringLiteral("^\\s*%1:\\s*$").arg(QRegularExpression::escape(block));
    QRegularExpression blockRe(blockHead);
    QRegularExpression keyRe(QStringLiteral("^\\s*%1:\\s*(.+)$").arg(QRegularExpression::escape(key)));

    int blockIndent = -1;
    bool inBlock = false;
    for (const QString &line : lines)
    {
        if (line.trimmed().isEmpty())
        {
            continue;
        }

        const int indent = line.indexOf(QRegularExpression("\\S"));
        if (!inBlock)
        {
            if (blockRe.match(line).hasMatch())
            {
                blockIndent = indent;
                inBlock = true;
            }
            continue;
        }

        if (indent <= blockIndent)
        {
            break;
        }

        const auto match = keyRe.match(line.trimmed());
        if (match.hasMatch())
        {
            return match.captured(1).trimmed();
        }
    }
    return {};
}

QStringList ScanWorker::readBlockList(const QStringList &lines, const QString &block, const QString &key)
{
    QStringList values;
    const QString blockHead = QStringLiteral("^\\s*%1:\\s*$").arg(QRegularExpression::escape(block));
    QRegularExpression blockRe(blockHead);

    QRegularExpression keyHead(QStringLiteral("^\\s*%1:\\s*$").arg(QRegularExpression::escape(key)));
    QRegularExpression itemRe(QStringLiteral("^\\s*-\\s*(.+)$"));

    int blockIndent = -1;
    int keyIndent = -1;
    bool inBlock = false;
    bool inKey = false;

    for (const QString &line : lines)
    {
        if (line.trimmed().isEmpty())
        {
            continue;
        }

        const int indent = line.indexOf(QRegularExpression("\\S"));
        if (!inBlock)
        {
            if (blockRe.match(line).hasMatch())
            {
                blockIndent = indent;
                inBlock = true;
            }
            continue;
        }

        if (indent <= blockIndent)
        {
            break;
        }

        if (!inKey)
        {
            if (keyHead.match(line).hasMatch())
            {
                keyIndent = indent;
                inKey = true;
            }
            continue;
        }

        if (indent <= keyIndent)
        {
            break;
        }

        const auto itemMatch = itemRe.match(line.trimmed());
        if (itemMatch.hasMatch())
        {
            values << trimQuotes(itemMatch.captured(1).trimmed());
        }
    }

    return values;
}

QList<ParamDTO> ScanWorker::readParams(const QStringList &lines) const
{
    QList<ParamDTO> params;
    int startIndex = -1;
    for (int i = 0; i < lines.size(); ++i)
    {
        if (lines[i].trimmed().startsWith(QStringLiteral("params:")))
        {
            startIndex = i + 1;
            break;
        }
    }

    if (startIndex == -1)
    {
        return params;
    }

    auto parseField = [](const QString &line, const QString &field) -> QString {
        QRegularExpression re(QStringLiteral("^\\s*[- ]*%1:\\s*(.+)$").arg(QRegularExpression::escape(field)));
        auto m = re.match(line.trimmed());
        if (m.hasMatch())
        {
            return trimQuotes(m.captured(1).trimmed());
        }
        return {};
    };

    ParamDTO current;
    bool inParam = false;
    for (int i = startIndex; i < lines.size(); ++i)
    {
        const QString trimmed = lines[i].trimmed();
        if (trimmed.startsWith('-'))
        {
            if (inParam && !current.key.isEmpty())
            {
                params << current;
            }
            current = ParamDTO{};
            inParam = true;
        }
        if (!inParam)
        {
            continue;
        }

        const QString keyVal = parseField(trimmed, QStringLiteral("key"));
        if (!keyVal.isEmpty()) current.key = keyVal;

        const QString labelVal = parseField(trimmed, QStringLiteral("label"));
        if (!labelVal.isEmpty()) current.label = labelVal;

        const QString typeVal = parseField(trimmed, QStringLiteral("type"));
        if (!typeVal.isEmpty()) current.type = paramTypeFromString(typeVal);

        const QString requiredVal = parseField(trimmed, QStringLiteral("required"));
        if (!requiredVal.isEmpty()) current.required = requiredVal.toLower() == QStringLiteral("true");

        const QString defaultVal = parseField(trimmed, QStringLiteral("default"));
        if (!defaultVal.isEmpty()) current.defaultValue = defaultVal;

        const QString multiVal = parseField(trimmed, QStringLiteral("multi"));
        if (!multiVal.isEmpty()) current.multi = multiVal.toLower() == QStringLiteral("true");

        const QString minVal = parseField(trimmed, QStringLiteral("min"));
        if (!minVal.isEmpty()) current.min = minVal.toDouble();

        const QString maxVal = parseField(trimmed, QStringLiteral("max"));
        if (!maxVal.isEmpty()) current.max = maxVal.toDouble();

        const QString stepVal = parseField(trimmed, QStringLiteral("step"));
        if (!stepVal.isEmpty()) current.step = stepVal.toDouble();

        const QString placeholderVal = parseField(trimmed, QStringLiteral("placeholder"));
        if (!placeholderVal.isEmpty()) current.placeholder = placeholderVal;

        const QString patternVal = parseField(trimmed, QStringLiteral("pattern"));
        if (!patternVal.isEmpty()) current.pattern = patternVal;

        const QString descVal = parseField(trimmed, QStringLiteral("description"));
        if (!descVal.isEmpty()) current.description = descVal;
    }

    if (inParam && !current.key.isEmpty())
    {
        params << current;
    }

    return params;
}
