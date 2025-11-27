#include "ScanWorker.h"

#include <QDir>
#include <QFileInfo>

#include <yaml-cpp/yaml.h>

namespace
{
QString toQString(const YAML::Node &node, const QString &fallback = QString())
{
    if (!node)
    {
        return fallback;
    }
    if (node.IsScalar())
    {
        return QString::fromStdString(node.as<std::string>());
    }
    return fallback;
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
    try
    {
        YAML::Node root = YAML::LoadFile(yamlPath.toStdString());

        dto.name = toQString(root["name"], dto.id);
        dto.version = toQString(root["version"]);
        dto.description = toQString(root["description"]);

        if (root["tags"])
        {
            for (const auto &tag : root["tags"])
            {
                dto.tags << QString::fromStdString(tag.as<std::string>());
            }
        }

        dto.command = toQString(root["command"]);
        dto.workdir = toQString(root["workdir"], QStringLiteral("."));

        if (root["env"])
        {
            const auto env = root["env"];
            dto.env.type = toQString(env["type"]);
            dto.env.useUv = env["use_uv"].as<bool>(false);
            dto.env.interpreter = toQString(env["interpreter"]);
            if (env["dependencies"])
            {
                for (const auto &dep : env["dependencies"])
                {
                    dto.env.dependencies << QString::fromStdString(dep.as<std::string>());
                }
            }
            if (env["env_vars"])
            {
                for (auto it = env["env_vars"].begin(); it != env["env_vars"].end(); ++it)
                {
                    dto.env.envVars.insert(QString::fromStdString(it->first.as<std::string>()),
                                           QString::fromStdString(it->second.as<std::string>("")));
                }
            }
        }

        if (root["params"])
        {
            for (const auto &p : root["params"])
            {
                ParamDTO param;
                param.key = toQString(p["key"]);
                param.label = toQString(p["label"], param.key);
                param.type = paramTypeFromString(toQString(p["type"]));
                param.required = p["required"].as<bool>(false);
                param.defaultValue = toQString(p["default"]);
                param.multi = p["multi"].as<bool>(false);
                param.min = p["min"].as<double>(0.0);
                param.max = p["max"].as<double>(0.0);
                param.step = p["step"].as<double>(1.0);
                param.placeholder = toQString(p["placeholder"]);
                param.pattern = toQString(p["pattern"]);
                param.description = toQString(p["description"]);

                if (p["options"])
                {
                    for (const auto &opt : p["options"])
                    {
                        ParamOption o;
                        if (opt.IsMap())
                        {
                            o.label = toQString(opt["label"]);
                            o.value = toQString(opt["value"], o.label);
                        }
                        else
                        {
                            const QString val = QString::fromStdString(opt.as<std::string>(""));
                            o.label = val;
                            o.value = val;
                        }
                        param.options.append(o);
                    }
                }

                if (!param.key.isEmpty())
                {
                    dto.params.append(param);
                }
            }
        }

        if (dto.command.isEmpty())
        {
            error = QStringLiteral("Missing command in %1").arg(yamlPath);
        }
    }
    catch (const YAML::Exception &ex)
    {
        error = QStringLiteral("Failed to parse %1: %2").arg(yamlPath, QString::fromStdString(ex.what()));
    }

    return dto;
}
