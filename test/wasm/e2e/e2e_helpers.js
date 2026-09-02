import { expect } from '@playwright/test';

/**
 * Navigate to the sketcher page and wait for the WASM module to be fully
 * loaded and the canvas to be visible.
 */
export async function waitForSketcherReady(page) {
  await page.goto('/wasm_shell.html');
  await page.waitForFunction(() => typeof window.Module !== 'undefined', {
    timeout: 20000,
  });
  // Wait for the canvas inside the shadow DOM to be attached and visible
  const canvas = page.locator('#screen canvas');
  await canvas.waitFor({ state: 'visible', timeout: 10000 });
}

/**
 * Focus the shadow DOM canvas so keyboard events reach Qt.
 */
export async function focusCanvas(page) {
  await page.locator('#screen canvas').focus();
}

/**
 * Return the {x, y} center of the canvas bounding box.
 * The center is well past the left toolbar (~90px wide), safely in the
 * drawing area.
 */
export async function getCanvasCenter(page) {
  const box = await page.locator('#screen canvas').boundingBox();
  return { x: Math.round(box.x + box.width / 2), y: Math.round(box.y + box.height / 2) };
}

/**
 * Resolve an object selector to {x, y, width, height, enabled} in page
 * coordinates, or null if nothing visible matches.
 *
 * This is the only way tests can locate anything inside the sketcher: Qt paints
 * the whole application into one canvas, so there are no DOM elements to query.
 * Once you have a rect, drive the application with ordinary Playwright mouse
 * and keyboard events.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} selector - "widget:<objectName>", "atom:<index>", or
 *   "bond:<index>"; indices are 0-based indices into the molecule
 */
export async function getRect(page, selector) {
  const rect = await page.evaluate((s) => {
    try {
      return JSON.parse(Module._sketcher_get_rect(s));
    } catch (e) {
      // A C++ exception reaches JS as an opaque emscripten value, so decode it
      // to keep the reason in the test failure
      throw new Error(Module.getExceptionMessage(e).join(': '));
    }
  }, selector);
  return rect && rect.width !== undefined ? rect : null;
}

/**
 * Return {x, y, width, height, enabled} for a selector, failing if nothing
 * visible matches.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} selector - see getRect
 */
export async function requireRect(page, selector) {
  const rect = await getRect(page, selector);
  if (rect === null) {
    throw new Error(`Nothing visible matches "${selector}"`);
  }
  return rect;
}

/**
 * Return the {x, y} center of the drawing area (excluding toolbar and top bar).
 * @param {import('@playwright/test').Page} page
 */
export async function getDrawingAreaCenter(page) {
  const rect = await requireRect(page, 'widget:view');
  return { x: Math.round(rect.x + rect.width / 2), y: Math.round(rect.y + rect.height / 2) };
}

/**
 * Return the current molecule as a SMILES string.
 */
export async function getExportedSmiles(page) {
  return page.evaluate(() => Module.sketcher_export_text(Module.Format.SMILES));
}

/**
 * Return the current molecule as a HELM string.
 */
export async function getExportedHelm(page) {
  return page.evaluate(() => Module.sketcher_export_text(Module.Format.HELM));
}

/**
 * Select all items on the canvas via Cmd+A / Ctrl+A.
 */
export async function selectAll(page) {
  await focusCanvas(page);
  await page.keyboard.press('ControlOrMeta+a');
}

/**
 * Return whether the sketcher is currently empty.
 */
export async function isSketcherEmpty(page) {
  return page.evaluate(() => Module.sketcher_is_empty());
}

/**
 * Press and release a mouse button at a page coordinate, optionally with
 * keyboard modifiers held down.
 *
 * Playwright's page.mouse.click() takes no modifiers, and Qt wants to see the
 * pointer arrive before the press, so this moves first and holds the modifiers
 * around the whole gesture.
 *
 * @param {import('@playwright/test').Page} page
 * @param {number} x
 * @param {number} y
 * @param {object} [options]
 * @param {'left'|'right'|'middle'} [options.button]
 * @param {string[]} [options.modifiers] - e.g. ['Shift']
 */
export async function mouseClick(page, x, y, { button = 'left', modifiers = [] } = {}) {
  for (const modifier of modifiers) {
    await page.keyboard.down(modifier);
  }
  try {
    await page.mouse.move(x, y, { steps: 4 });
    await page.mouse.down({ button });
    await page.mouse.up({ button });
  } finally {
    for (const modifier of [...modifiers].reverse()) {
      await page.keyboard.up(modifier);
    }
  }
}

