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

bool toBool(const YAML::Node &node, bool fallback = false)
{
    if (!node)
    {
        return fallback;
    }
    if (node.IsScalar())
    {
        return node.as<bool>(fallback);
    }
    return fallback;
}

QStringList toStringList(const YAML::Node &node)
{
    QStringList list;
    if (!node || !node.IsSequence())
    {
        return list;
    }
    for (const auto &item : node)
    {
        list << QString::fromStdString(item.as<std::string>(""));
    }
    return list;
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
        dto.category = toQString(root["category"], QStringLiteral("未分类"));
        dto.thumbnail = toQString(root["thumbnail"]);

        if (root["tags"])
        {
            for (const auto &tag : root["tags"])
            {
                dto.tags << QString::fromStdString(tag.as<std::string>());
            }
        }

        // runtime
        if (root["runtime"])
        {
            const auto runtime = root["runtime"];
            dto.runtime.type = toQString(runtime["type"]);
            dto.runtime.entry = toQString(runtime["entry"]);
            dto.runtime.args = toStringList(runtime["args"]);
            dto.runtime.shellWrap = toBool(runtime["shell"], toBool(runtime["shell_wrap"], false));
            dto.runtime.workdir = toQString(runtime["workdir"], QStringLiteral("."));
            dto.runtime.timeoutSeconds = runtime["timeout"].as<int>(0);

            if (runtime["extra_env"])
            {
                for (auto it = runtime["extra_env"].begin(); it != runtime["extra_env"].end(); ++it)
                {
                    dto.runtime.extraEnv.insert(QString::fromStdString(it->first.as<std::string>()),
                                                QString::fromStdString(it->second.as<std::string>("")));
                }
            }

            if (runtime["expected_outputs"])
            {
                for (const auto &out : runtime["expected_outputs"])
                {
                    ExpectedOutputDTO ex;
                    ex.path = toQString(out["path"]);
                    ex.label = toQString(out["label"], ex.path);
                    ex.type = toQString(out["type"], QStringLiteral("file"));
                    if (!ex.path.isEmpty())
                    {
                        dto.runtime.expectedOutputs.append(ex);
                    }
                }
            }
        }
        else
        {
            dto.runtime.type = QStringLiteral("generic");
            dto.runtime.entry = toQString(root["command"]); // legacy fallback
        }

        // env
        if (root["env"])
        {
            const auto env = root["env"];
            dto.env.strategy = toQString(env["strategy"], toQString(env["type"]));
            dto.env.interpreterPath = toQString(env["interpreter"], toQString(env["interpreter_path"]));
            dto.env.dependencies = toStringList(env["dependencies"]);
            dto.env.cacheDir = toQString(env["cache_dir"]);

            if (env["setup"])
            {
                const auto setup = env["setup"];
                dto.env.setup.command = toQString(setup["command"]);
                dto.env.setup.shell = toBool(setup["shell"]);
                dto.env.setup.workdir = toQString(setup["workdir"], QStringLiteral("."));
            }
        }

        if (dto.env.cacheDir.isEmpty())
        {
            if (dto.runtime.type.toLower() == QStringLiteral("r"))
            {
                dto.env.cacheDir = QStringLiteral(".r-lib");
            }
            else if (dto.runtime.type.toLower() == QStringLiteral("python"))
            {
                dto.env.cacheDir = QStringLiteral(".venv");
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

        if (dto.runtime.entry.isEmpty())
        {
            error = QStringLiteral("Missing runtime.entry in %1").arg(yamlPath);
        }
    }
    catch (const YAML::Exception &ex)
    {
        error = QStringLiteral("Failed to parse %1: %2").arg(yamlPath, QString::fromStdString(ex.what()));
    }

    return dto;
}
