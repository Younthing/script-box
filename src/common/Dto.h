#pragma once

#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

enum class ParamType
{
    File,
    Dir,
    Select,
    Int,
    Float,
    Text,
    Bool,
    Unknown
};

struct ParamOption
{
    QString label;
    QString value;
};

struct ParamDTO
{
    QString key;
    QString label;
    ParamType type{ParamType::Unknown};
    bool required{false};
    QString defaultValue;
    QList<ParamOption> options;
    bool multi{false};
    double min{0.0};
    double max{0.0};
    double step{1.0};
    QString placeholder;
    QString pattern;
    QString description;
};

struct EnvConfigDTO
{
    QString type; // "python" | "r"
    bool useUv{false};
    QString interpreter;
    QStringList dependencies;
    QMap<QString, QString> envVars;
};

struct ToolDTO
{
    QString id;
    QString name;
    QString version;
    QString description;
    QStringList tags;
    EnvConfigDTO env;
    QString command;
    QString workdir{"."};
    QList<ParamDTO> params;
};

struct RunParamValueDTO
{
    QString key;
    QStringList values;
};

struct RunRequestDTO
{
    QString toolId;
    QString toolVersion;
    QList<RunParamValueDTO> params;
    QString runDirectory;
    QString workdir;
};

struct ScanResultDTO
{
    QList<ToolDTO> tools;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

inline ParamType paramTypeFromString(const QString &value)
{
    const QString lowered = value.trimmed().toLower();
    if (lowered == "file") return ParamType::File;
    if (lowered == "dir") return ParamType::Dir;
    if (lowered == "select") return ParamType::Select;
    if (lowered == "int") return ParamType::Int;
    if (lowered == "float") return ParamType::Float;
    if (lowered == "text") return ParamType::Text;
    if (lowered == "bool") return ParamType::Bool;
    return ParamType::Unknown;
}

Q_DECLARE_METATYPE(ParamDTO)
Q_DECLARE_METATYPE(EnvConfigDTO)
Q_DECLARE_METATYPE(ToolDTO)
Q_DECLARE_METATYPE(RunParamValueDTO)
Q_DECLARE_METATYPE(RunRequestDTO)
Q_DECLARE_METATYPE(ScanResultDTO)
