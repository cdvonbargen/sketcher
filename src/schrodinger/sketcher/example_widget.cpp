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
    auto mol = rdkit_extensions::to_rdkit("C1=CC=CC=C1");
    auto smiles =
        rdkit_extensions::to_string(*mol, rdkit_extensions::Format::SMILES);
    m_ui->label->setText(QString::fromStdString(smiles));
}

ExampleWidget::~ExampleWidget() = default;

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/example_widget.moc"
