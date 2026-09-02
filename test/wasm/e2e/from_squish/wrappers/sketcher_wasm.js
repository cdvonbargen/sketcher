/**
 * Browser-facing Sketcher helpers used by the Squish-to-Playwright port.
 *
 * Nearly everything here is a thin re-export of ../../e2e_helpers.js. What
 * remains is either specific to reproducing a Squish workflow (importText) or
 * test-authoring tooling (the optional mouse marker).
 */
import {
  clickAction,
  clickWidget,
  focusCanvas,
  getDrawingAreaCenter,
  getRect,
  mouseClick as baseMouseClick,
  mouseDrag as baseMouseDrag,
  requireRect,
} from '../../e2e_helpers.js';

export {
  clickAction,
  clickItem,
  clickWidget,
  focusCanvas,
  hoverAction,
  isSketcherEmpty as isEmpty,
  requireRect,
  waitForSketcherReady as openSketcher,
} from '../../e2e_helpers.js';

/** Show a test-only cursor marker when PLAYWRIGHT_SHOW_MOUSE=1. */
async function showMouseMarker(page, x, y) {
  if (process.env.PLAYWRIGHT_SHOW_MOUSE !== '1') return;
  await page.evaluate(
    ({ left, top }) => {
      let marker = document.getElementById('playwright-mouse-marker');
      if (!marker) {
        marker = document.createElement('div');
        marker.id = 'playwright-mouse-marker';
        Object.assign(marker.style, {
          background: 'rgba(255, 45, 45, 0.28)',
          border: '2px solid #ff2d2d',
          borderRadius: '50%',
          boxSizing: 'border-box',
          height: '18px',
          left: '0',
          pointerEvents: 'none',
          position: 'fixed',
          top: '0',
          transform: 'translate(-50%, -50%)',
          width: '18px',
          zIndex: '2147483647',
        });
        document.body.append(marker);
      }
      marker.style.display = 'block';
      marker.style.left = `${left}px`;
      marker.style.top = `${top}px`;
    },
    { left: x, top: y },
  );
}

/** Hide the optional marker before a canvas visual checkpoint. */
export async function hideMouseMarker(page) {
  if (process.env.PLAYWRIGHT_SHOW_MOUSE !== '1') return;
  await page.evaluate(() => {
    const marker = document.getElementById('playwright-mouse-marker');
    if (marker) marker.style.display = 'none';
  });
}

/** mouseClick with the optional visible marker. */
export async function mouseClick(page, x, y, options) {
  await showMouseMarker(page, x, y);
  await baseMouseClick(page, x, y, options);
}

/** mouseDrag with the optional visible marker. */
export async function mouseDrag(page, start, end, options) {
  await showMouseMarker(page, start.x, start.y);
  await baseMouseDrag(page, start, end, options);
  await showMouseMarker(page, end.x, end.y);
}

/** Return a Qt widget's rectangle by its stable objectName. */
export async function widgetRect(page, objectName) {
  return requireRect(page, `widget:${objectName}`);
}

/** Return the center of the drawing area. */
export const drawingAreaCenter = getDrawingAreaCenter;

/** Click a visible text control and replace its value through keyboard input. */
export async function setWidgetText(page, objectName, text) {
  await clickWidget(page, objectName);
  await page.keyboard.press('ControlOrMeta+a');
  await page.keyboard.type(String(text), { delay: 10 });
}

export async function clipboardText(page) {
  return page.evaluate(async () => {
    if (!navigator.clipboard?.readText) {
      throw new Error('The browser Clipboard API is unavailable in this context.');
    }
    return navigator.clipboard.readText();
  });
}

export async function setClipboardText(page, text) {
  await page.evaluate((value) => navigator.clipboard.writeText(value), text);
}

/** @param {import('@playwright/test').Page} page */
export async function clearSketcher(page) {
  await clickWidget(page, 'clear_btn');
}

/**
 * Drive the real Import -> Paste in Text dialog, as the Squish suite does.
 * @param {import('@playwright/test').Page} page @param {string} text
 */
export async function importText(page, text) {
  await clickWidget(page, 'import_btn');
  await clickAction(page, 'Paste in Text...');
  await setWidgetText(page, 'structure_text_edit', text);

  // The generated Qt button box has no stable child object name for its OK
  // button. Its right half is the standard OK button in the standalone UI.
  const buttonBox = await widgetRect(page, 'buttonBox');
  await mouseClick(page, buttonBox.x + buttonBox.width * 0.75, buttonBox.y + buttonBox.height / 2);
}

/**
 * Load a structure as fixture setup, bypassing the import dialog.
 *
 * This is intentionally not an assertion of the user-facing Import flow.
 * Tests that cover Import must call importText() above; all other suites can
 * start from deterministic application state without introducing dialog
 * behavior as an unrelated dependency.
 */
export async function loadStructureForTest(page, text) {
  await page.evaluate((value) => Module.sketcher_import_text(value), text);
}

/** @param {import('@playwright/test').Page} page @param {string} format */
export async function exportText(page, format = 'SMILES') {
  return page.evaluate((name) => Module.sketcher_export_text(Module.Format[name]), format);
}

/** @param {import('@playwright/test').Page} page @param {string} element */
export async function drawElement(page, element) {
  const center = await getDrawingAreaCenter(page);
  await focusCanvas(page);
  await page.keyboard.press(element);
  await mouseClick(page, center.x, center.y);
}

/** @param {import('@playwright/test').Page} page */
export async function drawBond(page) {
  const center = await getDrawingAreaCenter(page);
  await mouseDrag(page, center, { x: center.x + 100, y: center.y });
}

/** Return the rendered rect of an atom or bond, or null if it isn't drawn yet. */
export async function renderedItemRect(page, type, index) {
  return getRect(page, `${type}:${index}`);
}
