#pragma once

#include "common/Dto.h"

#include <QMap>
#include <QWidget>

class QFormLayout;
class QLineEdit;

class DynamicForm : public QWidget
{
    Q_OBJECT
public:
    explicit DynamicForm(QWidget *parent = nullptr);

    void setParams(const QList<ParamDTO> &params);
    QList<RunParamValueDTO> collectValues() const;

signals:
    void formChanged();

private:
    QWidget *createField(const ParamDTO &param);
    RunParamValueDTO collectFor(const ParamDTO &param) const;

    QList<ParamDTO> m_params;
    QMap<QString, QWidget *> m_fieldWidgets;
    QFormLayout *m_formLayout{nullptr};
};
