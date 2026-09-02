import { expect, test } from '@playwright/test';
import {
  clickAction,
  clickWidget,
  getExportedSmiles,
  isSketcherEmpty,
  mouseClick,
  requireRect,
  setWidgetText,
  waitForSketcherReady,
} from './e2e_helpers.js';

/**
 * Drive the real Import -> Paste in Text dialog, as the Squish suite does.
 *
 * This lives here rather than in e2e_helpers.js because these are the only
 * tests that should use it: every other suite establishes its structure with
 * loadStructure() so that the dialog isn't an unrelated dependency.
 */
async function importTextViaDialog(page, text) {
  await clickWidget(page, 'import_btn');
  await clickAction(page, 'Paste in Text...');
  await setWidgetText(page, 'structure_text_edit', text);

  // The generated Qt button box has no stable child object name for its OK
  // button. Its right half is the standard OK button in the standalone UI.
  const buttonBox = await requireRect(page, 'widget:buttonBox');
  await mouseClick(page, buttonBox.x + buttonBox.width * 0.75, buttonBox.y + buttonBox.height / 2);
}

test.beforeEach(async ({ page }) => {
  await waitForSketcherReady(page);
});

test.describe.skip('ported Squish import and export', () => {
  // Re-enable after the standalone WASM import/export dialogs have stable
  // browser-side geometry for real click-and-type interaction. These tests
  // must not use the fixture loader because they cover the UI flow itself.
  // Source coverage: tst_import_menu, tst_export_menu, and tst_miscellaneous.
  test('imports SMILES and exports the current structure', async ({ page }) => {
    await importTextViaDialog(page, 'C=O');
    await expect.poll(() => getExportedSmiles(page)).toBe('C=O');
  });

  test('imports a molfile and exports SMILES', async ({ page }) => {
    const molfile = `\n  Sketcher          2D\n\n  2  1  0  0  0  0  0  0  0999 V2000\n    0.0000    0.0000    0.0000 C   0  0  0  0  0  0  0  0  0  0  0  0\n    1.5000    0.0000    0.0000 O   0  0  0  0  0  0  0  0  0  0  0  0\n  1  2  2  0\nM  END`;
    await importTextViaDialog(page, molfile);
    await expect.poll(() => getExportedSmiles(page)).toBe('C=O');
  });

  test('clear removes imported content', async ({ page }) => {
    await importTextViaDialog(page, 'c1ccccc1');
    await expect.poll(() => isSketcherEmpty(page)).toBe(false);
    await clickWidget(page, 'clear_btn');
    await expect.poll(() => isSketcherEmpty(page)).toBe(true);
  });

  test('exports SVG image data', async ({ page }) => {
    await importTextViaDialog(page, 'C=O');
    const svg = await page.evaluate(() =>
      atob(Module.sketcher_export_image(Module.ImageFormat.SVG)),
    );
    expect(svg).toContain('<svg');
  });
});
