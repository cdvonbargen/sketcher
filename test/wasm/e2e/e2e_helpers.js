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
 * Click whatever a selector resolves to, with a real mouse event so that Qt's
 * own hit-testing runs.
 *
 * Waits for the target to become visible and enabled first, since Qt updates
 * both in response to model changes that may not have landed yet. Fails if it
 * never does: a real user couldn't have clicked a disabled control, and a test
 * that clicks one and then asserts nothing happened would pass whether or not
 * the control was actually unavailable. To assert unavailability, poll
 * `(await requireRect(page, selector)).enabled` instead.
 *
 * @param {import('@playwright/test').Page} page
 * @param {string} selector - see getRect
 * @param {object} [options] - forwarded to page.mouse.click (e.g. {button: 'right'})
 */
export async function click(page, selector, options) {
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
  await page.mouse.click(rect.x + rect.width / 2, rect.y + rect.height / 2, options);
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
 * Programmatically activate a control by its Qt objectName, or a menu action by
 * its objectName or visible text.
 *
 * This is for controls inside Qt::Popup windows only — popup menus and popup
 * widgets each get their own canvas in WASM, whose event listeners Playwright
 * can't target, so no rect from getRect can reach them. Use click() for
 * anything in the main canvas so that real mouse events are exercised.
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
