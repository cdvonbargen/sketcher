/* -------------------------------------------------------------------------
 * Playwright-only test bridge for the WebAssembly Sketcher application.
 *
 * Qt/WASM paints the entire application into a single canvas element, so
 * Playwright cannot see individual widgets or scene items in the DOM. The
 * bridge exists to close exactly that gap and no more: sketcher_get_rect()
 * resolves a selector to canvas coordinates, and the test then drives the
 * application with real Playwright mouse and keyboard events. Anything a test
 * can do with coordinates plus Playwright belongs in the JavaScript helpers,
 * not here.
 *
 * Popup windows (menus, and Qt::Popup widgets) are painted into their own
 * canvas element, but that element is positioned over the page, so a rect
 * mapped through global coordinates is still a clickable page coordinate. That
 * means every control a test needs can be reached with a real mouse event, and
 * the bridge never has to activate anything programmatically.
 *
 * Everything here is self-contained: the function is registered with JavaScript
 * by the EMSCRIPTEN_BINDINGS block at the bottom, so no other translation unit
 * refers to it. Production UI code neither calls nor depends on this interface.
 * The bound name is underscore-prefixed to mark it as test-only.
 *
 * Copyright Schrodinger LLC, All Rights Reserved.
 --------------------------------------------------------------------------- */

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#endif

#include <stdexcept>
#include <string>
#include <utility>

#include <QAction>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <rdkit/GraphMol/Atom.h>
#include <rdkit/GraphMol/Bond.h>

#include "schrodinger/sketcher/molviewer/abstract_atom_or_monomer_item.h"
#include "schrodinger/sketcher/molviewer/abstract_bond_or_connector_item.h"
#include "schrodinger/sketcher/sketcher_widget.h"
#include "sketcher_instance.h"

using schrodinger::sketcher::AbstractAtomOrMonomerItem;
using schrodinger::sketcher::AbstractBondOrConnectorItem;
using schrodinger::sketcher::SketcherWidget;

namespace
{

std::string to_json(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
}

std::string rect_to_json(const QPoint& top_left, const QSize& size,
                         const bool enabled)
{
    QJsonObject result;
    result["x"] = top_left.x();
    result["y"] = top_left.y();
    result["width"] = size.width();
    result["height"] = size.height();
    result["enabled"] = enabled;
    return to_json(result);
}

/**
 * Split a selector such as "widget:na_u_btn" or "atom:2" into its kind and
 * value. Throws if there is no ':' separator, which indicates a test bug.
 */
std::pair<std::string, std::string> split_selector(const std::string& selector)
{
    const auto separator = selector.find(':');
    if (separator == std::string::npos) {
        throw std::runtime_error(
            "playwright test bridge: malformed selector '" + selector +
            "' (expected \"<kind>:<value>\")");
    }
    return {selector.substr(0, separator), selector.substr(separator + 1)};
}

/**
 * QAction text may contain an ampersand marking its mnemonic (e.g. "Modify
 * &All"), but tests refer to actions by the name the user sees.
 */
QString without_mnemonic(QString text)
{
    text.remove('&');
    return text.simplified();
}

/**
 * Find a visible child widget by objectName. Several widgets may share a name,
 * so this returns the first visible one, or nullptr if none is visible.
 */
QWidget* find_visible_widget(SketcherWidget& sketcher, const QString& name)
{
    for (auto* widget : sketcher.findChildren<QWidget*>(name)) {
        if (widget->isVisible()) {
            return widget;
        }
    }
    return nullptr;
}

/**
 * Map a point from some widget's coordinates into the sketcher's.
 *
 * This deliberately goes through global coordinates rather than using
 * QWidget::mapTo(). A Qt::Popup (or a dialog) is a separate top-level window,
 * and mapTo() only walks the widget hierarchy — for a window it accumulates
 * screen coordinates partway through and returns nonsense. Going via global
 * coordinates is correct for both cases and matches what Qt itself does for
 * independent top-level widgets.
 */
QPoint map_to_sketcher(const QWidget& widget, const SketcherWidget& sketcher,
                       const QPoint& point)
{
    return sketcher.mapFromGlobal(widget.mapToGlobal(point));
}

QGraphicsView& require_view(SketcherWidget& sketcher)
{
    auto* view = sketcher.findChild<QGraphicsView*>("view");
    if (view == nullptr) {
        throw std::runtime_error(
            "playwright test bridge: no QGraphicsView named 'view'");
    }
    return *view;
}

/**
 * Find the visible Scene item for the atom or bond at the given model index, or
 * nullptr if there isn't one.
 *
 * The casts are dynamic_casts to the abstract base classes rather than
 * qgraphicsitem_casts to AtomItem/BondItem, because qgraphicsitem_cast matches
 * type() exactly and so would skip monomers and monomer connectors: those are
 * siblings of AtomItem and BondItem, not subclasses.
 */
QGraphicsItem* find_visible_item(const QGraphicsView& view, const bool is_atom,
                                 const int index)
{
    for (auto* item : view.scene()->items()) {
        if (!item->isVisible()) {
            continue;
        }
        if (is_atom) {
            auto* atom_item = dynamic_cast<AbstractAtomOrMonomerItem*>(item);
            if (atom_item != nullptr &&
                static_cast<int>(atom_item->getAtom()->getIdx()) == index) {
                return atom_item;
            }
        } else {
            auto* bond_item = dynamic_cast<AbstractBondOrConnectorItem*>(item);
            if (bond_item != nullptr &&
                static_cast<int>(bond_item->getBond()->getIdx()) == index) {
                return bond_item;
            }
        }
    }
    return nullptr;
}

/**
 * Resolve a "widget:<objectName>" selector, or "{}" if no visible widget
 * matches.
 */
std::string widget_rect(SketcherWidget& sketcher, const std::string& name)
{
    auto* widget = find_visible_widget(sketcher, QString::fromStdString(name));
    if (widget == nullptr) {
        return "{}";
    }
    return rect_to_json(map_to_sketcher(*widget, sketcher, QPoint(0, 0)),
                        widget->size(), widget->isEnabled());
}

/**
 * Resolve an "action:<objectName or text>" selector to the action's row in
 * whichever menu is currently open, or "{}" if no visible menu has it.
 *
 * Only visible menus are searched, and only on demand: a QMenu has no geometry
 * until it is laid out, and inspecting one during its own show sequence aborts
 * the Qt/WASM runtime. Once the popup is up, actionGeometry() is safe to read
 * and gives a real click target. Submenus are found the same way, since an open
 * submenu is just another visible QMenu.
 *
 * Actions are matched on objectName first and then on display text, since most
 * menu actions are created without an objectName.
 */
std::string action_rect(SketcherWidget& sketcher, const std::string& name)
{
    const QString query = QString::fromStdString(name);
    const QString wanted = without_mnemonic(query);
    for (auto* menu : sketcher.findChildren<QMenu*>()) {
        if (!menu->isVisible()) {
            continue;
        }
        for (auto* action : menu->actions()) {
            if (action->objectName() != query &&
                without_mnemonic(action->text()) != wanted) {
                continue;
            }
            const QRect geometry = menu->actionGeometry(action);
            if (geometry.isEmpty()) {
                continue;
            }
            return rect_to_json(
                map_to_sketcher(*menu, sketcher, geometry.topLeft()),
                geometry.size(), action->isEnabled());
        }
    }
    return "{}";
}

/**
 * Resolve an "atom:<index>" or "bond:<index>" selector, or "{}" if no visible
 * item matches. Scene items are QGraphicsItems rather than QWidgets, so they
 * can't be found by objectName; their bounding rect is mapped through the View
 * transform instead.
 */
std::string item_rect(SketcherWidget& sketcher, const bool is_atom,
                      const std::string& value)
{
    bool is_number = false;
    const int index = QString::fromStdString(value).toInt(&is_number);
    if (!is_number) {
        throw std::runtime_error("playwright test bridge: '" + value +
                                 "' is not a valid item index");
    }
    auto& view = require_view(sketcher);
    auto* item = find_visible_item(view, is_atom, index);
    if (item == nullptr) {
        return "{}";
    }
    const QRect viewport_rect =
        view.mapFromScene(item->sceneBoundingRect()).boundingRect();
    return rect_to_json(
        map_to_sketcher(*view.viewport(), sketcher, viewport_rect.topLeft()),
        viewport_rect.size(), item->isEnabled());
}

} // namespace

