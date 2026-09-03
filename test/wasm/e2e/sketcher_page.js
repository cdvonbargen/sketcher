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
  clickPopupTool,
  clickWidget,
  contextMenuAction,
  focusCanvas,
  getClipboardText,
  getDrawingAreaCenter,
  isWidgetVisible,
  loadStructure,
  mouseClick,
  mouseDrag,
  openContextMenu,
  requireRect,
  waitForSketcherReady,
  widgetState,
} from './e2e_helpers.js';

/**
 * Squish names for buttons and tools whose Qt objectName is not simply the
 * lowercased name with a "_btn" suffix. Everything else -- every element, ring,
 * and bond-order tool -- follows that rule and needs no entry here.
 */
const WIDGET_NAMES = {
  down: 'stereo_bond2_btn',
  minus_charge: 'decrease_charge_btn',
  plus_charge: 'increase_charge_btn',
  rect_btn: 'select_tool_btn',
  single: 'single_bond_btn',
  up: 'stereo_bond1_btn',
};

function widgetName(name) {
  if (WIDGET_NAMES[name]) {
    return WIDGET_NAMES[name];
  }
  const lowered = String(name).toLowerCase();
  return lowered.endsWith('_btn') ? lowered : `${lowered}_btn`;
}

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

// Context menu rows whose visible text isn't just the Squish name title-cased.
const CONTEXT_MENU_NAMES = {
  copy_as: 'Copy As',
  modify_atoms: 'Modify Atoms',
  modify_bonds: 'Modify Bonds',
  replace_atoms_with: 'Replace Atoms with',
  set_element: 'Set Element',
  wildcard: 'Wildcard',
  other_type: 'Other Type',
  single_up_down: 'Single Up/Down',
  double_cis_trans: 'Double Cis/Trans',
  zero_order: 'Zero Order',
  '+_charge': '+ Charge',
  '–_charge': '– Charge',
  add_explicit_hydrogens: 'Add Explicit Hydrogens',
  remove_explicit_hydrogens: 'Remove Explicit Hydrogens',
  add_unpaired_electron: 'Add Unpaired Electron',
  remove_unpaired_electron: 'Remove Unpaired Electron',
  A: 'A (Any heavy atom)',
  AH: 'AH (Any or H)',
  Q: 'Q (Heteroatom)',
  QH: 'QH (Hetero or H)',
  M: 'M (Metal)',
  MH: 'MH (Metal or H)',
  X: 'X (Halogen)',
  XH: 'XH (Halogen or H)',
};

/**
 * Squish refers to a row by a snake_case name; Qt matches on visible text.
 *
 * A name that isn't snake_case is already the visible text and is passed
 * through untouched, since title-casing it would corrupt labels that contain a
 * lowercase word, such as "Not In a Ring".
 */
