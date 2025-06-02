// @copyright Schrodinger, LLC - All Rights Reserved

#include "schrodinger/sketcher/example_widget.h"

#include <QWidget>

#include "schrodinger/rdkit_extensions/convert.h"
#include "schrodinger/sketcher/ui/ui_example_widget.h"

namespace schrodinger
{
namespace sketcher
{

ExampleWidget::ExampleWidget(QWidget* parent) : QWidget(parent)
{
    m_ui.reset(new Ui::ExampleWidgetForm());
    m_ui->setupUi(this);
    m_ui->label->setText("Hello World!");
}

ExampleWidget::~ExampleWidget() = default;

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/example_widget.moc"
