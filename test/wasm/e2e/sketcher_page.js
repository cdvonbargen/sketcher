/**
 * Playwright equivalent of the Squish `Sketcher` page object.
 *
 * Method names deliberately follow qa-ims/squish_modules/maestro/sketcher.py so
 * that a ported test can be read against its Squish original. Everything here
 * is either a translation of a Squish identifier to a Qt objectName or a
 * behavior of the Squish wrapper that a test depends on; the actual browser
 * interaction lives in e2e_helpers.js.
 *
 * This is incremental: a method is added only when its standalone WASM
 * interaction has been implemented with real browser input.
 */
import {
  activateAction,
  clickWidget,
  focusCanvas,
  getClipboardText,
  getDrawingAreaCenter,
  loadStructure,
  mouseClick,
  mouseDrag,
  requireRect,
  waitForSketcherReady,
} from './e2e_helpers.js';

const BUTTON_NAMES = {
  clear: 'clear_btn',
  clear_selection: 'clear_selection_btn',
  invert_selection: 'invert_selection_btn',
  move_rotate: 'move_rotate_btn',
  select_all: 'select_all_btn',
  undo: 'undo_btn',
};

const TOOL_NAMES = {
  C: 'c_btn',
  Cl: 'cl_btn',
  N: 'n_btn',
  O: 'o_btn',
  P: 'p_btn',
  S: 's_btn',
  F: 'f_btn',
  H: 'h_btn',
  move_rotate: 'move_rotate_btn',
  rect_btn: 'select_tool_btn',
};

// Submenu parents in the More Actions menu. In Squish, passing one of these as
// the only argument opens its submenu without choosing a row; here it does
// nothing, since no menu is opened.
const SUBMENU_PARENTS = new Set(['copy_all_as', 'modify_all']);

// Rows are addressed by their action's text, since menu actions are created
// without an objectName. Note that the copy rows are relabelled to "Copy" and
// "Copy As" while something is selected, so these names only resolve with an
// empty selection.
const MORE_ACTION_NAMES = {
  add_explicit_hydrogens: 'Add Explicit Hydrogens',
  aromatize: 'Aromatize',
  kekulize: 'Kekulize',
  modify_all: 'Modify All',
  clear_selection: 'Clear Selection',
  copy_all_as: 'Copy All As',
  copy_all: 'Copy All',
  cut: 'Cut',
  fit_to_screen: 'Fit to Screen',
  flip_horizontal: 'Flip Horizontal',
  flip_vertical: 'Flip Vertical',
  invert_selection: 'Invert Selection',
  paste: 'Paste',
  redo: 'Redo',
  remove_explicit_hydrogens: 'Remove Explicit Hydrogens',
  select_all: 'Select All',
  undo: 'Undo',
};

const COPY_AS_NAMES = {
  cxsmi: 'Extended SMILES',
  inchikey: 'InChIKey',
  inchi: 'InChI',
  pdb: 'PDB',
  sdf: 'MDL SD V3000',
  smi: 'SMILES',
};

function modifiersFor(modifier) {
  if (modifier === 'shift') {
    return ['Shift'];
  }
  if (modifier === 'control') {
    return ['Control'];
  }
  return [];
}

/** Browser equivalent of the source Squish `Sketcher` class. */
export class Sketcher {
  /** @param {import('@playwright/test').Page} page */
  constructor(page) {
    this.page = page;
    this.current_tool = null;
  }

  async open() {
    await waitForSketcherReady(this.page);
  }

  /**
   * Fixture setup only; never use this method in `tst_import_menu`.
   */
  async load_structure_for_test(text) {
    await loadStructure(this.page, text);
  }

  /** Equivalent to Squish `click_button(object_name)`. */
  async click_button(object_name) {
    await clickWidget(this.page, BUTTON_NAMES[object_name] || object_name);
    if (object_name === 'clear') {
      this.current_tool = null;
    }
  }

  /** Equivalent to Squish `click_tool(tool, click_and_hold=False)`. */
  async click_tool(tool, _click_and_hold = false) {
    // Squish tracks sticky tools and avoids re-clicking the current tool.
    // In particular, repeated rect_btn clicks interrupt Shift-add selection.
    if (this.current_tool === tool) {
      return;
    }
    await clickWidget(this.page, TOOL_NAMES[tool] || tool);
    this.current_tool = tool;
  }

  /**
   * Equivalent to Squish `more_actions_menu(button1, button2=None)`.
   *
   * The menu is never opened. Qt runs a nested event loop for as long as a
   * QToolButton's menu is showing, which under Asyncify suspends the whole WASM
   * module, so a test cannot locate a row to click it — see activateAction.
   * Opening a submenu is therefore a no-op here, and only the leaf row matters.
   */
  async more_actions_menu(button1, button2 = null) {
    if (SUBMENU_PARENTS.has(button1)) {
      if (button2 === null) {
        return;
      }
      await activateAction(
        this.page,
        MORE_ACTION_NAMES[button2] || COPY_AS_NAMES[button2] || button2,
      );
      return;
    }
    if (button2 !== null) {
      throw new Error(`More Actions row "${button1}" has no submenu`);
    }
    await activateAction(this.page, MORE_ACTION_NAMES[button1] || button1);
  }

  /** Equivalent to Squish `getClipboardText()` after Copy/Cut actions. */
  async clipboard_text() {
    return getClipboardText(this.page);
  }

  /** Equivalent to Squish `type_text("sketcher_area", "<Ctrl+X>")`. */
  async type_text(object_name, text) {
    if (object_name !== 'sketcher_area') {
      throw new Error(`Standalone type_text does not yet support: ${object_name}`);
    }
    await focusCanvas(this.page);
    const shortcut = String(text)
      .replace(/^<Ctrl\+/, 'Control+')
      .replace(/^<Ctrl\+Shift\+/, 'Control+Shift+')
      .replace(/>$/, '');
    await this.page.keyboard.press(shortcut);
  }

  /** Equivalent to `mouse_drag` using source coordinates relative to canvas center. */
  async mouse_drag(x, y, dx, dy, modifier = null, click = 'left') {
    const center = await getDrawingAreaCenter(this.page);
    await mouseDrag(
      this.page,
      { x: center.x + x, y: center.y + y },
      { x: center.x + x + dx, y: center.y + y + dy },
      { button: click, modifiers: modifiersFor(modifier) },
    );
  }

  /** Equivalent to Squish `click_atom(atom, select=False, modifier=None)`. */
  async click_atom(atom, select = false, modifier = null) {
    await this.click_item('atom', atom, select, modifier);
  }

  /** Equivalent to Squish `click_bond(bond, select=False, modifier=None)`. */
  async click_bond(bond, select = false, modifier = null) {
    await this.click_item('bond', bond, select, modifier);
  }

  /**
   * The bridge maps the live QGraphicsItem through QGraphicsView, which avoids
   * the fragile SDF-coordinate-to-pixel calculation in Squish.
   */
  async click_item(type, index, select, modifier) {
    if (select) {
      await this.click_tool('rect_btn');
    }
    const rect = await requireRect(this.page, `${type}:${index}`);
    await mouseClick(this.page, rect.x + rect.width / 2, rect.y + rect.height / 2, {
      modifiers: modifiersFor(modifier),
    });
  }
}