function contextMenuLabel(name) {
  const mapped = CONTEXT_MENU_NAMES[name] || COPY_AS_NAMES[name];
  if (mapped) {
    return mapped;
  }
  if (!/^[a-z0-9_]+$/.test(name)) {
    return name;
  }
  return name.replace(/_/g, ' ').replace(/\b\w/g, (letter) => letter.toUpperCase());
}

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
    await clickWidget(this.page, widgetName(object_name));
    if (object_name === 'clear') {
      this.current_tool = null;
    }
  }

  /**
   * Equivalent to Squish `click_tool(tool, click_and_hold=False)`.
   *
   * A tool that isn't on the toolbar lives in a popup that only appears while
   * its owning button is held down, and is reached that way automatically --
   * see clickPopupTool. Pass `click_and_hold` to force the popup even for a
   * tool that is showing, which is how a test picks a specific shape from a
   * button that currently stands for a different one.
   */
  async click_tool(tool, click_and_hold = false) {
    // Squish tracks sticky tools and avoids re-clicking the current tool.
    // In particular, repeated rect_btn clicks interrupt Shift-add selection.
    if (this.current_tool === tool) {
      return;
    }
    const name = widgetName(tool);
    if (click_and_hold || !(await isWidgetVisible(this.page, name))) {
      await clickPopupTool(this.page, name);
    } else {
      await clickWidget(this.page, name);
    }
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

  /**
   * Equivalent to Squish `selection_context_menu(target, *actions)`.
   *
   * Right-clicks the target atom or bond, then walks the given rows. Unlike the
   * More Actions menu this is a real gesture throughout — see contextMenuAction.
   *
   * @param {{type: 'atom'|'bond', index: number}} target - Squish numbering
   * @param {...string} path - Squish row names, outermost first
   */
  async selection_context_menu(target, ...path) {
    await contextMenuAction(
      this.page,
      `${target.type}:${target.index - 1}`,
      ...path.map(contextMenuLabel),
    );
  }

  /**
   * Open a context menu and leave it showing, so a test can screenshot it.
   *
   * @param {{type: 'atom'|'bond', index: number}} target - Squish numbering
   * @param {...string} path - Squish row names to hover, outermost first
   */
  async open_selection_context_menu(target, ...path) {
    await openContextMenu(
      this.page,
      `${target.type}:${target.index - 1}`,
      ...path.map(contextMenuLabel),
    );
  }

  /** The Qt objectName Squish's name for a tool resolves to. */
  tool_widget_name(tool) {
    return widgetName(tool);
  }

  /** Equivalent to Squish `widget_state(object_name)`. */
  async widget_state(object_name) {
    return widgetState(this.page, widgetName(object_name));
  }

  /** Move the pointer to a point relative to the drawing area center. */
  async mouse_move(x, y) {
    const center = await getDrawingAreaCenter(this.page);
    await this.page.mouse.move(center.x + x, center.y + y, { steps: 4 });
  }

  /** Equivalent to Squish `click_sketcher(x, y)`, relative to the area center. */
  async click_sketcher(x, y) {
    const center = await getDrawingAreaCenter(this.page);
    await mouseClick(this.page, center.x + x, center.y + y);
  }

  /** Clear the canvas and forget the sticky tool, between phases of a test. */
  async reset_state() {
    await this.click_button('clear');
  }

  /** Equivalent to Squish `getClipboardText()` after Copy/Cut actions. */
  async clipboard_text() {
    return getClipboardText(this.page);
  }

  /**
   * Equivalent to Squish `type_text("sketcher_area", text)`.
   *
   * Squish writes a named key or chord in angle brackets, as in "<Ctrl+X>" or
   * "<Backspace>", and anything else literally.
   */
  async type_text(object_name, text) {
    if (object_name !== 'sketcher_area') {
      throw new Error(`Standalone type_text does not yet support: ${object_name}`);
    }
    await focusCanvas(this.page);
    const source = String(text);
    const bracketed = source.match(/^<(.+)>$/);
    if (bracketed) {
      await this.page.keyboard.press(bracketed[1].replace(/\bCtrl\b/g, 'Control'));
      return;
    }
    // Typed rather than pressed, so that a key like "+" isn't read as the
    // separator in a modifier chord.
    await this.page.keyboard.type(source);
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
   *
   * Squish numbers atoms and bonds from one, while the bridge takes the RDKit
   * model index, so the index is shifted here rather than in every ported test.
   */
  async click_item(type, index, select, modifier) {
    if (select) {
      await this.click_tool('rect_btn');
    }
    const rect = await requireRect(this.page, `${type}:${index - 1}`);
    await mouseClick(this.page, rect.x + rect.width / 2, rect.y + rect.height / 2, {
      modifiers: modifiersFor(modifier),
    });
  }

  /** Where an atom or bond is drawn right now, in page coordinates. */
  async rendered_object_rect(type, index) {
    return requireRect(this.page, `${type}:${index - 1}`);
  }

  /**
   * Record where several items are drawn before editing starts.
   *
   * Erasing an atom or bond renumbers every item after it, so a test that
   * deletes a series of them has to resolve all their positions up front and
   * then click the recorded points.
   */
  async capture_rendered_targets(type, indexes) {
    const targets = new Map();
    for (const index of indexes) {
      targets.set(index, await this.rendered_object_rect(type, index));
    }
    return targets;
  }

  /**
   * The center of a recorded rect, in page coordinates, optionally offset.
   *
   * The offset is how a test aims at something positioned relative to an item
   * rather than at the item itself, such as the rotation handle beside a
   * selection.
   */
  center_of(rect, dx = 0, dy = 0) {
    return { x: rect.x + rect.width / 2 + dx, y: rect.y + rect.height / 2 + dy };
  }

  /** Drag between two page points. */
  async drag(from, to, modifier = null) {
    await mouseDrag(this.page, from, to, { modifiers: modifiersFor(modifier) });
  }

  /** Click a point recorded by capture_rendered_targets or rendered_object_rect. */
  async click_rendered_target(rect, modifier = null) {
    const point = this.center_of(rect);
    await mouseClick(this.page, point.x, point.y, { modifiers: modifiersFor(modifier) });
  }

  /** Move the pointer over a point recorded by rendered_object_rect. */
  async hover_rendered_target(rect) {
    const point = this.center_of(rect);
    await this.page.mouse.move(point.x, point.y, { steps: 4 });
  }

  /** Double-click a point recorded by rendered_object_rect. */
  async double_click_rendered_target(rect) {
    const point = this.center_of(rect);
    await this.page.mouse.dblclick(point.x, point.y);
  }

  /** Drag from the center of one recorded point to the center of another. */
  async drag_between_rendered_targets(from, to, modifier = null) {
    await this.drag(this.center_of(from), this.center_of(to), modifier);
  }

  /** Drag by (dx, dy) from a recorded point. */
  async drag_from_rendered_target(rect, dx, dy, modifier = null) {
    await this.drag(this.center_of(rect), this.center_of(rect, dx, dy), modifier);
  }
}