// The function below deliberately has external linkage even though nothing
// else refers to it: the EMSCRIPTEN_BINDINGS block is compiled out on desktop
// builds, and a file-local function would then trip -Wunused-function under
// -Werror. Compiling it everywhere means the non-WASM CI jobs still catch
// errors here.

/**
 * Resolve an object selector to its position and size on the canvas, as
 * {"x":…, "y":…, "width":…, "height":…, "enabled":…}. Coordinates are relative
 * to the sketcher widget's top-left corner, which is also the top-left of the
 * WASM canvas, so tests can use them directly as page coordinates.
 *
 * Supported selectors:
 *
 *   "widget:<objectName>"  a QWidget, by its Qt objectName
 *   "action:<name>"        a row of a currently-open menu, by objectName or
 *                          visible text
 *   "atom:<index>"         an atom of the current molecule, by model index
 *   "bond:<index>"         a bond of the current molecule, by model index
 *
 * Monomers are addressed as "atom" and monomer connectors as "bond", since the
 * model stores them as RDKit atoms and bonds. Every atom has a non-empty
 * bounding rect even when its label isn't painted (an unlabeled carbon, say),
 * because the rect is built from the predictive highlighting path, so the
 * returned rect is always a usable click target.
 *
 * Model indices are indices into the molecule returned by
 * SketcherWidget::getRDKitMolecule(). That molecule is a plain copy of the one
 * the scene items are built from, so the indices agree; note that this is the
 * order the structure was built in, not the canonical order a format like
 * SMILES may renumber to on export.
 *
 * Returns "{}" when nothing visible matches, since there is no coordinate a
 * test could click. "enabled" reports whether Qt would accept a click there;
 * Playwright applies that check automatically to real DOM elements but has no
 * way to do so for something painted into a canvas.
 *
 * Throws std::runtime_error for a malformed selector or an unrecognized kind,
 * which indicates the test needs to be updated.
 */
std::string sketcher_get_rect(const std::string& selector)
{
    const auto [kind, value] = split_selector(selector);
    auto& sketcher = get_sketcher_instance();
    if (kind == "widget") {
        return widget_rect(sketcher, value);
    }
    if (kind == "action") {
        return action_rect(sketcher, value);
    }
    if (kind == "atom" || kind == "bond") {
        return item_rect(sketcher, kind == "atom", value);
    }
    throw std::runtime_error(
        "playwright test bridge: unrecognized selector kind '" + kind +
        "' (expected 'widget', 'action', 'atom', or 'bond')");
}

#ifdef __EMSCRIPTEN__
// A second bindings block alongside the one in main.cpp; embind allows any
// number of them as long as each has a distinct name. This object file is
// linked directly into the executable rather than through a static library, so
// the registrations always run.
EMSCRIPTEN_BINDINGS(sketcher_playwright_test_bridge)
{
    emscripten::function("_sketcher_get_rect", &sketcher_get_rect);
}
#endif