/**
 * Drag from one page coordinate to another with intermediate moves, so that Qt
 * sees a real drag rather than a teleport.
 *
 * @param {import('@playwright/test').Page} page
 * @param {{x: number, y: number}} start
 * @param {{x: number, y: number}} end
 * @param {object} [options]
 * @param {'left'|'right'|'middle'} [options.button]
 * @param {string[]} [options.modifiers]
 * @param {number} [options.steps]
 */
export async function mouseDrag(
  page,
  start,
  end,
  { button = 'left', modifiers = [], steps = 12 } = {},
) {
  for (const modifier of modifiers) {
    await page.keyboard.down(modifier);
  }
  try {
    await page.mouse.move(start.x, start.y, { steps: 4 });
    await page.mouse.down({ button });
    for (let step = 1; step <= steps; step += 1) {
      const progress = step / steps;
      await page.mouse.move(
        start.x + (end.x - start.x) * progress,
        start.y + (end.y - start.y) * progress,
      );
    }
    await page.mouse.up({ button });
  } finally {
    for (const modifier of [...modifiers].reverse()) {
      await page.keyboard.up(modifier);
    }
  }
}

/**
 * Wait for a selector to resolve to a visible, enabled rect and return it.
 *
 * Qt updates visibility and enabled state in response to model changes that may
 * not have landed yet, and a menu popup is laid out an event-loop turn after
 * the click that opens it. Failing on disabled is deliberate: a real user
 * couldn't have clicked it, and a test that clicks a disabled control then
 * asserts nothing happened would pass either way. To assert unavailability,
 * poll `(await requireRect(page, selector)).enabled` instead.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} selector - see getRect
 */
export async function waitForClickable(page, selector) {
  let rect;
  await expect
    .poll(
      async () => {
        rect = await getRect(page, selector);
        if (rect === null) {
          return 'not visible';
        }
        return rect.enabled ? 'clickable' : 'disabled';
      },
      { timeout: 5000, message: `"${selector}"` },
    )
    .toBe('clickable');
  return rect;
}

/**
 * Click whatever a selector resolves to, with a real mouse event so that Qt's
 * own hit-testing runs.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} selector - see getRect
 * @param {object} [options] - see mouseClick
 */
export async function click(page, selector, options) {
  const rect = await waitForClickable(page, selector);
  await mouseClick(page, rect.x + rect.width / 2, rect.y + rect.height / 2, options);
}

/**
 * Move the pointer over whatever a selector resolves to. Used to open a
 * cascading submenu the way a user does.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} selector - see getRect
 */
export async function hover(page, selector) {
  const rect = await waitForClickable(page, selector);
  await page.mouse.move(rect.x + rect.width / 2, rect.y + rect.height / 2, { steps: 4 });
}

/**
 * Click a toolbar button by its Qt objectName (e.g. "c_btn").
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} name - Qt objectName of the button
 */
export async function clickWidget(page, name) {
  await click(page, `widget:${name}`);
}

/**
 * Click an atom or bond by its index in the molecule.
 *
 * @param {import('@playwright/test').Page} page
 * @param {'atom'|'bond'} kind - monomers are addressed as 'atom' and monomer
 *   connectors as 'bond'
 * @param {number} index - 0-based index into the molecule's atoms or bonds
 * @param {object} [options] - forwarded to page.mouse.click (e.g. {button: 'right'})
 */
export async function clickItem(page, kind, index, options) {
  await click(page, `${kind}:${index}`, options);
}

/**
 * Click a row of a currently-open menu by its objectName or visible text.
 *
 * The menu must already be open — an action has no geometry until its menu is
 * laid out. click() polls, so this tolerates the event-loop turn Qt takes to
 * show the popup after the button that opens it is clicked.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} nameOrText - objectName, or the row's visible text
 */
export async function clickAction(page, nameOrText) {
  await click(page, `action:${nameOrText}`);
}

/**
 * Hover a row of a currently-open menu to open its submenu.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} nameOrText - objectName, or the row's visible text
 */
export async function hoverAction(page, nameOrText) {
  await hover(page, `action:${nameOrText}`);
}

/**
 * Programmatically activate a control by its Qt objectName, or a menu action by
 * its objectName or visible text.
 *
 * Prefer click() with a selector wherever possible, so that Qt's own hit-testing
 * runs. This is the fallback for controls that can't be resolved to a rect at
 * all — most usefully an action on a menu that hasn't been opened, since
 * actions only have geometry while their menu is laid out.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} nameOrText - Qt objectName, or the action's visible text
 * @throws if nothing matches, or if the match is disabled
 */
export async function activate(page, nameOrText) {
  await page.evaluate((n) => {
    try {
      Module._sketcher_activate(n);
    } catch (e) {
      // A C++ exception reaches JS as an opaque emscripten value, so decode it
      // to keep the reason in the test failure
      throw new Error(Module.getExceptionMessage(e).join(': '));
    }
  }, nameOrText);
}
