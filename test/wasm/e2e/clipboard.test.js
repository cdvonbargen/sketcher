import { expect, test } from '@playwright/test';
import {
  waitForSketcherReady,
  focusCanvas,
  getClipboardText,
  getExportedSmiles,
  isSketcherEmpty,
  loadStructure,
  setClipboardText,
  clickWidget,
  activateAction,
} from './e2e_helpers.js';

// On WASM the sketcher bypasses QClipboard entirely: copy writes to
// navigator.clipboard directly and paste reads from it. These tests exercise
// that path end to end, using the browser's own clipboard API to set up and
// inspect the payload. That is also why the Playwright bridge has no C++
// clipboard binding — there is nothing for one to do that the page can't
// already do.
//
// Copy and cut go through real Ctrl+C / Ctrl+X presses, but paste cannot: Qt
// swallows Ctrl+V while it waits for the browser's own paste event, which
// Chromium will not raise for a scripted key press. Paste is driven through the
// Paste menu action instead, which reaches the same clipboard read.

test.beforeEach(async ({ page }) => {
  await waitForSketcherReady(page);
});

test.describe('Clipboard', () => {
  test('copy writes the structure to the system clipboard', async ({ page }) => {
    await loadStructure(page, 'CCO');
    await expect.poll(() => getExportedSmiles(page), { timeout: 5000 }).toBe('CCO');

    // With nothing selected, copy takes the whole scene. An atomistic structure
    // is copied as a V3000 molblock (see CutCopyActionManager), not SMILES.
    await focusCanvas(page);
    await page.keyboard.press('ControlOrMeta+c');

    await expect.poll(() => getClipboardText(page), { timeout: 5000 }).toContain('V30 BEGIN ATOM');
    // The oxygen proves it's this structure and not an empty CTAB
    expect(await getClipboardText(page)).toMatch(/V30 \d+ O /);
  });

  test('paste imports plain text from the system clipboard', async ({ page }) => {
    await setClipboardText(page, 'c1ccccc1');
    expect(await isSketcherEmpty(page)).toBe(true);

    await activateAction(page, 'Paste');

    await expect.poll(() => getExportedSmiles(page), { timeout: 5000 }).toBe('c1ccccc1');
  });

  test('copy then paste round-trips a structure through the clipboard', async ({ page }) => {
    await loadStructure(page, 'CC(=O)Nc1ccccc1');
    await expect.poll(() => isSketcherEmpty(page), { timeout: 5000 }).toBe(false);
    const before = await getExportedSmiles(page);

    await focusCanvas(page);
    await page.keyboard.press('ControlOrMeta+c');
    // Wait for the async clipboard write to land before clearing, otherwise the
    // paste below can race it
    await expect.poll(() => getClipboardText(page), { timeout: 5000 }).toContain('V30');

    await clickWidget(page, 'clear_btn');
    await expect.poll(() => isSketcherEmpty(page), { timeout: 5000 }).toBe(true);

    await activateAction(page, 'Paste');

    // Copy stashes a lossless RDKit pickle alongside the text, so pasting back
    // into the sketcher reproduces the structure exactly
    await expect.poll(() => getExportedSmiles(page), { timeout: 5000 }).toBe(before);
  });

  test('cut removes the structure and puts it on the clipboard', async ({ page }) => {
    await loadStructure(page, 'CCO');
    await expect.poll(() => getExportedSmiles(page), { timeout: 5000 }).toBe('CCO');

    // Unlike copy, cut is only enabled with an active selection
    await focusCanvas(page);
    await page.keyboard.press('ControlOrMeta+a');
    await page.keyboard.press('ControlOrMeta+x');

    await expect.poll(() => isSketcherEmpty(page), { timeout: 5000 }).toBe(true);
    expect(await getClipboardText(page)).toMatch(/V30 \d+ O /);
  });
});
