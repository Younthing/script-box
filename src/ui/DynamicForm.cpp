#include "DynamicForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace
{
QList<ParamOption> ensureOptions(const ParamDTO &param)
{
    QList<ParamOption> options = param.options;
    if (options.isEmpty() && !param.defaultValue.isEmpty())
    {
        options.append({param.defaultValue, param.defaultValue});
    }
    return options;
}
} // namespace

DynamicForm::DynamicForm(QWidget *parent)
    : QWidget(parent)
{
    m_formLayout = new QFormLayout(this);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
}

void DynamicForm::setParams(const QList<ParamDTO> &params)
{
    // Clear existing widgets
    while (auto item = m_formLayout->takeAt(0))
    {
        if (auto w = item->widget())
        {
            w->deleteLater();
        }
        delete item;
    }
    m_fieldWidgets.clear();
    m_params = params;

    for (const auto &param : params)
    {
        QWidget *field = createField(param);
        m_formLayout->addRow(param.label, field);
        m_fieldWidgets.insert(param.key, field);
    }
}

QList<RunParamValueDTO> DynamicForm::collectValues() const
{
    QList<RunParamValueDTO> values;
    for (const auto &param : m_params)
    {
        values.append(collectFor(param));
    }
    return values;
}

QWidget *DynamicForm::createField(const ParamDTO &param)
{
    switch (param.type)
    {
    case ParamType::Int: {
        auto *spin = new QSpinBox(this);
        spin->setRange(static_cast<int>(param.min), static_cast<int>(param.max == 0 ? 1'000'000 : param.max));
        spin->setValue(param.defaultValue.isEmpty() ? 0 : param.defaultValue.toInt());
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, &DynamicForm::formChanged);
        return spin;
    }
    case ParamType::Float: {
        auto *spin = new QDoubleSpinBox(this);
        spin->setRange(param.min, param.max == 0 ? 1'000'000.0 : param.max);
        spin->setSingleStep(param.step);
        spin->setValue(param.defaultValue.isEmpty() ? 0.0 : param.defaultValue.toDouble());
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &DynamicForm::formChanged);
        return spin;
    }
    case ParamType::Select: {
        auto *combo = new QComboBox(this);
        const auto opts = ensureOptions(param);
        for (const auto &opt : opts)
        {
            combo->addItem(opt.label, opt.value);
        }
        connect(combo, &QComboBox::currentTextChanged, this, &DynamicForm::formChanged);
        return combo;
    }
    case ParamType::Bool: {
        auto *check = new QCheckBox(this);
        check->setChecked(param.defaultValue.toLower() == QStringLiteral("true"));
        connect(check, &QCheckBox::checkStateChanged, this, &DynamicForm::formChanged);
        return check;
    }
    case ParamType::File:
    case ParamType::Dir: {
        auto *container = new QWidget(this);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *edit = new QLineEdit(container);
        edit->setPlaceholderText(param.placeholder);
        edit->setText(param.defaultValue);
        connect(edit, &QLineEdit::textChanged, this, &DynamicForm::formChanged);

        auto *btn = new QPushButton(param.type == ParamType::File ? tr("Browse File") : tr("Browse Dir"), container);
        connect(btn, &QPushButton::clicked, container, [this, edit, param]() {
            QString path;
            if (param.type == ParamType::File)
            {
                path = QFileDialog::getOpenFileName(this, tr("Select File"));
            }
            else
            {
                path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
            }
            if (!path.isEmpty())
            {
                edit->setText(path);
            }
        });

        layout->addWidget(edit);
        layout->addWidget(btn);
        return container;
    }
    case ParamType::Text:
    default: {
        auto *edit = new QLineEdit(this);
        edit->setPlaceholderText(param.placeholder);
        edit->setText(param.defaultValue);
        connect(edit, &QLineEdit::textChanged, this, &DynamicForm::formChanged);
        return edit;
    }
    }
}

RunParamValueDTO DynamicForm::collectFor(const ParamDTO &param) const
{
    RunParamValueDTO dto;
    dto.key = param.key;

    QWidget *field = m_fieldWidgets.value(param.key, nullptr);
    if (!field)
    {
        return dto;
    }

    switch (param.type)
    {
    case ParamType::Int: {
        auto *spin = qobject_cast<QSpinBox *>(field);
        if (spin) dto.values << QString::number(spin->value());
        break;
    }
    case ParamType::Float: {
        auto *spin = qobject_cast<QDoubleSpinBox *>(field);
        if (spin) dto.values << QString::number(spin->value());
        break;
    }
    case ParamType::Select: {
        auto *combo = qobject_cast<QComboBox *>(field);
        if (combo) dto.values << combo->currentData().toString();
        break;
    }
    case ParamType::Bool: {
        auto *check = qobject_cast<QCheckBox *>(field);
        if (check) dto.values << (check->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        break;
    }
    case ParamType::File:
    case ParamType::Dir: {
        auto *container = field;
        auto *edit = container->findChild<QLineEdit *>();
        if (edit) dto.values << edit->text();
        break;
    }
    case ParamType::Text:
    default: {
        auto *edit = qobject_cast<QLineEdit *>(field);
        if (!edit && field)
        {
            edit = field->findChild<QLineEdit *>();
        }
        if (edit) dto.values << edit->text();
        break;
    }
    }

    return dto;
}
